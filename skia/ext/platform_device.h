// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SKIA_EXT_PLATFORM_DEVICE_H_
#define SKIA_EXT_PLATFORM_DEVICE_H_
#pragma once

// This file provides an easy way to include the appropriate PlatformDevice
// header file for your platform.

#if defined(WIN32)
#include <windows.h>
#endif

#include "third_party/skia/include/core/SkPreConfig.h"
#include "third_party/skia/include/core/SkColor.h"


class SkDevice;
struct SkIRect;

#if defined(__linux__) || defined(__FreeBSD__) || defined(__OpenBSD__)
typedef struct _cairo cairo_t;
typedef struct _cairo_rectangle cairo_rectangle_t;
#elif defined(__APPLE__)
typedef struct CGContext* CGContextRef;
typedef struct CGRect CGRect;
#endif

namespace skia {

class PlatformDevice;

#if defined(WIN32)
typedef HDC PlatformSurface;
typedef RECT PlatformRect;
#elif defined(__linux__) || defined(__FreeBSD__) || defined(__OpenBSD__)
typedef cairo_t* PlatformSurface;
typedef cairo_rectangle_t PlatformRect;
#elif defined(__APPLE__)
typedef CGContextRef PlatformSurface;
typedef CGRect PlatformRect;
#endif

// The following routines provide accessor points for the functionality
// exported by the various PlatformDevice ports.  The PlatformDevice, and
// BitmapPlatformDevice classes inherit directly from SkDevice, which is no
// longer a supported usage-pattern for skia.  In preparation of the removal of
// these classes, all calls to PlatformDevice::* should be routed through these
// helper functions.

// Bind a PlatformDevice instance, |platform_device| to |device|.  Subsequent
// calls to the functions exported below will forward the request to the
// corresponding method on the bound PlatformDevice instance.    If no
// PlatformDevice has been bound to the SkDevice passed, then the routines are
// NOPS.
SK_API void SetPlatformDevice(SkDevice* device,
                              PlatformDevice* platform_device);
SK_API PlatformDevice* GetPlatformDevice(SkDevice* device);

// Returns if the preferred rendering engine is vectorial or bitmap based.
// Forwards to PlatformDevice::IsVectorial, if a PlatformDevice is bound,
// otherwise falls-back to the SkDevice::getDeviceCapabilities routine.
SK_API bool IsVectorial(SkDevice* device);

// Returns if the native font rendering engine is allowed to render text to
// this device.
SK_API bool IsNativeFontRenderingAllowed(SkDevice* device);

// Returns the PlatformSurface used for native rendering into the device.
SK_API PlatformSurface BeginPlatformPaint(SkDevice* device);

// Finish a previous call to BeginPlatformPaint.
SK_API void EndPlatformPaint(SkDevice* device);

// Draws to the given PlatformSurface, |context|. Forwards to the
// PlatformDevice bound to |device|.  Otherwise is a NOP.
SK_API void DrawToNativeContext(SkDevice* device, PlatformSurface context,
                                int x, int y, const PlatformRect* src_rect);

// Sets the opacity of each pixel in the specified region to be opaque.
SK_API void MakeOpaque(SkDevice* device, int x, int y, int width, int height);

}  // namespace skia

#if defined(WIN32)
#include "skia/ext/platform_device_win.h"
#elif defined(__APPLE__)
#include "skia/ext/platform_device_mac.h"
#elif defined(__linux__) || defined(__FreeBSD__) || defined(__OpenBSD__) || \
      defined(__Solaris__)
#include "skia/ext/platform_device_linux.h"
#endif

#endif
