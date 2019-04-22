// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_TEST_PAINT_WORKLET_INPUT_H_
#define CC_TEST_TEST_PAINT_WORKLET_INPUT_H_

#include "cc/paint/paint_worklet_input.h"

namespace cc {

class TestPaintWorkletInput : public PaintWorkletInput {
 public:
  explicit TestPaintWorkletInput(const gfx::SizeF& size)
      : container_size_(size) {}

  gfx::SizeF GetSize() const override;
  int WorkletId() const override;

 protected:
  ~TestPaintWorkletInput() override = default;

 private:
  gfx::SizeF container_size_;
};

}  // namespace cc

#endif  // CC_TEST_TEST_PAINT_WORKLET_INPUT_H_
