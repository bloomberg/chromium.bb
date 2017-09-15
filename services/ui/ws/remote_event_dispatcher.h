// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_UI_WS_REMOTE_EVENT_DISPATCHER_H_
#define SERVICES_UI_WS_REMOTE_EVENT_DISPATCHER_H_

#include "services/ui/public/interfaces/remote_event_dispatcher.mojom.h"

namespace ui {
namespace ws {

class WindowServer;

class RemoteEventDispatcherImpl : public mojom::RemoteEventDispatcher {
 public:
  explicit RemoteEventDispatcherImpl(WindowServer* server);
  ~RemoteEventDispatcherImpl() override;

 private:
  // mojom::RemoteEventDispatcher:
  void DispatchEvent(int64_t display_id,
                     std::unique_ptr<ui::Event> event,
                     const DispatchEventCallback& cb) override;

  WindowServer* window_server_;

  DISALLOW_COPY_AND_ASSIGN(RemoteEventDispatcherImpl);
};

}  // namespace ws
}  // namespace ui

#endif  // SERVICES_UI_WS_REMOTE_EVENT_DISPATCHER_H_
