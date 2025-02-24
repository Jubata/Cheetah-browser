// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/core/v8/ScriptPromiseResolver.h"

#include "core/dom/ExecutionContext.h"
#include "core/probe/CoreProbes.h"
#include "public/platform/TaskType.h"

namespace blink {

ScriptPromiseResolver::ScriptPromiseResolver(ScriptState* script_state)
    : PausableObject(ExecutionContext::From(script_state)),
      state_(kPending),
      script_state_(script_state),
      timer_(GetExecutionContext()->GetTaskRunner(TaskType::kMicrotask),
             this,
             &ScriptPromiseResolver::OnTimerFired),
      resolver_(script_state) {
  if (GetExecutionContext()->IsContextDestroyed()) {
    state_ = kDetached;
    resolver_.Clear();
  }
  probe::AsyncTaskScheduled(GetExecutionContext(), "Promise", this);
}

void ScriptPromiseResolver::Pause() {
  timer_.Stop();
}

void ScriptPromiseResolver::Unpause() {
  if (state_ == kResolving || state_ == kRejecting)
    timer_.StartOneShot(0, BLINK_FROM_HERE);
}

void ScriptPromiseResolver::Detach() {
  if (state_ == kDetached)
    return;
  timer_.Stop();
  state_ = kDetached;
  resolver_.Clear();
  value_.Clear();
  keep_alive_.Clear();
  probe::AsyncTaskCanceled(GetExecutionContext(), this);
}

void ScriptPromiseResolver::KeepAliveWhilePending() {
  // keepAliveWhilePending() will be called twice if the resolver
  // is created in a suspended execution context and the resolver
  // is then resolved/rejected while in that suspended state.
  if (state_ == kDetached || keep_alive_)
    return;

  // Keep |this| around while the promise is Pending;
  // see detach() for the dual operation.
  keep_alive_ = this;
}

void ScriptPromiseResolver::OnTimerFired(TimerBase*) {
  DCHECK(state_ == kResolving || state_ == kRejecting);
  if (!GetScriptState()->ContextIsValid()) {
    Detach();
    return;
  }

  ScriptState::Scope scope(script_state_.get());
  ResolveOrRejectImmediately();
}

void ScriptPromiseResolver::ResolveOrRejectImmediately() {
  DCHECK(!GetExecutionContext()->IsContextDestroyed());
  DCHECK(!GetExecutionContext()->IsContextPaused());
  {
    probe::AsyncTask async_task(GetExecutionContext(), this);
    if (state_ == kResolving) {
      resolver_.Resolve(value_.NewLocal(script_state_->GetIsolate()));
    } else {
      DCHECK_EQ(state_, kRejecting);
      resolver_.Reject(value_.NewLocal(script_state_->GetIsolate()));
    }
  }
  Detach();
}

void ScriptPromiseResolver::Trace(blink::Visitor* visitor) {
  PausableObject::Trace(visitor);
}

}  // namespace blink
