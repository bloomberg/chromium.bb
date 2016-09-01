// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/ObjectPaintProperties.h"

namespace blink {

void ObjectPaintProperties::getContentsProperties(PropertyTreeState& properties) const
{
    properties = localBorderBoxProperties()->propertyTreeState;
    if (scrollTranslation())
        properties.transform = scrollTranslation();
    else if (svgLocalToBorderBoxTransform())
        properties.transform = svgLocalToBorderBoxTransform();

    if (overflowClip())
        properties.clip = overflowClip();
    else if (cssClip())
        properties.clip = cssClip();
    // TODO(chrishtr): cssClipFixedPosition needs to be handled somehow.
}

} // namespace blink
