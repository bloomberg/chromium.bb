// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/compositor_bindings/web_compositor_support_software_output_device.h"

namespace webkit {

WebCompositorSupportSoftwareOutputDevice::
    WebCompositorSupportSoftwareOutputDevice() {}

WebCompositorSupportSoftwareOutputDevice::
    ~WebCompositorSupportSoftwareOutputDevice() {}

WebKit::WebImage* WebCompositorSupportSoftwareOutputDevice::Lock(
    bool for_write) {
  DCHECK(device_);
  image_ = device_->accessBitmap(for_write);
  return &image_;
}

void WebCompositorSupportSoftwareOutputDevice::Unlock() { image_.reset(); }

void WebCompositorSupportSoftwareOutputDevice::DidChangeViewportSize(
    gfx::Size size) {
  if (device_.get() &&
      size.width() == device_->width() &&
      size.height() == device_->height())
    return;

  device_ = skia::AdoptRef(new SkDevice(
      SkBitmap::kARGB_8888_Config, size.width(), size.height(), true));
}

}  // namespace webkit
