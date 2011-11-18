// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/controls/menu/nested_dispatcher_gtk.h"

#if defined(TOUCH_UI)
#include "ui/views/focus/accelerator_handler.h"
#endif

namespace views {

NestedDispatcherGtk::NestedDispatcherGtk(MessageLoopForUI::Dispatcher* creator,
                                         bool allow_nested_task)
  : creator_(creator),
    allow_nested_task_(allow_nested_task) {
}

bool NestedDispatcherGtk::RunAndSelfDestruct() {
  bool nestable = MessageLoopForUI::current()->NestableTasksAllowed();
  if (allow_nested_task_)
    MessageLoopForUI::current()->SetNestableTasksAllowed(true);
  MessageLoopForUI::current()->RunWithDispatcher(this);
  if (allow_nested_task_)
    MessageLoopForUI::current()->SetNestableTasksAllowed(nestable);
  bool creator_is_deleted = creator_ == NULL;
  delete this;
  return creator_is_deleted;
}

void NestedDispatcherGtk::CreatorDestroyed() {
  creator_ = NULL;
}

#if defined(TOUCH_UI)
base::MessagePumpDispatcher::DispatchStatus
    NestedDispatcherGtk::Dispatch(XEvent* xevent) {
  return creator_->Dispatch(xevent);
}
#else
bool NestedDispatcherGtk::Dispatch(GdkEvent* event) {
  return creator_ && creator_->Dispatch(event);
}
#endif  // defined(TOUCH_UI)

}  // namespace views
