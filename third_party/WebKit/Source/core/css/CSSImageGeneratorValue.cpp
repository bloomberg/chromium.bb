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

CSSImageGeneratorValue::CSSImageGeneratorValue(ClassType classType)
    : CSSValue(classType) {}

CSSImageGeneratorValue::~CSSImageGeneratorValue() {}

void CSSImageGeneratorValue::addClient(const LayoutObject* layoutObject,
                                       const IntSize& size) {
  DCHECK(layoutObject);
  if (m_clients.isEmpty()) {
    DCHECK(!m_keepAlive);
    m_keepAlive = this;
  }

  if (!size.isEmpty())
    m_sizes.add(size);

  LayoutObjectSizeCountMap::iterator it = m_clients.find(layoutObject);
  if (it == m_clients.end()) {
    m_clients.insert(layoutObject, SizeAndCount(size, 1));
  } else {
    SizeAndCount& sizeCount = it->value;
    ++sizeCount.count;
  }
}

CSSImageGeneratorValue* CSSImageGeneratorValue::valueWithURLsMadeAbsolute() {
  if (isCrossfadeValue())
    return toCSSCrossfadeValue(this)->valueWithURLsMadeAbsolute();
  return this;
}

void CSSImageGeneratorValue::removeClient(const LayoutObject* layoutObject) {
  DCHECK(layoutObject);
  LayoutObjectSizeCountMap::iterator it = m_clients.find(layoutObject);
  SECURITY_DCHECK(it != m_clients.end());

  IntSize removedImageSize;
  SizeAndCount& sizeCount = it->value;
  IntSize size = sizeCount.size;
  if (!size.isEmpty()) {
    m_sizes.remove(size);
    if (!m_sizes.contains(size))
      m_images.erase(size);
  }

  if (!--sizeCount.count)
    m_clients.erase(layoutObject);

  if (m_clients.isEmpty()) {
    DCHECK(m_keepAlive);
    m_keepAlive.clear();
  }
}

Image* CSSImageGeneratorValue::getImage(const LayoutObject* layoutObject,
                                        const IntSize& size) {
  LayoutObjectSizeCountMap::iterator it = m_clients.find(layoutObject);
  if (it != m_clients.end()) {
    SizeAndCount& sizeCount = it->value;
    IntSize oldSize = sizeCount.size;
    if (oldSize != size) {
      removeClient(layoutObject);
      addClient(layoutObject, size);
    }
  }

  // Don't generate an image for empty sizes.
  if (size.isEmpty())
    return nullptr;

  // Look up the image in our cache.
  return m_images.at(size);
}

void CSSImageGeneratorValue::putImage(const IntSize& size,
                                      PassRefPtr<Image> image) {
  m_images.insert(size, std::move(image));
}

PassRefPtr<Image> CSSImageGeneratorValue::image(
    const LayoutObject& layoutObject,
    const IntSize& size,
    float zoom) {
  switch (getClassType()) {
    case CrossfadeClass:
      return toCSSCrossfadeValue(this)->image(layoutObject, size);
    case LinearGradientClass:
      return toCSSLinearGradientValue(this)->image(layoutObject, size);
    case PaintClass:
      return toCSSPaintValue(this)->image(layoutObject, size, zoom);
    case RadialGradientClass:
      return toCSSRadialGradientValue(this)->image(layoutObject, size);
    case ConicGradientClass:
      return toCSSConicGradientValue(this)->image(layoutObject, size);
    default:
      NOTREACHED();
  }
  return nullptr;
}

bool CSSImageGeneratorValue::isFixedSize() const {
  switch (getClassType()) {
    case CrossfadeClass:
      return toCSSCrossfadeValue(this)->isFixedSize();
    case LinearGradientClass:
      return toCSSLinearGradientValue(this)->isFixedSize();
    case PaintClass:
      return toCSSPaintValue(this)->isFixedSize();
    case RadialGradientClass:
      return toCSSRadialGradientValue(this)->isFixedSize();
    case ConicGradientClass:
      return toCSSConicGradientValue(this)->isFixedSize();
    default:
      NOTREACHED();
  }
  return false;
}

IntSize CSSImageGeneratorValue::fixedSize(const LayoutObject& layoutObject,
                                          const FloatSize& defaultObjectSize) {
  switch (getClassType()) {
    case CrossfadeClass:
      return toCSSCrossfadeValue(this)->fixedSize(layoutObject,
                                                  defaultObjectSize);
    case LinearGradientClass:
      return toCSSLinearGradientValue(this)->fixedSize(layoutObject);
    case PaintClass:
      return toCSSPaintValue(this)->fixedSize(layoutObject);
    case RadialGradientClass:
      return toCSSRadialGradientValue(this)->fixedSize(layoutObject);
    case ConicGradientClass:
      return toCSSConicGradientValue(this)->fixedSize(layoutObject);
    default:
      NOTREACHED();
  }
  return IntSize();
}

bool CSSImageGeneratorValue::isPending() const {
  switch (getClassType()) {
    case CrossfadeClass:
      return toCSSCrossfadeValue(this)->isPending();
    case LinearGradientClass:
      return toCSSLinearGradientValue(this)->isPending();
    case PaintClass:
      return toCSSPaintValue(this)->isPending();
    case RadialGradientClass:
      return toCSSRadialGradientValue(this)->isPending();
    case ConicGradientClass:
      return toCSSConicGradientValue(this)->isPending();
    default:
      NOTREACHED();
  }
  return false;
}

bool CSSImageGeneratorValue::knownToBeOpaque(
    const LayoutObject& layoutObject) const {
  switch (getClassType()) {
    case CrossfadeClass:
      return toCSSCrossfadeValue(this)->knownToBeOpaque(layoutObject);
    case LinearGradientClass:
      return toCSSLinearGradientValue(this)->knownToBeOpaque(layoutObject);
    case PaintClass:
      return toCSSPaintValue(this)->knownToBeOpaque(layoutObject);
    case RadialGradientClass:
      return toCSSRadialGradientValue(this)->knownToBeOpaque(layoutObject);
    case ConicGradientClass:
      return toCSSConicGradientValue(this)->knownToBeOpaque(layoutObject);
    default:
      NOTREACHED();
  }
  return false;
}

void CSSImageGeneratorValue::loadSubimages(const Document& document) {
  switch (getClassType()) {
    case CrossfadeClass:
      toCSSCrossfadeValue(this)->loadSubimages(document);
      break;
    case LinearGradientClass:
      toCSSLinearGradientValue(this)->loadSubimages(document);
      break;
    case PaintClass:
      toCSSPaintValue(this)->loadSubimages(document);
      break;
    case RadialGradientClass:
      toCSSRadialGradientValue(this)->loadSubimages(document);
      break;
    case ConicGradientClass:
      toCSSConicGradientValue(this)->loadSubimages(document);
      break;
    default:
      NOTREACHED();
  }
}

}  // namespace blink
