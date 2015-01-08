// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ClipPathRecorder_h
#define ClipPathRecorder_h

#include "platform/graphics/Path.h"
#include "platform/graphics/paint/DisplayItem.h"

namespace blink {

class GraphicsContext;

class PLATFORM_EXPORT ClipPathRecorder {
    WTF_MAKE_FAST_ALLOCATED;
public:
    ClipPathRecorder(GraphicsContext&, DisplayItemClient, const Path&, WindRule);
    ~ClipPathRecorder();

private:
    GraphicsContext& m_context;
    DisplayItemClient m_client;
};

} // namespace blink

#endif // ClipPathRecorder_h
