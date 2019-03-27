// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "third_party/blink/renderer/core/css/cssom/paint_worklet_input.h"

namespace blink {

PaintWorkletInput::PaintWorkletInput(
    const std::string& name,
    const FloatSize& container_size,
    float effective_zoom,
    PaintWorkletStylePropertyMap::CrossThreadData data)
    : name_(name),
      container_size_(container_size),
      effective_zoom_(effective_zoom),
      style_map_data_(std::move(data)) {}

}  // namespace blink
