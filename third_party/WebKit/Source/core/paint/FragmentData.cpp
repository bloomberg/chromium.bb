// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/FragmentData.h"
#include "core/paint/ObjectPaintProperties.h"

namespace blink {

FragmentData& FragmentData::EnsureNextFragment() {
  if (!next_fragment_)
    next_fragment_ = FragmentData::Create();
  return *next_fragment_.get();
}

RarePaintData& FragmentData::EnsureRarePaintData() {
  if (!rare_paint_data_)
    rare_paint_data_ = std::make_unique<RarePaintData>(visual_rect_.Location());
  return *rare_paint_data_.get();
}

void FragmentData::SetLocationInBacking(const LayoutPoint& point) {
  if (rare_paint_data_ || point != VisualRect().Location())
    EnsureRarePaintData().SetLocationInBacking(point);
}

}  // namespace blink
