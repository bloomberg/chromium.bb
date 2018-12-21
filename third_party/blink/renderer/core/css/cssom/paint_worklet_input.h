// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSSOM_PAINT_WORKLET_INPUT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSSOM_PAINT_WORKLET_INPUT_H_

#include <memory>
#include "cc/paint/paint_worklet_input.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/cssom/paint_worklet_style_property_map.h"
#include "third_party/blink/renderer/platform/geometry/float_size.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"

namespace blink {

class CORE_EXPORT PaintWorkletInput : public cc::PaintWorkletInput {
 public:
  PaintWorkletInput(const std::string& name,
                    const FloatSize& container_size,
                    float effective_zoom,
                    const Document&,
                    const ComputedStyle&,
                    Node* styled_node,
                    const Vector<CSSPropertyID>& native_properties,
                    const Vector<AtomicString>& custom_properties);

  ~PaintWorkletInput() override = default;

  // PaintWorkletInput implementation
  gfx::SizeF GetSize() const override {
    return gfx::SizeF(container_size_.Width(), container_size_.Height());
  }

  const std::string& Name() const { return name_; }
  const FloatSize& ContainerSize() const { return container_size_; }
  float EffectiveZoom() const { return effective_zoom_; }
  PaintWorkletStylePropertyMap* StyleMap() { return style_map_.Get(); }

 private:
  const std::string name_;
  const FloatSize container_size_;
  const float effective_zoom_;
  const CrossThreadPersistent<PaintWorkletStylePropertyMap> style_map_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSSOM_PAINT_WORKLET_INPUT_H_
