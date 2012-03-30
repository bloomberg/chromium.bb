// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_DESKTOP_DESKTOP_DISPATCHER_CLIENT_H_
#define UI_AURA_DESKTOP_DESKTOP_DISPATCHER_CLIENT_H_
#pragma once

#include "base/basictypes.h"
#include "ui/aura/client/dispatcher_client.h"

namespace aura {

// TODO(erg): I won't lie to you; I have no idea what this is or what it does.
class AURA_EXPORT DesktopDispatcherClient : public client::DispatcherClient {
 public:
  DesktopDispatcherClient();
  virtual ~DesktopDispatcherClient();

  virtual void RunWithDispatcher(MessageLoop::Dispatcher* dispatcher,
                                 aura::Window* associated_window,
                                 bool nestable_tasks_allowed) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(DesktopDispatcherClient);
};

}  // namespace aura

#endif  // UI_AURA_DESKTOP_DESKTOP_DISPATCHER_CLIENT_H_
