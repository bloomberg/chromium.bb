// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "ppapi/nacl_irt/plugin_startup.h"

namespace ppapi {
namespace {

int g_nacl_browser_ipc_fd = -1;
int g_nacl_renderer_ipc_fd = -1;

base::WaitableEvent* g_shutdown_event = NULL;
base::Thread* g_io_thread = NULL;

}  // namespace

void SetIPCFileDescriptors(int browser_ipc_fd, int renderer_ipc_fd) {
  // The initialization must be only once.
  DCHECK_EQ(g_nacl_browser_ipc_fd, -1);
  DCHECK_EQ(g_nacl_renderer_ipc_fd, -1);
  g_nacl_browser_ipc_fd = browser_ipc_fd;
  g_nacl_renderer_ipc_fd = renderer_ipc_fd;
}

void StartUpPlugin() {
  // The start up must be called only once.
  DCHECK(!g_shutdown_event);
  DCHECK(!g_io_thread);

  g_shutdown_event = new base::WaitableEvent(true, false);
  g_io_thread = new base::Thread("Chrome_NaClIOThread");
  g_io_thread->StartWithOptions(
      base::Thread::Options(base::MessageLoop::TYPE_IO, 0));
}

int GetBrowserIPCFileDescriptor() {
  // The descriptor must be initialized in advance.
  DCHECK_NE(g_nacl_browser_ipc_fd, -1);
  return g_nacl_browser_ipc_fd;
}

int GetRendererIPCFileDescriptor() {
  // The descriptor must be initialized in advance.
  DCHECK_NE(g_nacl_renderer_ipc_fd, -1);
  return g_nacl_renderer_ipc_fd;
}

base::WaitableEvent* GetShutdownEvent() {
  // The shutdown event must be initialized in advance.
  DCHECK(g_shutdown_event);
  return g_shutdown_event;
}

base::Thread* GetIOThread() {
  // The IOThread must be initialized in advance.
  DCHECK(g_io_thread);
  return g_io_thread;
}

}  // namespace ppapi
