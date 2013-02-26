// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_COMPOSITOR_BINDINGS_WEB_COMPOSITOR_SUPPORT_SOFTWARE_OUTPUT_DEVICE_H_
#define WEBKIT_COMPOSITOR_BINDINGS_WEB_COMPOSITOR_SUPPORT_SOFTWARE_OUTPUT_DEVICE_H_

#include "base/logging.h"
#include "cc/software_output_device.h"
#include "skia/ext/refptr.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebImage.h"
#include "third_party/skia/include/core/SkDevice.h"
#include "webkit/compositor_bindings/webkit_compositor_bindings_export.h"

namespace webkit {

class WebCompositorSupportSoftwareOutputDevice :
    public cc::SoftwareOutputDevice {
 public:
  WEBKIT_COMPOSITOR_BINDINGS_EXPORT WebCompositorSupportSoftwareOutputDevice();
  virtual ~WebCompositorSupportSoftwareOutputDevice();

  virtual WebKit::WebImage* Lock(bool for_write) OVERRIDE;
  virtual void Unlock() OVERRIDE;

  virtual void DidChangeViewportSize(gfx::Size size) OVERRIDE;

 private:
  skia::RefPtr<SkDevice> device_;
  WebKit::WebImage image_;
};

}  // namespace webkit

#endif  // WEBKIT_COMPOSITOR_BINDINGS_WEB_COMPOSITOR_SUPPORT_SOFTWARE_OUTPUT_DEVICE_H_
