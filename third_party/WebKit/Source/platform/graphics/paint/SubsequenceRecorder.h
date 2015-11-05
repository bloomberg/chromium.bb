// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SubsequenceRecorder_h
#define SubsequenceRecorder_h

#include "platform/graphics/paint/DisplayItem.h"

namespace blink {

class GraphicsContext;
class PaintController;

// SubsequenceRecorder records BeginSubsequenceDisplayItem and EndSubsequenceDisplayItem
// sentinels at either end of a continguous sequence of DisplayItems, and supports
// caching via a CachedDisplayItem with the CachedSubsequence DisplayItem type.
//
// Also note that useCachedSubsequenceIfPossible is not sufficient to determine whether a
// CachedSubsequence can be used. In particular, the client is responsible for checking that
// none of the DisplayItemClients that contribute to the subsequence have been invalidated.
//
class PLATFORM_EXPORT SubsequenceRecorder {
public:
    static bool useCachedSubsequenceIfPossible(GraphicsContext&, const DisplayItemClientWrapper&);

    SubsequenceRecorder(GraphicsContext&, const DisplayItemClientWrapper&);
    ~SubsequenceRecorder();

    void setUncacheable();

private:
    PaintController& m_paintController;
    DisplayItemClientWrapper m_client;
    size_t m_beginSubsequenceIndex;
};

} // namespace blink

#endif // SubsequenceRecorder_h
