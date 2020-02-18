// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_VR_GL_BROWSER_INTERFACE_H_
#define CHROME_BROWSER_ANDROID_VR_GL_BROWSER_INTERFACE_H_

#include <memory>

#include "base/android/jni_weak_ref.h"
#include "chrome/browser/vr/assets_load_status.h"
#include "chrome/browser/vr/ui_test_input.h"
#include "device/vr/public/mojom/vr_service.mojom.h"
#include "third_party/gvr-android-sdk/src/libraries/headers/vr/gvr/capi/include/gvr_types.h"
#include "ui/gfx/transform.h"

namespace gl {
class SurfaceTexture;
}

namespace vr {

// BrowserRenderer and its delegates talk to VrShell through this interface.
class GlBrowserInterface {
 public:
  virtual ~GlBrowserInterface() {}

  virtual void ForceExitVr() = 0;
  virtual void ContentSurfaceCreated(jobject surface,
                                     gl::SurfaceTexture* texture) = 0;
  virtual void ContentOverlaySurfaceCreated(jobject surface,
                                            gl::SurfaceTexture* texture) = 0;
  virtual void GvrDelegateReady(gvr::ViewerType viewer_type) = 0;
  // XRSessionPtr is optional, if null, the request failed.
  virtual void SendRequestPresentReply(device::mojom::XRSessionPtr) = 0;
  virtual void DialogSurfaceCreated(jobject surface,
                                    gl::SurfaceTexture* texture) = 0;
  virtual void ToggleCardboardGamepad(bool enabled) = 0;
};

}  // namespace vr

#endif  // CHROME_BROWSER_ANDROID_VR_GL_BROWSER_INTERFACE_H_
