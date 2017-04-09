// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/RarePaintData.h"

#include "core/paint/ObjectPaintProperties.h"

namespace blink {

RarePaintData::RarePaintData() {}

RarePaintData::~RarePaintData() {}

ObjectPaintProperties& RarePaintData::EnsurePaintProperties() {
  if (!paint_properties_)
    paint_properties_ = ObjectPaintProperties::Create();
  return *paint_properties_.get();
}

void RarePaintData::ClearLocalBorderBoxProperties() {
  local_border_box_properties_ = nullptr;

  // The contents properties are based on the border box so we need to clear
  // the cached value.
  contents_properties_ = nullptr;
}

void RarePaintData::SetLocalBorderBoxProperties(PropertyTreeState& state) {
  if (!local_border_box_properties_)
    local_border_box_properties_ = WTF::MakeUnique<PropertyTreeState>(state);
  else
    *local_border_box_properties_ = state;

  // The contents properties are based on the border box so we need to clear
  // the cached value.
  contents_properties_ = nullptr;
}

static std::unique_ptr<PropertyTreeState> ComputeContentsProperties(
    PropertyTreeState* local_border_box_properties,
    ObjectPaintProperties* paint_properties) {
  if (!local_border_box_properties)
    return nullptr;

  std::unique_ptr<PropertyTreeState> contents =
      WTF::MakeUnique<PropertyTreeState>(*local_border_box_properties);

  if (paint_properties) {
    if (paint_properties->ScrollTranslation())
      contents->SetTransform(paint_properties->ScrollTranslation());
    if (paint_properties->OverflowClip())
      contents->SetClip(paint_properties->OverflowClip());
    else if (paint_properties->CssClip())
      contents->SetClip(paint_properties->CssClip());
  }

  // TODO(chrishtr): cssClipFixedPosition needs to be handled somehow.

  return contents;
}

const PropertyTreeState* RarePaintData::ContentsProperties() const {
  if (!contents_properties_) {
    if (local_border_box_properties_) {
      contents_properties_ = ComputeContentsProperties(
          local_border_box_properties_.get(), paint_properties_.get());
    }
  } else {
#if DCHECK_IS_ON()
    // Check that the cached contents properties are valid by checking that they
    // do not change if recalculated.
    DCHECK(local_border_box_properties_);
    std::unique_ptr<PropertyTreeState> old_properties =
        std::move(contents_properties_);
    contents_properties_ = ComputeContentsProperties(
        local_border_box_properties_.get(), paint_properties_.get());
    DCHECK(*contents_properties_ == *old_properties);
#endif
  }
  return contents_properties_.get();
}

}  // namespace blink
