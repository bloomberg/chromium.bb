/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "core/style/StyleGeneratedImage.h"

#include "core/css/CSSImageGeneratorValue.h"
#include "core/css/resolver/StyleResolver.h"

namespace blink {

StyleGeneratedImage::StyleGeneratedImage(const CSSImageGeneratorValue& value)
    : image_generator_value_(const_cast<CSSImageGeneratorValue*>(&value)),
      fixed_size_(image_generator_value_->IsFixedSize()) {
  is_generated_image_ = true;
  if (value.IsPaintValue())
    is_paint_image_ = true;
}

CSSValue* StyleGeneratedImage::CssValue() const {
  return image_generator_value_.Get();
}

CSSValue* StyleGeneratedImage::ComputedCSSValue() const {
  return image_generator_value_->ValueWithURLsMadeAbsolute();
}

LayoutSize StyleGeneratedImage::ImageSize(
    const Document& document,
    float multiplier,
    const LayoutSize& default_object_size) const {
  if (fixed_size_) {
    FloatSize unzoomed_default_object_size(default_object_size);
    unzoomed_default_object_size.Scale(1 / multiplier);
    return ApplyZoom(LayoutSize(image_generator_value_->FixedSize(
                         document, unzoomed_default_object_size)),
                     multiplier);
  }

  return default_object_size;
}

void StyleGeneratedImage::AddClient(ImageResourceObserver* observer) {
  image_generator_value_->AddClient(observer, IntSize());
}

void StyleGeneratedImage::RemoveClient(ImageResourceObserver* observer) {
  image_generator_value_->RemoveClient(observer);
}

scoped_refptr<Image> StyleGeneratedImage::GetImage(
    const ImageResourceObserver& observer,
    const Document& document,
    const ComputedStyle& style,
    const IntSize& container_size,
    const LayoutSize* logical_size) const {
  return image_generator_value_->GetImage(observer, document, style,
                                          container_size, logical_size);
}

bool StyleGeneratedImage::KnownToBeOpaque(const Document& document,
                                          const ComputedStyle& style) const {
  return image_generator_value_->KnownToBeOpaque(document, style);
}

void StyleGeneratedImage::Trace(blink::Visitor* visitor) {
  visitor->Trace(image_generator_value_);
  StyleImage::Trace(visitor);
}

}  // namespace blink
