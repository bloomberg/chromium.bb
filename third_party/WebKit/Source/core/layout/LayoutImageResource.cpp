/*
 * Copyright (C) 1999 Lars Knoll <knoll@kde.org>
 * Copyright (C) 1999 Antti Koivisto <koivisto@kde.org>
 * Copyright (C) 2000 Dirk Mueller <mueller@kde.org>
 * Copyright (C) 2006 Allan Sandfeld Jensen <kde@carewolf.com>
 * Copyright (C) 2006 Samuel Weinig <sam.weinig@gmail.com>
 * Copyright (C) 2003, 2004, 2005, 2006, 2008, 2009, 2010 Apple Inc.
 *               All rights reserved.
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2010 Patrick Gansterer <paroga@paroga.com>
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

#include "core/layout/LayoutImageResource.h"

#include "core/dom/Element.h"
#include "core/layout/LayoutImage.h"
#include "core/svg/graphics/SVGImageForContainer.h"

namespace blink {

LayoutImageResource::LayoutImageResource()
    : layout_object_(nullptr), cached_image_(nullptr) {}

LayoutImageResource::~LayoutImageResource() {}

void LayoutImageResource::Initialize(LayoutObject* layout_object) {
  DCHECK(!layout_object_);
  DCHECK(layout_object);
  layout_object_ = layout_object;
}

void LayoutImageResource::Shutdown() {
  DCHECK(layout_object_);

  if (!cached_image_)
    return;
  cached_image_->RemoveObserver(layout_object_);
}

void LayoutImageResource::SetImageResource(ImageResourceContent* new_image) {
  DCHECK(layout_object_);

  if (cached_image_ == new_image)
    return;

  if (cached_image_) {
    cached_image_->RemoveObserver(layout_object_);
  }
  cached_image_ = new_image;
  if (cached_image_) {
    cached_image_->AddObserver(layout_object_);
    if (cached_image_->ErrorOccurred())
      layout_object_->ImageChanged(cached_image_.Get());
  } else {
    layout_object_->ImageChanged(cached_image_.Get());
  }
}

void LayoutImageResource::ResetAnimation() {
  DCHECK(layout_object_);

  if (!cached_image_)
    return;

  cached_image_->GetImage()->ResetAnimation();

  layout_object_->SetShouldDoFullPaintInvalidation();
}

LayoutSize LayoutImageResource::ImageSize(float multiplier) const {
  if (!cached_image_)
    return LayoutSize();
  LayoutSize size = cached_image_->ImageSize(
      LayoutObject::ShouldRespectImageOrientation(layout_object_), multiplier);
  if (layout_object_ && layout_object_->IsLayoutImage() && size.Width() &&
      size.Height())
    size.Scale(ToLayoutImage(layout_object_)->ImageDevicePixelRatio());
  return size;
}

PassRefPtr<Image> LayoutImageResource::GetImage(
    const IntSize& container_size) const {
  if (!cached_image_)
    return Image::NullImage();

  if (!cached_image_->GetImage()->IsSVGImage())
    return cached_image_->GetImage();

  KURL url;
  SVGImage* svg_image = ToSVGImage(cached_image_->GetImage());
  Node* node = layout_object_->GetNode();
  if (node && node->IsElementNode()) {
    const AtomicString& url_string = ToElement(node)->ImageSourceURL();
    url = node->GetDocument().CompleteURL(url_string);
  }
  return SVGImageForContainer::Create(
      svg_image, container_size, layout_object_->StyleRef().EffectiveZoom(),
      url);
}

bool LayoutImageResource::MaybeAnimated() const {
  Image* image = cached_image_ ? cached_image_->GetImage() : Image::NullImage();
  return image->MaybeAnimated();
}

}  // namespace blink
