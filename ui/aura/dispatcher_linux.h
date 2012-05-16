// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_DISPATCHER_LINUX_H_
#define UI_AURA_DISPATCHER_LINUX_H_
#pragma once

#include <map>
#include <X11/Xlib.h>
// Get rid of a macro from Xlib.h that conflicts with Aura's RootWindow class.
#undef RootWindow

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/message_loop.h"

namespace aura {

class DispatcherLinux : public MessageLoop::Dispatcher,
                        public base::MessagePumpObserver {
 public:
  DispatcherLinux();
  virtual ~DispatcherLinux();

  void WindowDispatcherCreated(::Window window,
                               MessageLoop::Dispatcher* dispatcher);
  void WindowDispatcherDestroying(::Window window);

  // Overridden from MessageLoop::Dispatcher:
  virtual bool Dispatch(const base::NativeEvent& event) OVERRIDE;

  // Overridden from base::MessagePumpObserver:
  virtual base::EventStatus WillProcessEvent(
      const base::NativeEvent& event) OVERRIDE;
  virtual void DidProcessEvent(const base::NativeEvent& event) OVERRIDE;

 private:
  typedef std::map< ::Window, MessageLoop::Dispatcher* > DispatchersMap;

  MessageLoop::Dispatcher* GetDispatcherForXEvent(XEvent* xev) const;

  DispatchersMap dispatchers_;

  DISALLOW_COPY_AND_ASSIGN(DispatcherLinux);
};

}  // namespace aura

#endif  // UI_AURA_DISPATCHER_LINUX_H_
