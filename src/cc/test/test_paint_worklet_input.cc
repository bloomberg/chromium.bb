// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/test_paint_worklet_input.h"

namespace cc {

gfx::SizeF TestPaintWorkletInput::GetSize() const {
  return container_size_;
}

int TestPaintWorkletInput::WorkletId() const {
  return 1u;
}

}  // namespace cc
