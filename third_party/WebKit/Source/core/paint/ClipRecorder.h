// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ClipRecorder_h
#define ClipRecorder_h

#include "core/rendering/RenderLayerModelObject.h"
#include "platform/geometry/LayoutRect.h"
#include "platform/graphics/paint/DisplayItem.h"

namespace blink {

class ClipRecorder {
public:
    ClipRecorder(RenderLayerModelObject&, const PaintInfo&, const LayoutRect&);
    ~ClipRecorder();

    static DisplayItem::Type paintPhaseToClipType(PaintPhase);

private:
    const PaintInfo& m_paintInfo;
    RenderLayerModelObject& m_renderer;
};

} // namespace blink

#endif // ClipRecorder_h
