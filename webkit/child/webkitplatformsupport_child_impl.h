// // Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_CHILD_WEBKITPLATFORMSUPPORT_CHILD_IMPL_H_
#define WEBKIT_CHILD_WEBKITPLATFORMSUPPORT_CHILD_IMPL_H_

#include "base/threading/thread_local_storage.h"
#include "webkit/child/webfallbackthemeengine_impl.h"
#include "webkit/child/webkit_child_export.h"
#include "webkit/child/webkitplatformsupport_impl.h"

#if defined(USE_DEFAULT_RENDER_THEME)
#include "webkit/child/webthemeengine_impl_default.h"
#elif defined(OS_WIN)
#include "webkit/child/webthemeengine_impl_win.h"
#elif defined(OS_MACOSX)
#include "webkit/child/webthemeengine_impl_mac.h"
#elif defined(OS_ANDROID)
#include "webkit/child/webthemeengine_impl_android.h"
#endif

namespace webkit_glue {

class FlingCurveConfiguration;

class WEBKIT_CHILD_EXPORT WebKitPlatformSupportChildImpl :
    public WebKitPlatformSupportImpl {
 public:
  WebKitPlatformSupportChildImpl();
  virtual ~WebKitPlatformSupportChildImpl();

  // Platform methods (partial implementation):
  virtual WebKit::WebThemeEngine* themeEngine();
  virtual WebKit::WebFallbackThemeEngine* fallbackThemeEngine();

  void SetFlingCurveParameters(
    const std::vector<float>& new_touchpad,
    const std::vector<float>& new_touchscreen);

  virtual WebKit::WebGestureCurve* createFlingAnimationCurve(
      int device_source,
      const WebKit::WebFloatPoint& velocity,
      const WebKit::WebSize& cumulative_scroll) OVERRIDE;

  virtual WebKit::WebThread* createThread(const char* name);
  virtual WebKit::WebThread* currentThread();

  virtual void didStartWorkerRunLoop(
      const WebKit::WebWorkerRunLoop& runLoop) OVERRIDE;
  virtual void didStopWorkerRunLoop(
      const WebKit::WebWorkerRunLoop& runLoop) OVERRIDE;

  virtual WebKit::WebDiscardableMemory* allocateAndLockDiscardableMemory(
      size_t bytes);

 private:
  static void DestroyCurrentThread(void*);

  WebThemeEngineImpl native_theme_engine_;
  WebFallbackThemeEngineImpl fallback_theme_engine_;
  base::ThreadLocalStorage::Slot current_thread_slot_;
  scoped_ptr<FlingCurveConfiguration> fling_curve_configuration_;
};

}  // namespace webkit_glue

#endif  // WEBKIT_CHILD_WEBKITPLATFORMSUPPORT_CHILD_IMPL_H_
