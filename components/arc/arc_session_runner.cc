// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/arc_session_runner.h"

#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/optional.h"
#include "base/task_runner.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "components/arc/arc_util.h"

namespace arc {

namespace {

constexpr base::TimeDelta kDefaultRestartDelay =
    base::TimeDelta::FromSeconds(5);

chromeos::SessionManagerClient* GetSessionManagerClient() {
  // If the DBusThreadManager or the SessionManagerClient aren't available,
  // there isn't much we can do. This should only happen when running tests.
  if (!chromeos::DBusThreadManager::IsInitialized() ||
      !chromeos::DBusThreadManager::Get() ||
      !chromeos::DBusThreadManager::Get()->GetSessionManagerClient())
    return nullptr;
  return chromeos::DBusThreadManager::Get()->GetSessionManagerClient();
}

void RecordInstanceCrashUma(ArcContainerLifetimeEvent sample) {
  UMA_HISTOGRAM_ENUMERATION("Arc.ContainerLifetimeEvent", sample,
                            ArcContainerLifetimeEvent::COUNT);
}

void RecordInstanceRestartAfterCrashUma(size_t restart_after_crash_count) {
  UMA_HISTOGRAM_COUNTS_100("Arc.ContainerRestartAfterCrashCount",
                           restart_after_crash_count);
}

// Gets an ArcContainerLifetimeEvent value to record. Returns nullopt when no
// UMA recording is needed.
base::Optional<ArcContainerLifetimeEvent> GetArcContainerLifetimeEvent(
    size_t restart_after_crash_count,
    ArcStopReason stop_reason,
    bool was_running) {
  // Record UMA only when this is the first non-early crash. This has to be
  // done before checking other conditions. Otherwise, an early crash after
  // container restart might be recorded. Each CONTAINER_STARTED event can
  // be paired up to one non-START event.
  if (restart_after_crash_count)
    return base::nullopt;

  switch (stop_reason) {
    case ArcStopReason::SHUTDOWN:
    case ArcStopReason::LOW_DISK_SPACE:
      // We don't record these events.
      return base::nullopt;
    case ArcStopReason::GENERIC_BOOT_FAILURE:
      return ArcContainerLifetimeEvent::CONTAINER_FAILED_TO_START;
    case ArcStopReason::CRASH:
      return was_running ? ArcContainerLifetimeEvent::CONTAINER_CRASHED
                         : ArcContainerLifetimeEvent::CONTAINER_CRASHED_EARLY;
  }

  NOTREACHED();
  return base::nullopt;
}

// Returns true if restart is needed for given conditions.
bool IsRestartNeeded(base::Optional<ArcInstanceMode> target_mode,
                     ArcStopReason stop_reason,
                     bool was_running) {
  if (!target_mode.has_value()) {
    // The request to run ARC is canceled by the caller. No need to restart.
    return false;
  }

  switch (stop_reason) {
    case ArcStopReason::SHUTDOWN:
      // This is a part of stop requested by ArcSessionRunner.
      // If ARC is re-requested to start, restart is necessary.
      // This case happens, e.g., RequestStart() -> RequestStop() ->
      // RequestStart(), case. If the second RequestStart() is called before
      // the instance previously running is stopped, then just |target_mode_|
      // is set. On completion, restart is needed.
      return true;
    case ArcStopReason::GENERIC_BOOT_FAILURE:
    case ArcStopReason::LOW_DISK_SPACE:
      // These two are errors on starting. To prevent failure loop, do not
      // restart.
      return false;
    case ArcStopReason::CRASH:
      // ARC instance is crashed unexpectedly, so automatically restart.
      // However, to avoid crash loop, do not restart if it is not successfully
      // started yet. So, check |was_running|.
      return was_running;
  }

  NOTREACHED();
  return false;
}

// Returns true if the request to start/upgrade ARC instance is allowed
// operation.
bool IsRequestAllowed(const base::Optional<ArcInstanceMode>& current_mode,
                      ArcInstanceMode request_mode) {
  if (request_mode == ArcInstanceMode::MINI_INSTANCE &&
      ShouldArcOnlyStartAfterLogin()) {
    // Skip starting ARC for now. We'll have another chance to start the full
    // instance after the user logs in.
    return false;
  }

  if (!current_mode.has_value()) {
    // This is a request to start a new ARC instance (either mini instance
    // or full instance).
    return true;
  }

  if (current_mode == ArcInstanceMode::MINI_INSTANCE &&
      request_mode == ArcInstanceMode::FULL_INSTANCE) {
    // This is a request to upgrade the running mini instance to full instance.
    return true;
  }

  // Otherwise, not allowed.
  LOG(ERROR) << "Unexpected ARC instance mode transition request: "
             << current_mode << " -> " << request_mode;
  return false;
}

// Returns true if OnSessionStopped() should be called to notify observers.
bool ShouldNotifyOnSessionStopped(
    const base::Optional<ArcInstanceMode>& target_mode) {
  DCHECK(target_mode.has_value());

  switch (target_mode.value()) {
    case ArcInstanceMode::MINI_INSTANCE:
      return false;
    case ArcInstanceMode::FULL_INSTANCE:
      return true;
  }

  NOTREACHED() << "Unexpeceted |target_mode|: "
               << static_cast<int>(target_mode.value());
  return false;
}

}  // namespace

ArcSessionRunner::ArcSessionRunner(const ArcSessionFactory& factory)
    : restart_delay_(kDefaultRestartDelay),
      restart_after_crash_count_(0),
      factory_(factory),
      weak_ptr_factory_(this) {
  chromeos::SessionManagerClient* client = GetSessionManagerClient();
  if (client)
    client->AddObserver(this);
}

ArcSessionRunner::~ArcSessionRunner() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (arc_session_)
    arc_session_->RemoveObserver(this);
  chromeos::SessionManagerClient* client = GetSessionManagerClient();
  if (client)
    client->RemoveObserver(this);
}

void ArcSessionRunner::AddObserver(Observer* observer) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  observer_list_.AddObserver(observer);
}

void ArcSessionRunner::RemoveObserver(Observer* observer) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  observer_list_.RemoveObserver(observer);
}

void ArcSessionRunner::RequestStart(ArcInstanceMode request_mode) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (target_mode_ == request_mode) {
    // Consecutive RequestStart() call for the same mode. Do nothing.
    return;
  }

  if (!IsRequestAllowed(target_mode_, request_mode))
    return;

  VLOG(1) << "Session start requested: " << request_mode;
  target_mode_ = request_mode;
  if (arc_session_ && arc_session_->IsStopRequested()) {
    // This is the case where RequestStop() was called, but before
    // |arc_session_| had finshed stopping, RequestStart() is called.
    // Do nothing in the that case, since when |arc_session_| does actually
    // stop, OnSessionStopped() will be called, where it should automatically
    // restart.
    return;
  }

  if (restart_timer_.IsRunning()) {
    // |restart_timer_| may be running if this is upgrade request in a
    // following scenario.
    // - RequestStart(MINI_INSTANCE)
    // - RequestStop()
    // - RequestStart(MINI_INSTANCE)
    // - OnSessionStopped()
    // - RequestStart(FULL_INSTANCE) before RestartArcSession() is called.
    // In such a case, defer the operation to RestartArcSession() called later.
    return;
  }

  // No asynchronous event is expected later. Trigger the ArcSession now.
  StartArcSession();
}

void ArcSessionRunner::RequestStop() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  VLOG(1) << "Session stop requested";
  target_mode_ = base::nullopt;

  if (arc_session_) {
    // If |arc_session_| is running, stop it.
    // Note that |arc_session_| may be already in the process of stopping or
    // be stopped.
    // E.g. RequestStart() -> RequestStop() -> RequestStart() -> RequestStop()
    // case. If the second RequestStop() is called before the first
    // RequestStop() is not yet completed for the instance, Stop() of the
    // instance is called again, but it is no-op as expected.
    arc_session_->Stop();
  }

  // In case restarting is in progress, cancel it.
  restart_timer_.Stop();
}

void ArcSessionRunner::OnShutdown() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  VLOG(1) << "OnShutdown";
  target_mode_ = base::nullopt;
  restart_timer_.Stop();
  if (arc_session_)
    arc_session_->OnShutdown();
  // ArcSession::OnShutdown() invokes OnSessionStopped() synchronously.
  // In the observer method, |arc_session_| should be destroyed.
  DCHECK(!arc_session_);
}

void ArcSessionRunner::SetRestartDelayForTesting(
    const base::TimeDelta& restart_delay) {
  DCHECK(!arc_session_);
  DCHECK(!restart_timer_.IsRunning());
  restart_delay_ = restart_delay;
}

void ArcSessionRunner::StartArcSession() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!restart_timer_.IsRunning());
  DCHECK(target_mode_.has_value());

  VLOG(1) << "Starting ARC instance";
  if (!arc_session_) {
    arc_session_ = factory_.Run();
    arc_session_->AddObserver(this);
    // Record the UMA only when |restart_after_crash_count_| is zero to avoid
    // recording an auto-restart-then-crash loop. Such a crash loop is recorded
    // separately with RecordInstanceRestartAfterCrashUma().
    if (!restart_after_crash_count_)
      RecordInstanceCrashUma(ArcContainerLifetimeEvent::CONTAINER_STARTING);
  } else {
    DCHECK_EQ(ArcInstanceMode::MINI_INSTANCE, arc_session_->GetTargetMode());
  }
  arc_session_->Start(target_mode_.value());
}

void ArcSessionRunner::RestartArcSession() {
  VLOG(0) << "Restarting ARC instance";
  // The order is important here. Call StartArcSession(), then notify observers.
  StartArcSession();
  for (auto& observer : observer_list_)
    observer.OnSessionRestarting();
}

void ArcSessionRunner::OnSessionStopped(ArcStopReason stop_reason,
                                        bool was_running) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(arc_session_);
  DCHECK(!restart_timer_.IsRunning());

  VLOG(0) << "ARC stopped: " << stop_reason;

  // The observers should be agnostic to the existence of the limited-purpose
  // instance.
  const bool notify_observers =
      ShouldNotifyOnSessionStopped(arc_session_->GetTargetMode());

  arc_session_->RemoveObserver(this);
  arc_session_.reset();

  const base::Optional<ArcContainerLifetimeEvent> uma_to_record =
      GetArcContainerLifetimeEvent(restart_after_crash_count_, stop_reason,
                                   was_running);
  if (uma_to_record.has_value())
    RecordInstanceCrashUma(uma_to_record.value());

  const bool restarting =
      IsRestartNeeded(target_mode_, stop_reason, was_running);

  if (restarting && stop_reason == ArcStopReason::CRASH) {
    ++restart_after_crash_count_;
  } else {
    // The session ended. Record the restart count.
    RecordInstanceRestartAfterCrashUma(restart_after_crash_count_);
    restart_after_crash_count_ = 0;
  }

  if (restarting) {
    // There was a previous invocation and it crashed for some reason. Try
    // starting ARC instance later again.
    // Note that even |restart_delay_| is 0 (for testing), it needs to
    // PostTask, because observer callback may call RequestStart()/Stop().
    VLOG(0) << "ARC restarting";
    restart_timer_.Start(FROM_HERE, restart_delay_,
                         base::Bind(&ArcSessionRunner::RestartArcSession,
                                    weak_ptr_factory_.GetWeakPtr()));
  }

  if (notify_observers) {
    for (auto& observer : observer_list_)
      observer.OnSessionStopped(stop_reason, restarting);
  }
}

void ArcSessionRunner::EmitLoginPromptVisibleCalled() {
  // Since 'login-prompt-visible' Upstart signal starts all Upstart jobs the
  // instance may depend on such as cras, EmitLoginPromptVisibleCalled() is the
  // safe place to start a mini instance.
  DCHECK(!arc_session_);
  RequestStart(ArcInstanceMode::MINI_INSTANCE);
}

}  // namespace arc
