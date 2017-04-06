// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/RarePaintData.h"

#include "core/paint/ObjectPaintProperties.h"

namespace blink {

RarePaintData::RarePaintData() {}

RarePaintData::~RarePaintData() {}

ObjectPaintProperties& RarePaintData::ensurePaintProperties() {
  if (!m_paintProperties)
    m_paintProperties = ObjectPaintProperties::create();
  return *m_paintProperties.get();
}

void RarePaintData::clearLocalBorderBoxProperties() {
  m_localBorderBoxProperties = nullptr;

  // The contents properties are based on the border box so we need to clear
  // the cached value.
  m_contentsProperties = nullptr;
}

void RarePaintData::setLocalBorderBoxProperties(PropertyTreeState& state) {
  if (!m_localBorderBoxProperties)
    m_localBorderBoxProperties = WTF::makeUnique<PropertyTreeState>(state);
  else
    *m_localBorderBoxProperties = state;

  // The contents properties are based on the border box so we need to clear
  // the cached value.
  m_contentsProperties = nullptr;
}

static std::unique_ptr<PropertyTreeState> computeContentsProperties(
    PropertyTreeState* localBorderBoxProperties,
    ObjectPaintProperties* paintProperties) {
  if (!localBorderBoxProperties)
    return nullptr;

  std::unique_ptr<PropertyTreeState> contents =
      WTF::makeUnique<PropertyTreeState>(*localBorderBoxProperties);

  if (paintProperties) {
    if (paintProperties->scrollTranslation())
      contents->setTransform(paintProperties->scrollTranslation());
    if (paintProperties->overflowClip())
      contents->setClip(paintProperties->overflowClip());
    else if (paintProperties->cssClip())
      contents->setClip(paintProperties->cssClip());
  }

  // TODO(chrishtr): cssClipFixedPosition needs to be handled somehow.

  return contents;
}

const PropertyTreeState* RarePaintData::contentsProperties() const {
  if (!m_contentsProperties) {
    if (m_localBorderBoxProperties) {
      m_contentsProperties = computeContentsProperties(
          m_localBorderBoxProperties.get(), m_paintProperties.get());
    }
  } else {
#if DCHECK_IS_ON()
    // Check that the cached contents properties are valid by checking that they
    // do not change if recalculated.
    DCHECK(m_localBorderBoxProperties);
    std::unique_ptr<PropertyTreeState> oldProperties =
        std::move(m_contentsProperties);
    m_contentsProperties = computeContentsProperties(
        m_localBorderBoxProperties.get(), m_paintProperties.get());
    DCHECK(*m_contentsProperties == *oldProperties);
#endif
  }
  return m_contentsProperties.get();
}

}  // namespace blink
