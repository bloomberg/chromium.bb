// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/ws/drag_controller.h"

#include "base/logging.h"
#include "services/ui/ws/drag_source.h"
#include "services/ui/ws/drag_target_connection.h"
#include "services/ui/ws/event_dispatcher.h"
#include "services/ui/ws/server_window.h"

namespace ui {
namespace ws {

struct DragController::Operation {
  OperationType type;
  uint32_t event_flags;
  gfx::Point screen_position;
};

struct DragController::WindowState {
  // Set to true once we've observed the ServerWindow* that is the key to this
  // instance in |window_state_|.
  bool observed;

  // If we're waiting for a response, this is the type of message. TYPE_NONE
  // means there's no outstanding
  OperationType waiting_on_reply;

  // The operation that we'll send off if |waiting_on_reply| isn't TYPE_NONE.
  Operation queued_operation;
};

DragController::DragController(
    DragSource* source,
    ServerWindow* source_window,
    DragTargetConnection* source_connection,
    int32_t drag_pointer,
    mojo::Map<mojo::String, mojo::Array<uint8_t>> mime_data,
    uint32_t drag_operations)
    : source_(source),
      drag_operations_(drag_operations),
      drag_pointer_id_(drag_pointer),
      source_window_(source_window),
      source_connection_(source_connection),
      mime_data_(std::move(mime_data)),
      weak_factory_(this) {
  EnsureWindowObserved(source_window_);
}

DragController::~DragController() {
  for (auto& pair : window_state_) {
    if (pair.second.observed)
      pair.first->RemoveObserver(this);
  }
}

void DragController::Cancel() {
  MessageDragCompleted(false);
  // |this| may be deleted now.
}

bool DragController::DispatchPointerEvent(const ui::PointerEvent& event,
                                          ServerWindow* current_target) {
  uint32_t event_flags =
      event.flags() &
      (ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN);
  gfx::Point screen_position = event.location();

  if (waiting_for_final_drop_response_) {
    // If we're waiting on a target window to respond to the final drag drop
    // call, don't process any more pointer events.
    return false;
  }

  if (event.pointer_id() != drag_pointer_id_)
    return false;

  // If |current_target| doesn't accept drags, walk its hierarchy up until we
  // find one that does (or set to nullptr at the top of the tree).
  while (current_target && !current_target->can_accept_drops())
    current_target = current_target->parent();

  if (current_target) {
    // If we're non-null, we're about to use |current_target| in some
    // way. Ensure that we receive notifications that this window has gone
    // away.
    EnsureWindowObserved(current_target);
  }

  if (current_target && current_target == current_target_window_ &&
      event.type() != ET_POINTER_UP) {
    QueueOperation(current_target, OperationType::OVER, event_flags,
                   screen_position);
  } else if (current_target != current_target_window_) {
    if (current_target_window_) {
      QueueOperation(current_target_window_, OperationType::LEAVE, event_flags,
                     screen_position);
    }

    if (current_target) {
      // TODO(erg): If we have a queued LEAVE operation, does this turn into a
      // noop?
      QueueOperation(current_target, OperationType::ENTER, event_flags,
                     screen_position);
    }

    SetCurrentTargetWindow(current_target);
  }

  if (event.type() == ET_POINTER_UP) {
    if (current_target) {
      QueueOperation(current_target, OperationType::DROP, event_flags,
                     screen_position);
      waiting_for_final_drop_response_ = true;
    } else {
      // The pointer was released over no window or a window that doesn't
      // accept drags.
      MessageDragCompleted(false);
    }
  }

  return true;
}

void DragController::OnWillDestroyDragTargetConnection(
    DragTargetConnection* connection) {
  called_on_drag_mime_types_.erase(connection);
}

void DragController::MessageDragCompleted(bool success) {
  for (DragTargetConnection* connection : called_on_drag_mime_types_)
    connection->PerformOnDragDropDone();
  called_on_drag_mime_types_.clear();

  source_->OnDragCompleted(success);
  // |this| may be deleted now.
}

size_t DragController::GetSizeOfQueueForWindow(ServerWindow* window) {
  auto it = window_state_.find(window);
  if (it == window_state_.end())
    return 0u;
  if (it->second.waiting_on_reply == OperationType::NONE)
    return 0u;
  if (it->second.queued_operation.type == OperationType::NONE)
    return 1u;
  return 2u;
}

void DragController::SetCurrentTargetWindow(ServerWindow* current_target) {
  current_target_window_ = current_target;
}

void DragController::EnsureWindowObserved(ServerWindow* window) {
  if (!window)
    return;

  WindowState& state = window_state_[window];
  if (!state.observed) {
    state.observed = true;
    window->AddObserver(this);
  }
}

void DragController::QueueOperation(ServerWindow* window,
                                    OperationType type,
                                    uint32_t event_flags,
                                    gfx::Point screen_position) {
  // If this window doesn't have the mime data, send it.
  DragTargetConnection* connection = source_->GetDragTargetForWindow(window);
  if (connection != source_connection_ &&
      !base::ContainsKey(called_on_drag_mime_types_, connection)) {
    connection->PerformOnDragDropStart(mime_data_.Clone());
    called_on_drag_mime_types_.insert(connection);
  }

  WindowState& state = window_state_[window];
  // Set the queued operation to the incoming.
  state.queued_operation = {type, event_flags, screen_position};

  if (state.waiting_on_reply == OperationType::NONE) {
    // Send the operation immediately.
    DispatchOperation(window, &state);
  }
}

void DragController::DispatchOperation(ServerWindow* target,
                                       WindowState* state) {
  DragTargetConnection* connection = source_->GetDragTargetForWindow(target);

  DCHECK_EQ(OperationType::NONE, state->waiting_on_reply);
  Operation& op = state->queued_operation;
  switch (op.type) {
    case OperationType::NONE: {
      // NONE case to silence the compiler.
      NOTREACHED();
      break;
    }
    case OperationType::ENTER: {
      connection->PerformOnDragEnter(
          target, op.event_flags, op.screen_position, drag_operations_,
          base::Bind(&DragController::OnDragStatusCompleted,
                     weak_factory_.GetWeakPtr(), target->id()));
      state->waiting_on_reply = OperationType::ENTER;
      break;
    }
    case OperationType::OVER: {
      connection->PerformOnDragOver(
          target, op.event_flags, op.screen_position, drag_operations_,
          base::Bind(&DragController::OnDragStatusCompleted,
                     weak_factory_.GetWeakPtr(), target->id()));
      state->waiting_on_reply = OperationType::OVER;
      break;
    }
    case OperationType::LEAVE: {
      connection->PerformOnDragLeave(target);
      state->waiting_on_reply = OperationType::NONE;
      break;
    }
    case OperationType::DROP: {
      connection->PerformOnCompleteDrop(
          target, op.event_flags, op.screen_position, drag_operations_,
          base::Bind(&DragController::OnDragDropCompleted,
                     weak_factory_.GetWeakPtr(), target->id()));
      state->waiting_on_reply = OperationType::DROP;
      break;
    }
  }

  state->queued_operation = {OperationType::NONE, 0, gfx::Point()};
}

void DragController::OnRespondToOperation(ServerWindow* window) {
  WindowState& state = window_state_[window];
  DCHECK_NE(OperationType::NONE, state.waiting_on_reply);
  state.waiting_on_reply = OperationType::NONE;
  if (state.queued_operation.type != OperationType::NONE)
    DispatchOperation(window, &state);
}

void DragController::OnDragStatusCompleted(const WindowId& id,
                                           uint32_t bitmask) {
  ServerWindow* window = source_->GetWindowById(id);
  if (!window) {
    // The window has been deleted and its queue is empty.
    return;
  }

  // We must remove the completed item.
  OnRespondToOperation(window);

  // TODO(erg): |bitmask| is the allowed drag actions at the mouse location. We
  // should use this data to change the cursor.
}

void DragController::OnDragDropCompleted(const WindowId& id, uint32_t bitmask) {
  ServerWindow* window = source_->GetWindowById(id);
  if (!window) {
    // The window has been deleted after we sent the drop message. It's really
    // hard to recover from this so just signal to the source that our drag
    // failed.
    MessageDragCompleted(false);
    return;
  }

  OnRespondToOperation(window);
  MessageDragCompleted(bitmask != 0u);
}

void DragController::OnWindowDestroying(ServerWindow* window) {
  auto it = window_state_.find(window);
  if (it != window_state_.end()) {
    window->RemoveObserver(this);
    window_state_.erase(it);
  }

  if (current_target_window_ == window)
    current_target_window_ = nullptr;

  if (source_window_ == window) {
    source_window_ = nullptr;
    // Our source window is being deleted, fail the drag.
    MessageDragCompleted(false);
  }
}

}  // namespace ws
}  // namespace ui
