// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_CLIENT_DISPATCHER_CLIENT_H_
#define UI_AURA_CLIENT_DISPATCHER_CLIENT_H_

#include "base/message_loop/message_pump_dispatcher.h"
#include "ui/aura/aura_export.h"
#include "ui/aura/window.h"

namespace aura {
class Window;
namespace client {

// An interface implemented by an object which handles nested dispatchers.
class AURA_EXPORT DispatcherClient {
 public:
  virtual void RunWithDispatcher(base::MessagePumpDispatcher* dispatcher,
                                 aura::Window* associated_window) = 0;
};

AURA_EXPORT void SetDispatcherClient(Window* root_window,
                                     DispatcherClient* client);
AURA_EXPORT DispatcherClient* GetDispatcherClient(Window* root_window);

}  // namespace client
}  // namespace aura

#endif  // UI_AURA_CLIENT_DISPATCHER_CLIENT_H_
