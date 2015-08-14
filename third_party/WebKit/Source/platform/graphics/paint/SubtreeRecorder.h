// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SubtreeRecorder_h
#define SubtreeRecorder_h

#include "platform/graphics/paint/DisplayItemClient.h"

namespace blink {

class DisplayItemList;
class GraphicsContext;

// Records subtree information during painting. The recorder's life span covers
// all painting operations executed during the root of the subtree's paint method
// for the paint phase.
class PLATFORM_EXPORT SubtreeRecorder {
public:
    SubtreeRecorder(GraphicsContext&, const DisplayItemClientWrapper&, int paintPhase);
    ~SubtreeRecorder();

    bool canUseCache() const;

private:
    DisplayItemList* m_displayItemList;
    DisplayItemClientWrapper m_client;
    const int m_paintPhase;
    bool m_canUseCache;
#if ENABLE(ASSERT)
    mutable bool m_checkedCanUseCache;
#endif
};

} // namespace blink

#endif // SubtreeRecorder_h
