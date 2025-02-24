// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/client_controlled_state.h"

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/window_state_type.h"
#include "ash/root_window_controller.h"
#include "ash/screen_util.h"
#include "ash/shell.h"
#include "ash/wm/screen_pinning_controller.h"
#include "ash/wm/window_animation_types.h"
#include "ash/wm/window_parenting_utils.h"
#include "ash/wm/window_positioning_utils.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_state_delegate.h"
#include "ash/wm/window_state_util.h"
#include "ash/wm/wm_event.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/wm/core/window_util.h"

namespace ash {
namespace wm {

ClientControlledState::ClientControlledState(std::unique_ptr<Delegate> delegate)
    : BaseState(mojom::WindowStateType::DEFAULT),
      delegate_(std::move(delegate)) {}

ClientControlledState::~ClientControlledState() {}

void ClientControlledState::HandleTransitionEvents(WindowState* window_state,
                                                   const WMEvent* event) {
  bool pin_transition = window_state->IsTrustedPinned() ||
                        window_state->IsPinned() || event->IsPinEvent();
  // Pinned State transition is handled on server side.
  if (pin_transition) {
    // Only one window can be pinned.
    if (event->IsPinEvent() &&
        Shell::Get()->screen_pinning_controller()->IsPinned()) {
      return;
    }
    mojom::WindowStateType next_state_type = GetStateForTransitionEvent(event);
    delegate_->HandleWindowStateRequest(state_type_, next_state_type);
    mojom::WindowStateType old_state_type = state_type_;

    bool was_pinned = window_state->IsPinned();
    bool was_trusted_pinned = window_state->IsTrustedPinned();

    EnterNextState(window_state, next_state_type);

    VLOG(1) << "Processing Pinned Transtion: event=" << event->type()
            << ", state=" << old_state_type << "=>" << next_state_type
            << ", pinned=" << was_pinned << "=>" << window_state->IsPinned()
            << ", trusted pinned=" << was_trusted_pinned << "=>"
            << window_state->IsTrustedPinned();
    return;
  }

  switch (event->type()) {
    case WM_EVENT_NORMAL:
    case WM_EVENT_MAXIMIZE:
    case WM_EVENT_MINIMIZE:
    case WM_EVENT_FULLSCREEN:
    case WM_EVENT_SNAP_LEFT:
    case WM_EVENT_SNAP_RIGHT: {
      // Reset window state
      window_state->UpdateWindowPropertiesFromStateType();
      mojom::WindowStateType next_state = GetStateForTransitionEvent(event);
      VLOG(1) << "Processing State Transtion: event=" << event->type()
              << ", state=" << state_type_ << ", next_state=" << next_state;
      // Then ask delegate to handle the window state change.
      delegate_->HandleWindowStateRequest(state_type_, next_state);
      break;
    }
    case WM_EVENT_SHOW_INACTIVE:
      NOTREACHED();
      break;
    default:
      NOTREACHED() << "Unknown event :" << event->type();
  }
}

void ClientControlledState::AttachState(
    WindowState* window_state,
    WindowState::State* state_in_previous_mode) {}

void ClientControlledState::DetachState(WindowState* window_state) {}

void ClientControlledState::HandleWorkspaceEvents(WindowState* window_state,
                                                  const WMEvent* event) {
  // Client is responsible for adjusting bounds after workspace bounds change.
}

void ClientControlledState::HandleCompoundEvents(WindowState* window_state,
                                                 const WMEvent* event) {
  switch (event->type()) {
    case WM_EVENT_TOGGLE_MAXIMIZE_CAPTION:
      if (window_state->IsFullscreen()) {
        const wm::WMEvent event(wm::WM_EVENT_TOGGLE_FULLSCREEN);
        window_state->OnWMEvent(&event);
      } else if (window_state->IsMaximized()) {
        window_state->Restore();
      } else if (window_state->IsNormalOrSnapped()) {
        if (window_state->CanMaximize())
          window_state->Maximize();
      }
      break;
    case WM_EVENT_TOGGLE_MAXIMIZE:
      if (window_state->IsFullscreen()) {
        const wm::WMEvent event(wm::WM_EVENT_TOGGLE_FULLSCREEN);
        window_state->OnWMEvent(&event);
      } else if (window_state->IsMaximized()) {
        window_state->Restore();
      } else if (window_state->CanMaximize()) {
        window_state->Maximize();
      }
      break;
    case WM_EVENT_TOGGLE_VERTICAL_MAXIMIZE:
      // TODO(oshima): Implement this.
      break;
    case WM_EVENT_TOGGLE_HORIZONTAL_MAXIMIZE:
      // TODO(oshima): Implement this.
      break;
    case WM_EVENT_TOGGLE_FULLSCREEN:
      ToggleFullScreen(window_state, window_state->delegate());
      break;
    case WM_EVENT_CYCLE_SNAP_LEFT:
    case WM_EVENT_CYCLE_SNAP_RIGHT:
      // TODO(oshima): implement this.
      break;
    default:
      NOTREACHED() << "Invalid event :" << event->type();
      break;
  }
}

void ClientControlledState::HandleBoundsEvents(WindowState* window_state,
                                               const WMEvent* event) {
  switch (event->type()) {
    case WM_EVENT_SET_BOUNDS: {
      // TODO(oshima): Send the set bounds request to client.
      const SetBoundsEvent* set_bounds_event =
          static_cast<const SetBoundsEvent*>(event);
      if (set_bounds_locally_) {
        window_state->SetBoundsDirect(set_bounds_event->requested_bounds());
      } else if (window_state->IsPinned() || window_state->IsTrustedPinned()) {
        // In pinned state, it should ignore the SetBounds from window manager
        // or user.
      } else {
        delegate_->HandleBoundsRequest(window_state->GetStateType(),
                                       set_bounds_event->requested_bounds());
      }
      break;
    }
    case WM_EVENT_CENTER:
      // TODO(oshima): implement this.
      break;
    default:
      NOTREACHED() << "Unknown event:" << event->type();
  }
}

bool ClientControlledState::EnterNextState(
    WindowState* window_state,
    mojom::WindowStateType next_state_type) {
  // Do nothing if  we're already in the same state.
  if (state_type_ == next_state_type)
    return false;

  mojom::WindowStateType previous_state_type = state_type_;
  state_type_ = next_state_type;

  window_state->UpdateWindowPropertiesFromStateType();
  window_state->NotifyPreStateTypeChange(previous_state_type);

  // Don't update the window if the window is detached from parent.
  // This can happen during dragging.
  // TODO(oshima): This was added for DOCKED windows. Investigate if
  // we still need this.
  if (window_state->window()->parent())
    UpdateMinimizedState(window_state, previous_state_type);

  window_state->NotifyPostStateTypeChange(previous_state_type);

  if (next_state_type == mojom::WindowStateType::PINNED ||
      previous_state_type == mojom::WindowStateType::PINNED ||
      next_state_type == mojom::WindowStateType::TRUSTED_PINNED ||
      previous_state_type == mojom::WindowStateType::TRUSTED_PINNED) {
    Shell::Get()->screen_pinning_controller()->SetPinnedWindow(
        window_state->window());
  }
  return true;
}

}  // namespace wm
}  // namespace ash
