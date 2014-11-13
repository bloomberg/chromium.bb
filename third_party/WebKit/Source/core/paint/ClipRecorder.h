// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ClipRecorder_h
#define ClipRecorder_h

#include "platform/graphics/paint/DisplayItem.h"

namespace blink {

class ClipDisplayItem;
class ClipRect;
class GraphicsContext;
class RenderLayer;
class RoundedRect;

class ClipRecorder {
public:
    explicit ClipRecorder(RenderLayer*, GraphicsContext*, DisplayItem::Type, const ClipRect&);
    void addRoundedRectClip(const RoundedRect&);

    ~ClipRecorder();

private:
    ClipDisplayItem* m_clipDisplayItem;
    GraphicsContext* m_graphicsContext;
    RenderLayer* m_renderLayer;
};

} // namespace blink

#endif // ClipRecorder_h
