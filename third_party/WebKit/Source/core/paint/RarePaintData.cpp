// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/RarePaintData.h"

#include "core/paint/ObjectPaintProperties.h"
#include "core/paint/PaintLayer.h"

namespace blink {

RarePaintData::RarePaintData() {}

RarePaintData::~RarePaintData() {}

void RarePaintData::SetLayer(std::unique_ptr<PaintLayer> layer) {
  layer_ = std::move(layer);
};

ObjectPaintProperties& RarePaintData::EnsurePaintProperties() {
  if (!paint_properties_)
    paint_properties_ = ObjectPaintProperties::Create();
  return *paint_properties_.get();
}

void RarePaintData::ClearLocalBorderBoxProperties() {
  local_border_box_properties_ = nullptr;
}

void RarePaintData::SetLocalBorderBoxProperties(PropertyTreeState& state) {
  if (!local_border_box_properties_)
    local_border_box_properties_ = WTF::MakeUnique<PropertyTreeState>(state);
  else
    *local_border_box_properties_ = state;
}

PropertyTreeState RarePaintData::ContentsProperties() const {
  DCHECK(local_border_box_properties_);
  PropertyTreeState contents(*local_border_box_properties_);
  if (paint_properties_) {
    if (paint_properties_->ScrollTranslation())
      contents.SetTransform(paint_properties_->ScrollTranslation());
    if (paint_properties_->OverflowClip())
      contents.SetClip(paint_properties_->OverflowClip());
    else if (paint_properties_->CssClip())
      contents.SetClip(paint_properties_->CssClip());
  }

  // TODO(chrishtr): cssClipFixedPosition needs to be handled somehow.

  return contents;
}

}  // namespace blink
