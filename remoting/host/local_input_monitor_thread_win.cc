// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/local_input_monitor_thread_win.h"

#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/synchronization/waitable_event.h"
#include "remoting/host/mouse_move_observer.h"
#include "third_party/skia/include/core/SkPoint.h"

using namespace remoting;

namespace {
LocalInputMonitorThread* g_local_input_monitor_thread = NULL;
base::LazyInstance<base::Lock>::Leaky g_thread_lock = LAZY_INSTANCE_INITIALIZER;
}  // namespace

LocalInputMonitorThread::LocalInputMonitorThread()
    : base::SimpleThread("LocalInputMonitor") {
}

LocalInputMonitorThread::~LocalInputMonitorThread() {
  DCHECK(observers_.empty());
}

void LocalInputMonitorThread::AddObserver(
    MouseMoveObserver* mouse_move_observer) {
  base::AutoLock lock(lock_);
  observers_.insert(mouse_move_observer);
}

bool LocalInputMonitorThread::RemoveObserver(
    MouseMoveObserver* mouse_move_observer) {
  base::AutoLock lock(lock_);
  observers_.erase(mouse_move_observer);
  return observers_.empty();
}

void LocalInputMonitorThread::Stop() {
  CHECK(PostThreadMessage(tid(), WM_QUIT, 0, 0));
  Join();
}

void LocalInputMonitorThread::Run() {
  extern HMODULE g_hModule;
  HHOOK win_event_hook = SetWindowsHookEx(WH_MOUSE_LL, HandleLowLevelMouseEvent,
                                          g_hModule, 0);
  if (!win_event_hook) {
    DWORD err = GetLastError();
    LOG(ERROR) << "SetWindowHookEx failed: " << err;
    return;
  }

  MSG msg;
  BOOL result;
  while ((result = GetMessage(&msg, NULL, 0, 0)) != 0) {
    if (result == -1) {
      DWORD err = GetLastError();
      LOG(ERROR) << "GetMessage failed: " << err;
      break;
    } else {
      DispatchMessage(&msg);
    }
  }

  if (win_event_hook) {
    CHECK(UnhookWindowsHookEx(win_event_hook));
  }
}

void LocalInputMonitorThread::LocalMouseMoved(const SkIPoint& mouse_position) {
  base::AutoLock lock(lock_);
  for (MouseMoveObservers::const_iterator i = observers_.begin();
       i != observers_.end(); ++i) {
    (*i)->OnLocalMouseMoved(mouse_position);
  }
}

LRESULT WINAPI LocalInputMonitorThread::HandleLowLevelMouseEvent(
    int code, WPARAM event_type, LPARAM event_data) {
  if (code == HC_ACTION) {
    if (event_type == WM_MOUSEMOVE) {
      PMSLLHOOKSTRUCT data = reinterpret_cast<PMSLLHOOKSTRUCT>(event_data);
      if ((data->flags & LLMHF_INJECTED) == 0) {
        SkIPoint mouse_position = SkIPoint::Make(data->pt.x, data->pt.y);
        // |g_local_input_monitor_thread| cannot be NULL because this function
        // is called from within the GetMessage call running on that thread.
        DCHECK(g_local_input_monitor_thread);
        g_local_input_monitor_thread->LocalMouseMoved(mouse_position);
      }
    }
  }
  return CallNextHookEx(NULL, code, event_type, event_data);
}


// static
void LocalInputMonitorThread::AddMouseMoveObserver(
    MouseMoveObserver* mouse_move_observer) {
  base::AutoLock lock(g_thread_lock.Get());
  if (!g_local_input_monitor_thread) {
    g_local_input_monitor_thread = new LocalInputMonitorThread;
    g_local_input_monitor_thread->Start();
  }
  g_local_input_monitor_thread->AddObserver(mouse_move_observer);
}

// static
void LocalInputMonitorThread::RemoveMouseMoveObserver(
    MouseMoveObserver* mouse_move_observer) {
  DCHECK(g_local_input_monitor_thread);
  base::AutoLock lock(g_thread_lock.Get());
  if (g_local_input_monitor_thread->RemoveObserver(mouse_move_observer)) {
    g_local_input_monitor_thread->Stop();
    delete g_local_input_monitor_thread;
    g_local_input_monitor_thread = NULL;
  }
}
