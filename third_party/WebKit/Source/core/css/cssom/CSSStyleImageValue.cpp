// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/cssom/CSSStyleImageValue.h"

namespace blink {

double CSSStyleImageValue::intrinsicWidth(bool& is_null) const {
  is_null = IsCachePending();
  if (is_null)
    return 0;
  return ImageLayoutSize().Width().ToDouble();
}

double CSSStyleImageValue::intrinsicHeight(bool& is_null) const {
  is_null = IsCachePending();
  if (is_null)
    return 0;
  return ImageLayoutSize().Height().ToDouble();
}

double CSSStyleImageValue::intrinsicRatio(bool& is_null) {
  is_null = IsCachePending();
  if (is_null || intrinsicHeight(is_null) == 0) {
    is_null = true;
    return 0;
  }
  return intrinsicWidth(is_null) / intrinsicHeight(is_null);
}

FloatSize CSSStyleImageValue::ElementSize(
    const FloatSize& default_object_size) const {
  bool not_used;
  return FloatSize(intrinsicWidth(not_used), intrinsicHeight(not_used));
}

bool CSSStyleImageValue::IsAccelerated() const {
  return GetImage() && GetImage()->IsTextureBacked();
}

RefPtr<Image> CSSStyleImageValue::GetImage() const {
  if (IsCachePending())
    return nullptr;
  // cachedImage can be null if image is StyleInvalidImage
  ImageResourceContent* cached_image =
      image_value_->CachedImage()->CachedImage();
  if (cached_image) {
    // getImage() returns the nullImage() if the image is not available yet
    return cached_image->GetImage()->ImageForDefaultFrame();
  }
  return nullptr;
}

}  // namespace blink
