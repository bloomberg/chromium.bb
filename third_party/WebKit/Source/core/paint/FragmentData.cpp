// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/FragmentData.h"
#include "core/paint/ObjectPaintProperties.h"

namespace blink {

ObjectPaintProperties& FragmentData::EnsurePaintProperties() {
  if (!paint_properties_)
    paint_properties_ = ObjectPaintProperties::Create();
  return *paint_properties_.get();
}

void FragmentData::ClearPaintProperties() {
  paint_properties_.reset(nullptr);
}

}  // namespace blink
