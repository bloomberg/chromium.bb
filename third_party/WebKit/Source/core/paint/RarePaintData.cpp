// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/RarePaintData.h"

#include "core/paint/FragmentData.h"
#include "core/paint/PaintLayer.h"

namespace blink {

RarePaintData::RarePaintData() : unique_id_(NewUniqueObjectId()) {}

RarePaintData::~RarePaintData() {}

void RarePaintData::SetLayer(std::unique_ptr<PaintLayer> layer) {
  layer_ = std::move(layer);
};

FragmentData& RarePaintData::EnsureFragment() {
  if (!fragment_data_)
    fragment_data_ = FragmentData::Create();
  return *fragment_data_.get();
}

}  // namespace blink
