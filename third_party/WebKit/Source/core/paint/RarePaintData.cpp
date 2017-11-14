// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/RarePaintData.h"

#include "core/paint/PaintLayer.h"

namespace blink {

ObjectPaintProperties& RarePaintData::EnsurePaintProperties() {
  if (!paint_properties_)
    paint_properties_ = ObjectPaintProperties::Create();
  return *paint_properties_.get();
}

void RarePaintData::ClearPaintProperties() {
  paint_properties_.reset(nullptr);
}

void RarePaintData::ClearLocalBorderBoxProperties() {
  local_border_box_properties_ = nullptr;
}

void RarePaintData::SetLocalBorderBoxProperties(PropertyTreeState& state) {
  if (!local_border_box_properties_)
    local_border_box_properties_ = std::make_unique<PropertyTreeState>(state);
  else
    *local_border_box_properties_ = state;
}

const TransformPaintPropertyNode* RarePaintData::GetPreTransform() const {
  if (!paint_properties_)
    return local_border_box_properties_->Transform();
  if (paint_properties_->Transform())
    return paint_properties_->Transform()->Parent();
  return local_border_box_properties_->Transform();
}

const TransformPaintPropertyNode* RarePaintData::GetPostScrollTranslation()
    const {
  if (!paint_properties_)
    return local_border_box_properties_->Transform();
  if (paint_properties_->ScrollTranslation())
    return paint_properties_->ScrollTranslation();
  if (paint_properties_->Perspective())
    return paint_properties_->Perspective();
  return local_border_box_properties_->Transform();
}

const ClipPaintPropertyNode* RarePaintData::GetPreCssClip() const {
  if (!paint_properties_)
    return local_border_box_properties_->Clip();
  if (paint_properties_->CssClip())
    return paint_properties_->CssClip()->Parent();
  return local_border_box_properties_->Clip();
}

const ClipPaintPropertyNode* RarePaintData::GetPostOverflowClip() const {
  if (!paint_properties_)
    return local_border_box_properties_->Clip();
  if (paint_properties_->OverflowClip())
    return paint_properties_->OverflowClip();
  return local_border_box_properties_->Clip();
}

const EffectPaintPropertyNode* RarePaintData::GetPreEffect() const {
  if (!paint_properties_)
    return local_border_box_properties_->Effect();
  if (paint_properties_->Effect())
    return paint_properties_->Effect()->Parent();
  if (paint_properties_->Filter())
    return paint_properties_->Filter()->Parent();
  return local_border_box_properties_->Effect();
}

PropertyTreeState RarePaintData::PreEffectProperties() const {
  DCHECK(local_border_box_properties_);
  return PropertyTreeState(GetPreTransform(), GetPreCssClip(), GetPreEffect());
}

PropertyTreeState RarePaintData::ContentsProperties() const {
  DCHECK(local_border_box_properties_);
  PropertyTreeState contents(*local_border_box_properties_);
  if (auto* properties = PaintProperties()) {
    if (properties->ScrollTranslation())
      contents.SetTransform(properties->ScrollTranslation());
    if (properties->OverflowClip())
      contents.SetClip(properties->OverflowClip());
    else if (properties->CssClip())
      contents.SetClip(properties->CssClip());
  }

  // TODO(chrishtr): cssClipFixedPosition needs to be handled somehow.

  return contents;
}

RarePaintData::RarePaintData(const LayoutPoint& location_in_backing)
    : unique_id_(NewUniqueObjectId()),
      location_in_backing_(location_in_backing) {}

RarePaintData::~RarePaintData() {}

void RarePaintData::SetLayer(std::unique_ptr<PaintLayer> layer) {
  layer_ = std::move(layer);
};

}  // namespace blink
