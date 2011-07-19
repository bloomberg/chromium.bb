// Copyright (c) 2011 The Chromium Authors. All rights reserved.
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

#if defined(__linux__) || defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__sun)
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
#elif defined(__linux__) || defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__sun)
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

}  // namespace skia

#if defined(WIN32)
#include "skia/ext/platform_device_win.h"
#elif defined(__APPLE__)
#include "skia/ext/platform_device_mac.h"
#elif defined(__linux__) || defined(__FreeBSD__) || defined(__OpenBSD__) || \
      defined(__sun)
#include "skia/ext/platform_device_linux.h"
#endif

#endif
