// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/FragmentData.h"
#include "core/paint/PaintLayer.h"

namespace blink {

// These are defined here because of PaintLayer dependency.

FragmentData::RareData::RareData(const LayoutPoint& location_in_backing)
    : unique_id(NewUniqueObjectId()),
      location_in_backing(location_in_backing) {}

FragmentData::RareData::~RareData() = default;

FragmentData& FragmentData::EnsureNextFragment() {
  if (!next_fragment_)
    next_fragment_ = std::make_unique<FragmentData>();
  return *next_fragment_.get();
}

FragmentData::RareData& FragmentData::EnsureRareData() {
  if (!rare_data_)
    rare_data_ = std::make_unique<RareData>(visual_rect_.Location());
  return *rare_data_;
}

void FragmentData::SetLayer(std::unique_ptr<PaintLayer> layer) {
  if (rare_data_ || layer)
    EnsureRareData().layer = std::move(layer);
}

const TransformPaintPropertyNode* FragmentData::PreTransform() const {
  if (const auto* properties = PaintProperties()) {
    if (properties->Transform())
      return properties->Transform()->Parent();
  }
  DCHECK(LocalBorderBoxProperties());
  return LocalBorderBoxProperties()->Transform();
}

const TransformPaintPropertyNode* FragmentData::PostScrollTranslation() const {
  if (const auto* properties = PaintProperties()) {
    if (properties->ScrollTranslation())
      return properties->ScrollTranslation();
    if (properties->Perspective())
      return properties->Perspective();
  }
  DCHECK(LocalBorderBoxProperties());
  return LocalBorderBoxProperties()->Transform();
}

const ClipPaintPropertyNode* FragmentData::PreCssClip() const {
  if (const auto* properties = PaintProperties()) {
    if (properties->CssClip())
      return properties->CssClip()->Parent();
  }
  DCHECK(LocalBorderBoxProperties());
  return LocalBorderBoxProperties()->Clip();
}

const ClipPaintPropertyNode* FragmentData::PostOverflowClip() const {
  if (const auto* properties = PaintProperties()) {
    if (properties->OverflowClip())
      return properties->OverflowClip();
    if (properties->InnerBorderRadiusClip())
      return properties->InnerBorderRadiusClip();
  }
  DCHECK(LocalBorderBoxProperties());
  return LocalBorderBoxProperties()->Clip();
}

const EffectPaintPropertyNode* FragmentData::PreEffect() const {
  if (const auto* properties = PaintProperties()) {
    if (properties->Effect())
      return properties->Effect()->Parent();
    if (properties->Filter())
      return properties->Filter()->Parent();
  }
  DCHECK(LocalBorderBoxProperties());
  return LocalBorderBoxProperties()->Effect();
}

const EffectPaintPropertyNode* FragmentData::PreFilter() const {
  if (const auto* properties = PaintProperties()) {
    if (properties->Filter())
      return properties->Filter()->Parent();
  }
  DCHECK(LocalBorderBoxProperties());
  return LocalBorderBoxProperties()->Effect();
}

}  // namespace blink
