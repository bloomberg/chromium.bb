// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCELERATED_WIDGET_MAC_SURFACE_HANDLE_TYPES_H_
#define UI_ACCELERATED_WIDGET_MAC_SURFACE_HANDLE_TYPES_H_

#include <IOSurface/IOSurface.h>
#include <OpenGL/CGLIOSurface.h>

#include "base/basictypes.h"
#include "ui/accelerated_widget_mac/accelerated_widget_mac_export.h"
#include "ui/base/cocoa/remote_layer_api.h"

namespace ui {

// The surface handle passed between the GPU and browser process may refer to
// an IOSurface or a CAContext. These helper functions must be used to identify
// and translate between the types.
enum SurfaceHandleType {
  kSurfaceHandleTypeInvalid,
  kSurfaceHandleTypeIOSurface,
  kSurfaceHandleTypeCAContext,
};

ACCELERATED_WIDGET_MAC_EXPORT
SurfaceHandleType GetSurfaceHandleType(uint64 surface_handle);

ACCELERATED_WIDGET_MAC_EXPORT
CAContextID CAContextIDFromSurfaceHandle(uint64 surface_handle);

ACCELERATED_WIDGET_MAC_EXPORT
IOSurfaceID IOSurfaceIDFromSurfaceHandle(uint64 surface_handle);

ACCELERATED_WIDGET_MAC_EXPORT
uint64 SurfaceHandleFromIOSurfaceID(IOSurfaceID io_surface_id);

ACCELERATED_WIDGET_MAC_EXPORT
uint64 SurfaceHandleFromCAContextID(CAContextID ca_context_id);

}  //  namespace ui

#endif  // UI_ACCELERATED_WIDGET_MAC_SURFACE_HANDLE_TYPES_H_
