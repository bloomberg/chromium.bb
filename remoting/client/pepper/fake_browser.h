// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_PEPPER_FAKE_BROWSER_H_
#define REMOTING_CLIENT_PEPPER_FAKE_BROWSER_H_

#include "base/scoped_ptr.h"
#include "base/singleton.h"
#include "third_party/npapi/bindings/nphostapi.h"
#include "third_party/npapi/bindings/npapi_extensions.h"

// Each ARGB pixel is stored in a 4-byte uint32.
#define ARGB_PIXEL_SIZE 4

class FakeBrowser {
 public:
  NPNetscapeFuncs* GetBrowserFuncs() { return browser_funcs_.get(); }
  NPNExtensions* GetExtensions() { return extensions_.get(); }

  NPDevice* GetDevice2d() { return device2d_.get(); }
  void ForceStrideInDeviceContext(bool forceStride);

  NPWindow* GetWindow() { return window_.get(); }
  void GetWindowInfo(int* width, int* height) {
    *width = width_;
    *height = height_;
  }

  uint32* AllocPixelBuffer(int stride);
  void FreePixelBuffer();

  uint32* GetPixelBuffer() { return pixel_buffer_.get(); }
  int GetPixelBufferStride() { return stride_; }

 private:
  // Singleton private bits.
  friend struct DefaultSingletonTraits<FakeBrowser>;
  FakeBrowser();
  virtual ~FakeBrowser();

  // Browser callback functions.
  scoped_ptr<NPNetscapeFuncs> browser_funcs_;

  // Browser extension callbacks.
  scoped_ptr<NPNExtensions> extensions_;

  // The rendering device (provided by the browser to the plugin).
  scoped_ptr<NPDevice> device2d_;

  // Window (provided by the browser to the plugin).
  scoped_ptr<NPWindow> window_;

  // Pixel buffer to store the device2d_ (and window_) pixels.
  scoped_array<uint32> pixel_buffer_;
  int width_, height_, stride_;

  DISALLOW_COPY_AND_ASSIGN(FakeBrowser);
};

#endif  // REMOTING_CLIENT_PEPPER_FAKE_BROWSER_H_
