// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WIDGET_DESKTOP_AURA_DESKTOP_DISPATCHER_CLIENT_H_
#define UI_VIEWS_WIDGET_DESKTOP_AURA_DESKTOP_DISPATCHER_CLIENT_H_

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "ui/aura/client/dispatcher_client.h"
#include "ui/views/views_export.h"

namespace views {

// TODO(erg): I won't lie to you; I have no idea what this is or what it does.
class VIEWS_EXPORT DesktopDispatcherClient
    : public aura::client::DispatcherClient {
 public:
  DesktopDispatcherClient();
  virtual ~DesktopDispatcherClient();

  virtual void RunWithDispatcher(base::MessagePumpDispatcher* dispatcher,
                                 aura::Window* associated_window) OVERRIDE;
  virtual void QuitNestedMessageLoop() OVERRIDE;

 private:
  base::Closure quit_closure_;

  // Used to keep track of whether the client has been destroyed while the
  // nested loop was running.
  base::WeakPtrFactory<DesktopDispatcherClient> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(DesktopDispatcherClient);
};

}  // namespace views

#endif  // UI_VIEWS_WIDGET_DESKTOP_AURA_DESKTOP_DISPATCHER_CLIENT_H_
