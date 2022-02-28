// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wayland/wayland_watcher.h"

#include "base/task/current_thread.h"
#include "components/exo/wayland/server.h"

namespace exo {
namespace wayland {

WaylandWatcher::WaylandWatcher(wayland::Server* server)
    : controller_(FROM_HERE), server_(server) {
  base::CurrentUIThread::Get()->WatchFileDescriptor(
      server_->GetFileDescriptor(),
      true,  // persistent
      base::MessagePumpForUI::WATCH_READ, &controller_, this);
}

WaylandWatcher::~WaylandWatcher() {
  controller_.StopWatchingFileDescriptor();
}

void WaylandWatcher::OnFileCanReadWithoutBlocking(int fd) {
  server_->Dispatch(base::TimeDelta());
  server_->Flush();
}

void WaylandWatcher::OnFileCanWriteWithoutBlocking(int fd) {
  NOTREACHED();
}

}  // namespace wayland
}  // namespace exo
