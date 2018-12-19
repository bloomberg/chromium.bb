// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PLATFORM_PAINT_WORKLET_INPUT_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PLATFORM_PAINT_WORKLET_INPUT_H_

#include "cc/paint/paint_worklet_input.h"

namespace blink {

class PLATFORM_EXPORT PlatformPaintWorkletInput : public cc::PaintWorkletInput {
 public:
  PlatformPaintWorkletInput(const std::string& name,
                            const FloatSize& container_size,
                            float effective_zoom)
      : name_(name),
        container_size_(container_size),
        effective_zoom_(effective_zoom) {}

  ~PlatformPaintWorkletInput() override = default;

  // PaintWorkletInput implementation
  gfx::SizeF GetSize() const override {
    return gfx::SizeF(container_size_.Width(), container_size_.Height());
  }

  const std::string& Name() const { return name_; }
  const FloatSize& ContainerSize() const { return container_size_; }
  float EffectiveZoom() const { return effective_zoom_; }

 private:
  std::string name_;
  FloatSize container_size_;
  float effective_zoom_;
  // TODO(crbug.com/895579): add a cross thread style map.
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHIC_PLATFORM_PAINT_WORKLET_INPUT_H_
