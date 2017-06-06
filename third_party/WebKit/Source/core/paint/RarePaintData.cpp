// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/RarePaintData.h"

#include "core/paint/FragmentData.h"
#include "core/paint/PaintLayer.h"

namespace blink {

RarePaintData::RarePaintData() {
  static LayoutObjectId layout_object_id_counter = 0;
  unique_id_ = ++layout_object_id_counter;
}

RarePaintData::~RarePaintData() {}

void RarePaintData::SetLayer(std::unique_ptr<PaintLayer> layer) {
  layer_ = std::move(layer);
};

FragmentData& RarePaintData::EnsureFragment() {
  if (!fragment_data_)
    fragment_data_ = FragmentData::Create();
  return *fragment_data_.get();
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
  if (fragment_data_) {
    if (auto* properties = fragment_data_->PaintProperties()) {
      if (properties->ScrollTranslation())
        contents.SetTransform(properties->ScrollTranslation());
      if (properties->OverflowClip())
        contents.SetClip(properties->OverflowClip());
      else if (properties->CssClip())
        contents.SetClip(properties->CssClip());
    }
  }

  // TODO(chrishtr): cssClipFixedPosition needs to be handled somehow.

  return contents;
}

}  // namespace blink
