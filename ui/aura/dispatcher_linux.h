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
#include "ui/aura/env.h"

namespace aura {

class RootWindowHostLinux;

class DispatcherLinux : public Dispatcher {
 public:
  DispatcherLinux();
  virtual ~DispatcherLinux();

  void RootWindowHostCreated(::Window window,
                             ::Window root,
                             RootWindowHostLinux* host);
  void RootWindowHostDestroying(::Window window, ::Window root);

  // Overridden from MessageLoop::Dispatcher:
  virtual base::MessagePumpDispatcher::DispatchStatus Dispatch(
      XEvent* xev) OVERRIDE;

 private:
  typedef std::map< ::Window, RootWindowHostLinux* > HostsMap;

  RootWindowHostLinux* GetRootWindowHostForXEvent(XEvent* xev) const;

  HostsMap hosts_;

  DISALLOW_COPY_AND_ASSIGN(DispatcherLinux);
};

}  // namespace aura

#endif  // UI_AURA_DISPATCHER_LINUX_H_
