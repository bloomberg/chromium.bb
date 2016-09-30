// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/ObjectPaintProperties.h"

namespace blink {

void ObjectPaintProperties::getContentsPropertyTreeState(GeometryPropertyTreeState& state, LayoutPoint& paintOffsetFromState) const
{
    state = localBorderBoxProperties()->geometryPropertyTreeState;
    if (svgLocalToBorderBoxTransform()) {
        state.transform = svgLocalToBorderBoxTransform();
        // No paint offset from the state because svgLocalToBorderTransform
        // embeds the paint offset in it.
        paintOffsetFromState = LayoutPoint();
    } else {
        if (scrollTranslation())
            state.transform = scrollTranslation();
        paintOffsetFromState = localBorderBoxProperties()->paintOffset;
    }

    if (overflowClip())
        state.clip = overflowClip();
    else if (cssClip())
        state.clip = cssClip();
    // TODO(chrishtr): cssClipFixedPosition needs to be handled somehow.
}

} // namespace blink
