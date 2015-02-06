// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FloatClipRecorder_h
#define FloatClipRecorder_h

#include "core/layout/LayoutObject.h"
#include "platform/geometry/FloatRect.h"
#include "platform/graphics/paint/DisplayItem.h"

namespace blink {

class FloatClipRecorder {
public:
    FloatClipRecorder(GraphicsContext&, DisplayItemClient, PaintPhase, const FloatRect&);
    ~FloatClipRecorder();

private:
    GraphicsContext& m_context;
    DisplayItemClient m_client;
    DisplayItem::Type m_clipType;
};

} // namespace blink

#endif // FloatClipRecorder_h
