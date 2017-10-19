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

void FragmentData::ClearLocalBorderBoxProperties() {
  local_border_box_properties_ = nullptr;
}

void FragmentData::SetLocalBorderBoxProperties(PropertyTreeState& state) {
  if (!local_border_box_properties_)
    local_border_box_properties_ = WTF::MakeUnique<PropertyTreeState>(state);
  else
    *local_border_box_properties_ = state;
}

FragmentData& FragmentData::EnsureNextFragment() {
  if (!next_fragment_)
    next_fragment_ = FragmentData::Create();
  return *next_fragment_.get();
}

const TransformPaintPropertyNode* FragmentData::GetPreTransform() const {
  if (!paint_properties_)
    return local_border_box_properties_->Transform();
  if (paint_properties_->Transform())
    return paint_properties_->Transform()->Parent();
  return local_border_box_properties_->Transform();
}

const TransformPaintPropertyNode* FragmentData::GetPostScrollTranslation()
    const {
  if (!paint_properties_)
    return local_border_box_properties_->Transform();
  if (paint_properties_->ScrollTranslation())
    return paint_properties_->ScrollTranslation();
  if (paint_properties_->Perspective())
    return paint_properties_->Perspective();
  return local_border_box_properties_->Transform();
}

const ClipPaintPropertyNode* FragmentData::GetPreCssClip() const {
  if (!paint_properties_)
    return local_border_box_properties_->Clip();
  if (paint_properties_->CssClip())
    return paint_properties_->CssClip()->Parent();
  return local_border_box_properties_->Clip();
}

const ClipPaintPropertyNode* FragmentData::GetPostOverflowClip() const {
  if (!paint_properties_)
    return local_border_box_properties_->Clip();
  if (paint_properties_->OverflowClip())
    return paint_properties_->OverflowClip();
  return local_border_box_properties_->Clip();
}

const EffectPaintPropertyNode* FragmentData::GetPreEffect() const {
  if (!paint_properties_)
    return local_border_box_properties_->Effect();
  if (paint_properties_->Effect())
    return paint_properties_->Effect()->Parent();
  if (paint_properties_->Filter())
    return paint_properties_->Filter()->Parent();
  return local_border_box_properties_->Effect();
}

PropertyTreeState FragmentData::PreEffectProperties() const {
  DCHECK(local_border_box_properties_);
  return PropertyTreeState(GetPreTransform(), GetPreCssClip(), GetPreEffect());
}

PropertyTreeState FragmentData::ContentsProperties() const {
  DCHECK(local_border_box_properties_);
  return PropertyTreeState(GetPostScrollTranslation(), GetPostOverflowClip(),
                           local_border_box_properties_->Effect());
}

}  // namespace blink
