// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/child/webkitplatformsupport_child_impl.h"

#include "base/memory/discardable_memory.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"
#include "webkit/child/fling_curve_configuration.h"
#include "webkit/child/web_discardable_memory_impl.h"
#include "webkit/child/webthread_impl.h"
#include "webkit/child/worker_task_runner.h"

#if defined(OS_ANDROID)
#include "webkit/child/fling_animator_impl_android.h"
#endif

using blink::WebFallbackThemeEngine;
using blink::WebThemeEngine;

namespace webkit_glue {

WebKitPlatformSupportChildImpl::WebKitPlatformSupportChildImpl()
    : current_thread_slot_(&DestroyCurrentThread),
      fling_curve_configuration_(new FlingCurveConfiguration) {}

WebKitPlatformSupportChildImpl::~WebKitPlatformSupportChildImpl() {}

WebThemeEngine* WebKitPlatformSupportChildImpl::themeEngine() {
  return &native_theme_engine_;
}

WebFallbackThemeEngine* WebKitPlatformSupportChildImpl::fallbackThemeEngine() {
  return &fallback_theme_engine_;
}

void WebKitPlatformSupportChildImpl::SetFlingCurveParameters(
    const std::vector<float>& new_touchpad,
    const std::vector<float>& new_touchscreen) {
  fling_curve_configuration_->SetCurveParameters(new_touchpad, new_touchscreen);
}

blink::WebGestureCurve*
WebKitPlatformSupportChildImpl::createFlingAnimationCurve(
    int device_source,
    const blink::WebFloatPoint& velocity,
    const blink::WebSize& cumulative_scroll) {
#if defined(OS_ANDROID)
  return FlingAnimatorImpl::CreateAndroidGestureCurve(velocity,
                                                      cumulative_scroll);
#endif

  if (device_source == blink::WebGestureEvent::Touchscreen)
    return fling_curve_configuration_->CreateForTouchScreen(velocity,
                                                            cumulative_scroll);

  return fling_curve_configuration_->CreateForTouchPad(velocity,
                                                       cumulative_scroll);
}

blink::WebThread* WebKitPlatformSupportChildImpl::createThread(
    const char* name) {
  return new WebThreadImpl(name);
}

blink::WebThread* WebKitPlatformSupportChildImpl::currentThread() {
  WebThreadImplForMessageLoop* thread =
      static_cast<WebThreadImplForMessageLoop*>(current_thread_slot_.Get());
  if (thread)
    return (thread);

  scoped_refptr<base::MessageLoopProxy> message_loop =
      base::MessageLoopProxy::current();
  if (!message_loop.get())
    return NULL;

  thread = new WebThreadImplForMessageLoop(message_loop.get());
  current_thread_slot_.Set(thread);
  return thread;
}

void WebKitPlatformSupportChildImpl::didStartWorkerRunLoop(
    const blink::WebWorkerRunLoop& runLoop) {
  WorkerTaskRunner* worker_task_runner = WorkerTaskRunner::Instance();
  worker_task_runner->OnWorkerRunLoopStarted(runLoop);
}

void WebKitPlatformSupportChildImpl::didStopWorkerRunLoop(
    const blink::WebWorkerRunLoop& runLoop) {
  WorkerTaskRunner* worker_task_runner = WorkerTaskRunner::Instance();
  worker_task_runner->OnWorkerRunLoopStopped(runLoop);
}

blink::WebDiscardableMemory*
WebKitPlatformSupportChildImpl::allocateAndLockDiscardableMemory(size_t bytes) {
  if (!base::DiscardableMemory::SupportedNatively())
    return NULL;
  return WebDiscardableMemoryImpl::CreateLockedMemory(bytes).release();
}

// static
void WebKitPlatformSupportChildImpl::DestroyCurrentThread(void* thread) {
  WebThreadImplForMessageLoop* impl =
      static_cast<WebThreadImplForMessageLoop*>(thread);
  delete impl;
}

}  // namespace webkit_glue
