// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ScopeRecorder_h
#define ScopeRecorder_h

#include "core/layout/PaintPhase.h"
#include "wtf/PassOwnPtr.h"

namespace blink {

class DisplayItem;
class DisplayItemList;
class GraphicsContext;
class LayoutObject;

class ScopeRecorder {
public:
    ScopeRecorder(GraphicsContext*, const LayoutObject&);
    ~ScopeRecorder();

private:
    DisplayItemList* m_displayItemList;
    const LayoutObject& m_object;
};

} // namespace blink

#endif // ScopeRecorder_h
