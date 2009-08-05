// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "skia/ext/vector_canvas.h"

namespace skia {

VectorCanvas::VectorCanvas() {
}

VectorCanvas::~VectorCanvas() {
}

SkBounder* VectorCanvas::setBounder(SkBounder* bounder) {
  if (!IsTopDeviceVectorial())
    return PlatformCanvas::setBounder(bounder);

  // This function isn't used in the code. Verify this assumption.
  SkASSERT(false);
  return NULL;
}

SkDrawFilter* VectorCanvas::setDrawFilter(SkDrawFilter* filter) {
  // This function isn't used in the code. Verify this assumption.
  SkASSERT(false);
  return NULL;
}

bool VectorCanvas::IsTopDeviceVectorial() const {
  return getTopPlatformDevice().IsVectorial();
}

}  // namespace skia

