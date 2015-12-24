// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accelerated_widget_mac/surface_handle_types.h"

#include "base/logging.h"

namespace ui {
namespace {

// The type of the handle is stored in the upper 64 bits.
const uint64_t kTypeMask = 0xFFFFFFFFull << 32;

const uint64_t kTypeIOSurface = 0x01010101ull << 32;
const uint64_t kTypeCAContext = 0x02020202ull << 32;

// To make it a bit less likely that we'll just cast off the top bits of the
// handle to get the ID, XOR lower bits with a type-specific mask.
const uint32_t kXORMaskIOSurface = 0x01010101;
const uint32_t kXORMaskCAContext = 0x02020202;

}  // namespace

SurfaceHandleType GetSurfaceHandleType(uint64_t surface_handle) {
  switch(surface_handle & kTypeMask) {
    case kTypeIOSurface:
      return kSurfaceHandleTypeIOSurface;
    case kTypeCAContext:
      return kSurfaceHandleTypeCAContext;
  }
  return kSurfaceHandleTypeInvalid;
}

IOSurfaceID IOSurfaceIDFromSurfaceHandle(uint64_t surface_handle) {
  DCHECK_EQ(kSurfaceHandleTypeIOSurface, GetSurfaceHandleType(surface_handle));
  return static_cast<uint32_t>(surface_handle) ^ kXORMaskIOSurface;
}

CAContextID CAContextIDFromSurfaceHandle(uint64_t surface_handle) {
  DCHECK_EQ(kSurfaceHandleTypeCAContext, GetSurfaceHandleType(surface_handle));
  return static_cast<uint32_t>(surface_handle) ^ kXORMaskCAContext;
}

uint64_t SurfaceHandleFromIOSurfaceID(IOSurfaceID io_surface_id) {
  return kTypeIOSurface | (io_surface_id ^ kXORMaskIOSurface);
}

uint64_t SurfaceHandleFromCAContextID(CAContextID ca_context_id) {
  return kTypeCAContext | (ca_context_id ^ kXORMaskCAContext);
}

}  //  namespace ui
