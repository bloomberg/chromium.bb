// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "third_party/blink/renderer/core/css/cssom/paint_worklet_input.h"

namespace blink {

PaintWorkletInput::PaintWorkletInput(
    const String& name,
    const FloatSize& container_size,
    float effective_zoom,
    int worklet_id,
    PaintWorkletStylePropertyMap::CrossThreadData data)
    : name_(name.IsolatedCopy()),
      container_size_(container_size),
      effective_zoom_(effective_zoom),
      worklet_id_(worklet_id),
      style_map_data_(std::move(data)) {}

}  // namespace blink
