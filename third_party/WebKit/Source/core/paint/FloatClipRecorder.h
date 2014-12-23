// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FloatClipRecorder_h
#define FloatClipRecorder_h

#include "core/rendering/RenderObject.h"
#include "platform/geometry/FloatRect.h"

namespace blink {

class FloatClipRecorder {
public:
    FloatClipRecorder(GraphicsContext&, DisplayItemClient, const PaintPhase&, const FloatRect&);
    ~FloatClipRecorder();

private:
    static DisplayItem::Type paintPhaseToFloatClipType(PaintPhase);

    GraphicsContext& m_context;
    DisplayItemClient m_client;
};

} // namespace blink

#endif // FloatClipRecorder_h
