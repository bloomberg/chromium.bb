/*
 * Copyright (C) 2008 Apple Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/css/CSSImageGeneratorValue.h"

#include "core/css/CSSCrossfadeValue.h"
#include "core/css/CSSGradientValue.h"
#include "core/css/CSSPaintValue.h"
#include "core/layout/LayoutObject.h"
#include "platform/graphics/Image.h"

namespace blink {

using cssvalue::ToCSSCrossfadeValue;

CSSImageGeneratorValue::CSSImageGeneratorValue(ClassType class_type)
    : CSSValue(class_type) {}

CSSImageGeneratorValue::~CSSImageGeneratorValue() {}

void CSSImageGeneratorValue::AddClient(const ImageResourceObserver* client,
                                       const IntSize& size) {
  DCHECK(client);
  if (clients_.IsEmpty()) {
    DCHECK(!keep_alive_);
    keep_alive_ = this;
  }

  if (!size.IsEmpty())
    sizes_.insert(size);

  ClientSizeCountMap::iterator it = clients_.find(client);
  if (it == clients_.end()) {
    clients_.insert(client, SizeAndCount(size, 1));
  } else {
    SizeAndCount& size_count = it->value;
    ++size_count.count;
  }
}

CSSImageGeneratorValue* CSSImageGeneratorValue::ValueWithURLsMadeAbsolute() {
  if (IsCrossfadeValue())
    return ToCSSCrossfadeValue(this)->ValueWithURLsMadeAbsolute();
  return this;
}

void CSSImageGeneratorValue::RemoveClient(const ImageResourceObserver* client) {
  DCHECK(client);
  ClientSizeCountMap::iterator it = clients_.find(client);
  SECURITY_DCHECK(it != clients_.end());

  IntSize removed_image_size;
  SizeAndCount& size_count = it->value;
  IntSize size = size_count.size;
  if (!size.IsEmpty()) {
    sizes_.erase(size);
    if (!sizes_.Contains(size))
      images_.erase(size);
  }

  if (!--size_count.count)
    clients_.erase(client);

  if (clients_.IsEmpty()) {
    DCHECK(keep_alive_);
    keep_alive_.Clear();
  }
}

Image* CSSImageGeneratorValue::GetImage(const ImageResourceObserver* client,
                                        const Document&,
                                        const ComputedStyle&,
                                        const IntSize& size) {
  ClientSizeCountMap::iterator it = clients_.find(client);
  if (it != clients_.end()) {
    SizeAndCount& size_count = it->value;
    IntSize old_size = size_count.size;
    if (old_size != size) {
      RemoveClient(client);
      AddClient(client, size);
    }
  }

  // Don't generate an image for empty sizes.
  if (size.IsEmpty())
    return nullptr;

  // Look up the image in our cache.
  return images_.at(size);
}

void CSSImageGeneratorValue::PutImage(const IntSize& size,
                                      RefPtr<Image> image) {
  images_.insert(size, std::move(image));
}

RefPtr<Image> CSSImageGeneratorValue::GetImage(
    const ImageResourceObserver& client,
    const Document& document,
    const ComputedStyle& style,
    const IntSize& size) {
  switch (GetClassType()) {
    case kCrossfadeClass:
      return ToCSSCrossfadeValue(this)->GetImage(client, document, style, size);
    case kLinearGradientClass:
      return ToCSSLinearGradientValue(this)->GetImage(client, document, style,
                                                      size);
    case kPaintClass:
      return ToCSSPaintValue(this)->GetImage(client, document, style, size);
    case kRadialGradientClass:
      return ToCSSRadialGradientValue(this)->GetImage(client, document, style,
                                                      size);
    case kConicGradientClass:
      return ToCSSConicGradientValue(this)->GetImage(client, document, style,
                                                     size);
    default:
      NOTREACHED();
  }
  return nullptr;
}

bool CSSImageGeneratorValue::IsFixedSize() const {
  switch (GetClassType()) {
    case kCrossfadeClass:
      return ToCSSCrossfadeValue(this)->IsFixedSize();
    case kLinearGradientClass:
      return ToCSSLinearGradientValue(this)->IsFixedSize();
    case kPaintClass:
      return ToCSSPaintValue(this)->IsFixedSize();
    case kRadialGradientClass:
      return ToCSSRadialGradientValue(this)->IsFixedSize();
    case kConicGradientClass:
      return ToCSSConicGradientValue(this)->IsFixedSize();
    default:
      NOTREACHED();
  }
  return false;
}

IntSize CSSImageGeneratorValue::FixedSize(
    const Document& document,
    const FloatSize& default_object_size) {
  switch (GetClassType()) {
    case kCrossfadeClass:
      return ToCSSCrossfadeValue(this)->FixedSize(document,
                                                  default_object_size);
    case kLinearGradientClass:
      return ToCSSLinearGradientValue(this)->FixedSize(document);
    case kPaintClass:
      return ToCSSPaintValue(this)->FixedSize(document);
    case kRadialGradientClass:
      return ToCSSRadialGradientValue(this)->FixedSize(document);
    case kConicGradientClass:
      return ToCSSConicGradientValue(this)->FixedSize(document);
    default:
      NOTREACHED();
  }
  return IntSize();
}

bool CSSImageGeneratorValue::IsPending() const {
  switch (GetClassType()) {
    case kCrossfadeClass:
      return ToCSSCrossfadeValue(this)->IsPending();
    case kLinearGradientClass:
      return ToCSSLinearGradientValue(this)->IsPending();
    case kPaintClass:
      return ToCSSPaintValue(this)->IsPending();
    case kRadialGradientClass:
      return ToCSSRadialGradientValue(this)->IsPending();
    case kConicGradientClass:
      return ToCSSConicGradientValue(this)->IsPending();
    default:
      NOTREACHED();
  }
  return false;
}

bool CSSImageGeneratorValue::KnownToBeOpaque(const Document& document,
                                             const ComputedStyle& style) const {
  switch (GetClassType()) {
    case kCrossfadeClass:
      return ToCSSCrossfadeValue(this)->KnownToBeOpaque(document, style);
    case kLinearGradientClass:
      return ToCSSLinearGradientValue(this)->KnownToBeOpaque(document, style);
    case kPaintClass:
      return ToCSSPaintValue(this)->KnownToBeOpaque(document, style);
    case kRadialGradientClass:
      return ToCSSRadialGradientValue(this)->KnownToBeOpaque(document, style);
    case kConicGradientClass:
      return ToCSSConicGradientValue(this)->KnownToBeOpaque(document, style);
    default:
      NOTREACHED();
  }
  return false;
}

void CSSImageGeneratorValue::LoadSubimages(const Document& document) {
  switch (GetClassType()) {
    case kCrossfadeClass:
      ToCSSCrossfadeValue(this)->LoadSubimages(document);
      break;
    case kLinearGradientClass:
      ToCSSLinearGradientValue(this)->LoadSubimages(document);
      break;
    case kPaintClass:
      ToCSSPaintValue(this)->LoadSubimages(document);
      break;
    case kRadialGradientClass:
      ToCSSRadialGradientValue(this)->LoadSubimages(document);
      break;
    case kConicGradientClass:
      ToCSSConicGradientValue(this)->LoadSubimages(document);
      break;
    default:
      NOTREACHED();
  }
}

}  // namespace blink
