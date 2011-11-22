// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_CONTROLS_MENU_NESTED_DISPATCHER_GTK_H_
#define VIEWS_CONTROLS_MENU_NESTED_DISPATCHER_GTK_H_
#pragma once

#include "base/message_loop.h"

namespace views {

// A nested dispatcher that can out-live the creator of this
// dispatcher. This is to deal with the scenario where a menu object
// is deleted while it's handling the message loop.  Note that
// |RunAndSelfDestruct| method always delete itself regardless of
// whether or not the menu object is deleted, so a menu object should
// create a new instance for each open request.
// http://crosbug.com/7228, http://crosbug.com/7929
class NestedDispatcherGtk : public MessageLoopForUI::Dispatcher {
 public:
  NestedDispatcherGtk(MessageLoopForUI::Dispatcher* creator,
                      bool allow_nested_task);

  // Run the messsage loop and returns if the menu has been
  // deleted in the nested loop. Returns true if the menu is
  // deleted, or false otherwise.
  bool RunAndSelfDestruct();

  // Tells the nested dispatcher that creator has been destroyed.
  void CreatorDestroyed();

 private:
  virtual ~NestedDispatcherGtk() {}

  // Overriden from MessageLoopForUI::Dispatcher:
  virtual bool Dispatch(GdkEvent* event);

  // Creator of the nested loop.
  MessageLoopForUI::Dispatcher* creator_;

  // True to allow nested task in the message loop.
  bool allow_nested_task_;

  DISALLOW_COPY_AND_ASSIGN(NestedDispatcherGtk);
};

}  // namespace views

#endif  // VIEWS_CONTROLS_MENU_NESTED_DISPATCHER_GTK_H_
