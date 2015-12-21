// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/ObjectPaintProperties.h"

namespace blink {

TransformPaintPropertyNode* ObjectPaintProperties::transformForLayerContents() const
{
    // See the hierarchy diagram in the header.
    if (m_transform)
        return m_transform.get();
    if (m_paintOffsetTranslation)
        return m_paintOffsetTranslation.get();
    return nullptr;
}

} // namespace blink
