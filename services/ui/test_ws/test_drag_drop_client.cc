// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/test_ws/test_drag_drop_client.h"

#include "base/logging.h"
#include "ui/aura/client/drag_drop_delegate.h"
#include "ui/aura/window.h"
#include "ui/base/dragdrop/drop_target_event.h"

namespace ui {
namespace test {

TestDragDropClient::TestDragDropClient() = default;
TestDragDropClient::~TestDragDropClient() = default;

int TestDragDropClient::StartDragAndDrop(
    const ui::OSExchangeData& data,
    aura::Window* root_window,
    aura::Window* source_window,
    const gfx::Point& screen_location,
    int operation,
    ui::DragDropTypes::DragEventSource source) {
  if (IsDragDropInProgress())
    return ui::DragDropTypes::DRAG_NONE;

  DCHECK(!root_window_);

  root_window_ = root_window;
  root_window_->AddObserver(this);
  root_window_->AddPreTargetHandler(this, ui::EventTarget::Priority::kSystem);

  drag_data_ = &data;
  drag_operation_ = operation;

  drag_loop_ = std::make_unique<base::RunLoop>(
      base::RunLoop::Type::kNestableTasksAllowed);
  drag_loop_->Run();
  drag_loop_.reset();

  if (root_window_) {
    root_window_->RemovePreTargetHandler(this);
    root_window_->RemoveObserver(this);
    root_window_ = nullptr;
  }

  return drag_operation_;
}

void TestDragDropClient::DragCancel() {
  if (!IsDragDropInProgress())
    return;

  drag_operation_ = ui::DragDropTypes::DRAG_NONE;
  drag_loop_->Quit();
}

bool TestDragDropClient::IsDragDropInProgress() {
  return !!drag_loop_;
}

void TestDragDropClient::OnWindowDestroyed(aura::Window* window) {
  DCHECK_EQ(root_window_, window);
  root_window_ = nullptr;
  DragCancel();
}

void TestDragDropClient::OnMouseEvent(ui::MouseEvent* event) {
  DCHECK(IsDragDropInProgress());
  DCHECK(root_window_);

  aura::Window* target =
      root_window_->GetEventHandlerForPoint(event->location());
  if (!target) {
    NOTREACHED() << "No target found for drag and drop mouse event.";
    event->StopPropagation();
    return;
  }

  event->ConvertLocationToTarget(root_window_, target);
  switch (event->type()) {
    case ui::ET_MOUSE_DRAGGED:
      DragUpdate(target, *event);
      break;
    case ui::ET_MOUSE_RELEASED:
      Drop(target, *event);
      break;
    default:
      break;
  }
  event->StopPropagation();
}

void TestDragDropClient::DragUpdate(aura::Window* target,
                                    const ui::LocatedEvent& event) {
  if (target != drag_window_) {
    if (drag_window_) {
      aura::client::DragDropDelegate* delegate =
          aura::client::GetDragDropDelegate(drag_window_);
      if (delegate)
        delegate->OnDragExited();
    }

    drag_window_ = target;

    if (drag_window_) {
      aura::client::DragDropDelegate* delegate =
          aura::client::GetDragDropDelegate(drag_window_);
      if (delegate) {
        ui::DropTargetEvent e(*drag_data_, event.location_f(),
                              event.root_location_f(), drag_operation_);
        e.set_flags(event.flags());
        ui::Event::DispatcherApi(&e).set_target(target);
        delegate->OnDragEntered(e);
      }
    }
  } else {
    aura::client::DragDropDelegate* delegate =
        aura::client::GetDragDropDelegate(drag_window_);
    if (delegate) {
      ui::DropTargetEvent e(*drag_data_, event.location_f(),
                            event.root_location_f(), drag_operation_);
      e.set_flags(event.flags());
      ui::Event::DispatcherApi(&e).set_target(target);
      delegate->OnDragUpdated(e);
    }
  }
}

void TestDragDropClient::Drop(aura::Window* target,
                              const ui::LocatedEvent& event) {
  if (target != drag_window_)
    DragUpdate(target, event);

  aura::client::DragDropDelegate* delegate =
      aura::client::GetDragDropDelegate(target);
  if (delegate) {
    ui::DropTargetEvent e(*drag_data_, event.location_f(),
                          event.root_location_f(), drag_operation_);
    e.set_flags(event.flags());
    ui::Event::DispatcherApi(&e).set_target(target);
    drag_operation_ = delegate->OnPerformDrop(e);
  }

  drag_loop_->Quit();
}

}  // namespace test
}  // namespace ui
