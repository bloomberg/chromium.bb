// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CompositingRecorder_h
#define CompositingRecorder_h

#include "platform/graphics/GraphicsTypes.h"
#include "platform/graphics/paint/DisplayItem.h"
#include "public/platform/WebBlendMode.h"

namespace blink {

class GraphicsContext;
class LayoutObject;

class CompositingRecorder {
public:
    explicit CompositingRecorder(GraphicsContext*, DisplayItemClient, const CompositeOperator, const WebBlendMode&, const float);

    ~CompositingRecorder();

private:
    DisplayItemClient m_client;
    GraphicsContext* m_graphicsContext;
};

} // namespace blink

#endif // CompositingRecorder_h
