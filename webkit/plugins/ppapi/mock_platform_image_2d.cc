// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/ppapi/mock_platform_image_2d.h"

#include "skia/ext/platform_canvas.h"

namespace webkit {
namespace ppapi {

MockPlatformImage2D::MockPlatformImage2D(int width, int height)
  : width_(width),
    height_(height) {
}

MockPlatformImage2D::~MockPlatformImage2D() {
}

skia::PlatformCanvas* MockPlatformImage2D::Map() {
  return skia::CreatePlatformCanvas(width_, height_, true);
}

intptr_t MockPlatformImage2D::GetSharedMemoryHandle(uint32* byte_count) const {
  *byte_count = 0;
  return 0;
}

TransportDIB* MockPlatformImage2D::GetTransportDIB() const {
  return NULL;
}

}  // namespace ppapi
}  // namespace webkit
