// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdlib.h>

#include "remoting/client/pepper/fake_browser.h"

// Constant value for browser window.
static const int kWindowWidth = 100;
static const int kWindowHeight = 100;

// ----------------------------------------------------------------------------
// Browser callback routines
// These are simple implementations of the routines that the browser provides
// to the plugin as callbacks.
// ----------------------------------------------------------------------------

// Handler for getvalue.
// Must be of type NPN_GetValueProcPtr.
NPError Browser_GetValue(NPP instance, NPNVariable variable, void *ret_value) {
  if (variable == NPNVPepperExtensions) {
    NPNExtensions** ret = static_cast<NPNExtensions**>(ret_value);
    *ret = Singleton<FakeBrowser>()->GetExtensions();
    return NPERR_NO_ERROR;
  }
  return NPERR_GENERIC_ERROR;
}

// Extension handler for acquireDevice.
// Must be of type NPAcquireDevicePtr.
NPDevice* Extension_AcquireDevice(NPP instance, NPDeviceID device) {
  if (device == NPPepper2DDevice) {
    return Singleton<FakeBrowser>()->GetDevice2d();
  }
  // TODO(garykac): Add support for NPPepper3DDevice.
  return NULL;
}

// Initialize 2D device context.
NPError Device_InitializeContext2D(NPP instance,
                                   NPDeviceContext2D* context,
                                   int extra_bytes) {
  FakeBrowser* browser = Singleton<FakeBrowser>::get();
  NPDeviceContext2D* context2d = static_cast<NPDeviceContext2D*>(context);
  int width, height;
  browser->GetWindowInfo(&width, &height);
  int stride = (width * ARGB_PIXEL_SIZE) + extra_bytes;
  context2d->region = browser->AllocPixelBuffer(stride);
  context2d->stride = stride;

  return NPERR_NO_ERROR;
}

// Device handler for initializeContext
// This initializes a 2D context where the stride == width.
// Must be of type NPDeviceInitializeContextPtr.
NPError Device_InitializeContext2D_NoExtraBytes(NPP instance,
                                                const NPDeviceConfig* config,
                                                NPDeviceContext* context) {
  return Device_InitializeContext2D(instance,
                                    static_cast<NPDeviceContext2D*>(context),
                                    0);
}

// Device handler for initializeContext
// This initializes a 2D context where the stride > width.
// Must be of type NPDeviceInitializeContextPtr.
NPError Device_InitializeContext2D_ExtraBytes(NPP instance,
                                              const NPDeviceConfig* config,
                                              NPDeviceContext* context) {
  return Device_InitializeContext2D(instance,
                                    static_cast<NPDeviceContext2D*>(context),
                                    8 /* extra_bytes */);
}

// Device handler for flushContext
// Must be of type NPDeviceFlushContextPtr.
NPError Device_FlushContext(NPP instance, NPDeviceContext* context,
                            NPDeviceFlushContextCallbackPtr callback,
                            void* userData) {
  return NPERR_NO_ERROR;
}

// ----------------------------------------------------------------------------
// FakeBrowserFuncs
// Singleton class for creating/managing the NPNetscapeFuncs struct that we
// need to provide to the Pepper plugin.
// ----------------------------------------------------------------------------

FakeBrowser::FakeBrowser() {
  // Setup fake versions of the browser funcs needed by the unit tests.
  // There are dozens of browser funcs that can be set up, but we only worry
  // about the ones needed for our unittests.
  browser_funcs_.reset(new NPNetscapeFuncs());
  browser_funcs_->getvalue = &Browser_GetValue;

  // Setup fake extension funcs structure.
  extensions_.reset(new NPNExtensions());
  extensions_->acquireDevice = &Extension_AcquireDevice;

  // Setup fake device funcs structure.
  device2d_.reset(new NPDevice());
  device2d_->initializeContext = &Device_InitializeContext2D_NoExtraBytes;
  device2d_->flushContext = &Device_FlushContext;

  // Fake browser window.
  window_.reset(new NPWindow());
  window_->x = 0;
  window_->y = 0;
  window_->width = kWindowWidth;
  window_->height = kWindowHeight;

  width_ = kWindowWidth;
  height_ = kWindowHeight;

  stride_ = 0;
  pixel_buffer_.reset();
}

FakeBrowser::~FakeBrowser() {
  FreePixelBuffer();
}

// Normally in our tests, the stride (ie, the number of bytes between the
// start of a row and the start of the next row) is equal to the number of
// bytes used to store the pixels for the row.
// Passing true to this routine sets things up so that there are a few extra
// padding bytes to the end of each row so that the stride is not the same
// as the row width.
void FakeBrowser::ForceStrideInDeviceContext(bool extra_bytes) {
  if (extra_bytes) {
    device2d_->initializeContext = &Device_InitializeContext2D_ExtraBytes;
  } else {
    device2d_->initializeContext = &Device_InitializeContext2D_NoExtraBytes;
  }
}

// Allocate a pixel buffer for the plugin to use.
// The height and width of the buffer come from the window size.
// The stride value is used to force each row to be |stride| bytes in size.
// This is typically done to add extra padding bytes to the end of each row.
uint32* FakeBrowser::AllocPixelBuffer(int stride) {
  // Don't allow the stride to be less than the window width.
  if (stride < width_) {
    stride = width_;
  }
  stride_ = stride;
  pixel_buffer_.reset(new uint32[height_ * stride]);

  return pixel_buffer_.get();
}

void FakeBrowser::FreePixelBuffer() {
  stride_ = 0;
  pixel_buffer_.reset();
}
