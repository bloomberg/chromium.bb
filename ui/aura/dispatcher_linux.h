// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_DISPATCHER_LINUX_H_
#define UI_AURA_DISPATCHER_LINUX_H_
#pragma once

#include <map>
#include <vector>
#include <X11/Xlib.h>
// Get rid of a macro from Xlib.h that conflicts with Aura's RootWindow class.
#undef RootWindow

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/message_loop.h"
#include "ui/aura/aura_export.h"

namespace aura {

class AURA_EXPORT DispatcherLinux : public MessageLoop::Dispatcher,
                                    public base::MessagePumpObserver {
 public:
  DispatcherLinux();
  virtual ~DispatcherLinux();

  // Adds/Removes |dispatcher| for the |x_window|.
  void AddDispatcherForWindow(MessageLoop::Dispatcher* dispatcher,
                                   ::Window x_window);
  void RemoveDispatcherForWindow(::Window x_window);

  // Adds/Removes |dispatcher| to receive all events sent to the X
  // root window.
  void AddDispatcherForRootWindow(MessageLoop::Dispatcher* dispatcher);
  void RemoveDispatcherForRootWindow(MessageLoop::Dispatcher* dispatcher);

  // Overridden from MessageLoop::Dispatcher:
  virtual bool Dispatch(const base::NativeEvent& event) OVERRIDE;

  // Overridden from base::MessagePumpObserver:
  virtual base::EventStatus WillProcessEvent(
      const base::NativeEvent& event) OVERRIDE;
  virtual void DidProcessEvent(const base::NativeEvent& event) OVERRIDE;

 private:
  typedef std::map< ::Window, MessageLoop::Dispatcher* > DispatchersMap;
  typedef std::vector<MessageLoop::Dispatcher*> Dispatchers;

  MessageLoop::Dispatcher* GetDispatcherForXEvent(XEvent* xev) const;

  DispatchersMap dispatchers_;
  Dispatchers root_window_dispatchers_;

  ::Window x_root_window_;

  DISALLOW_COPY_AND_ASSIGN(DispatcherLinux);
};

}  // namespace aura

#endif  // UI_AURA_DISPATCHER_LINUX_H_
