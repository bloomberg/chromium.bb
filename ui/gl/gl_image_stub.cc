// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_image_stub.h"

#include <GL/gl.h>

namespace gfx {

GLImageStub::GLImageStub() {}

GLImageStub::~GLImageStub() {}

Size GLImageStub::GetSize() {
  return Size(1, 1);
}

unsigned GLImageStub::GetInternalFormat() { return GL_RGBA; }

bool GLImageStub::BindTexImage(unsigned target) { return true; }

bool GLImageStub::CopyTexImage(unsigned target) {
  return true;
}

bool GLImageStub::CopyTexSubImage(unsigned target,
                                  const Point& offset,
                                  const Rect& rect) {
  return true;
}

bool GLImageStub::ScheduleOverlayPlane(AcceleratedWidget widget,
                                       int z_order,
                                       OverlayTransform transform,
                                       const Rect& bounds_rect,
                                       const RectF& crop_rect) {
  return false;
}

}  // namespace gfx
