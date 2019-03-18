// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/in_process_renderer_thread.h"

#include "build/build_config.h"
#include "third_party/blink/public/platform/scheduler/web_thread_scheduler.h"
#include "content/public/renderer/render_thread.h"

#if defined(OS_ANDROID)
#include "base/android/jni_android.h"
#endif

namespace content {

#if defined(OS_ANDROID)
extern bool g_browser_main_loop_shutting_down;
#endif

InProcessRendererThread::InProcessRendererThread(
    const InProcessChildThreadParams& params)
    : Thread("Chrome_InProcRendererThread"), params_(params) {}

InProcessRendererThread::~InProcessRendererThread() {
#if defined(OS_ANDROID)
  // Don't allow the render thread to be shut down in single process mode on
  // Android unless the browser is shutting down.
  // Temporary CHECK() to debug http://crbug.com/514141
  CHECK(g_browser_main_loop_shutting_down);
#endif

  Stop();
}

void InProcessRendererThread::Init() {
  // Call AttachCurrentThreadWithName, before any other AttachCurrentThread()
  // calls. The latter causes Java VM to assign Thread-??? to the thread name.
  // Please note calls to AttachCurrentThreadWithName after AttachCurrentThread
  // will not change the thread name kept in Java VM.
#if defined(OS_ANDROID)
  base::android::AttachCurrentThreadWithName(thread_name());
  // Make sure we aren't somehow reinitialising the inprocess renderer thread on
  // Android. Temporary CHECK() to debug http://crbug.com/514141
  CHECK(!render_process_);
#endif

  std::unique_ptr<blink::scheduler::WebThreadScheduler> main_thread_scheduler =
      blink::scheduler::WebThreadScheduler::CreateMainThreadScheduler();

  RenderThread::InitInProcessRenderer(params_, std::move(main_thread_scheduler));
}

void InProcessRendererThread::CleanUp() {
#if defined(OS_ANDROID)
  // Don't allow the render thread to be shut down in single process mode on
  // Android unless the browser is shutting down.
  // Temporary CHECK() to debug http://crbug.com/514141
  CHECK(g_browser_main_loop_shutting_down);
#endif

  RenderThread::CleanUpInProcessRenderer();

  // It's a little lame to manually set this flag.  But the single process
  // RendererThread will receive the WM_QUIT.  We don't need to assert on
  // this thread, so just force the flag manually.
  // If we want to avoid this, we could create the InProcRendererThread
  // directly with _beginthreadex() rather than using the Thread class.
  // We used to set this flag in the Init function above. However there
  // other threads like WebThread which are created by this thread
  // which resets this flag. Please see Thread::StartWithOptions. Setting
  // this flag to true in Cleanup works around these problems.
  SetThreadWasQuitProperly(true);
}

base::Thread* CreateInProcessRendererThread(
    const InProcessChildThreadParams& params) {
  return new InProcessRendererThread(params);
}

}  // namespace content
