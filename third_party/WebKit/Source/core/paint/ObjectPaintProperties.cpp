// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/ObjectPaintProperties.h"

namespace blink {

void ObjectPaintProperties::updateContentsProperties() const {
  DCHECK(m_localBorderBoxProperties);
  DCHECK(!m_contentsProperties);

  m_contentsProperties =
      WTF::makeUnique<PropertyTreeState>(*m_localBorderBoxProperties);

  if (scrollTranslation())
    m_contentsProperties->setTransform(scrollTranslation());
  if (overflowClip())
    m_contentsProperties->setClip(overflowClip());
  else if (cssClip())
    m_contentsProperties->setClip(cssClip());

  // TODO(chrishtr): cssClipFixedPosition needs to be handled somehow.
}

}  // namespace blink
