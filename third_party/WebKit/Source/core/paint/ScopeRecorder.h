// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ScopeRecorder_h
#define ScopeRecorder_h

#include "core/CoreExport.h"
#include "platform/graphics/paint/DisplayItem.h"
#include "wtf/Allocator.h"

namespace blink {

class GraphicsContext;
class LayoutObject;
class PaintController;

class CORE_EXPORT ScopeRecorder {
    DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();
public:
    ScopeRecorder(GraphicsContext&);

    ~ScopeRecorder();

private:
    PaintController& m_paintController;
};

} // namespace blink

#endif // ScopeRecorder_h
