// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/skottie_wrapper.h"

namespace cc {

void SkottieWrapper::Seek(float t) {
  Seek(t, FrameDataCallback());
}

void SkottieWrapper::Draw(SkCanvas* canvas, float t, const SkRect& rect) {
  Draw(canvas, t, rect, FrameDataCallback());
}

}  // namespace cc
