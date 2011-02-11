// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/controls/menu/nested_dispatcher_gtk.h"

#if defined(TOUCH_UI)
#include "views/focus/accelerator_handler.h"
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
  MessageLoopForUI::current()->Run(this);
  if (allow_nested_task_)
    MessageLoopForUI::current()->SetNestableTasksAllowed(nestable);
  bool creator_is_deleted = creator_ == NULL;
  delete this;
  return creator_is_deleted;
}

void NestedDispatcherGtk::CreatorDestroyed() {
  creator_ = NULL;
}

bool NestedDispatcherGtk::Dispatch(GdkEvent* event) {
  if (creator_ != NULL) {
#if defined(TOUCH_UI)
    return static_cast<base::MessagePumpForUI::Dispatcher*>
        (creator_)->Dispatch(event);
#else
    return creator_->Dispatch(event);
#endif
  } else {
    return false;
  }
}

#if defined(TOUCH_UI)
base::MessagePumpGlibXDispatcher::DispatchStatus
    NestedDispatcherGtk::DispatchX(XEvent* xevent) {
  return creator_->DispatchX(xevent);
}
#endif

}  // namespace views
