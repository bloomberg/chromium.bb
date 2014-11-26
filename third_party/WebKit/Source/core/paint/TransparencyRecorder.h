// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TransparencyRecorder_h
#define TransparencyRecorder_h

#include "platform/graphics/paint/DisplayItem.h"
#include "public/platform/WebBlendMode.h"

namespace blink {

class GraphicsContext;
class RenderObject;

class TransparencyRecorder {
public:
    explicit TransparencyRecorder(GraphicsContext*, const RenderObject*, const WebBlendMode&, const float opacity);

    ~TransparencyRecorder();

private:
    const RenderObject* m_renderer;
    GraphicsContext* m_graphicsContext;
};

} // namespace blink

#endif // TransparencyRecorder_h
