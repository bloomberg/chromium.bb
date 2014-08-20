// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FrameViewAutoSizeInfo_h
#define FrameViewAutoSizeInfo_h

#include "platform/geometry/IntSize.h"
#include "wtf/FastAllocBase.h"
#include "wtf/Noncopyable.h"
#include "wtf/RefPtr.h"

namespace blink {

class FrameView;

class FrameViewAutoSizeInfo {
    WTF_MAKE_NONCOPYABLE(FrameViewAutoSizeInfo);
    WTF_MAKE_FAST_ALLOCATED;

public:
    FrameViewAutoSizeInfo(FrameView*);
    ~FrameViewAutoSizeInfo();
    void configureAutoSizeMode(const IntSize& minSize, const IntSize& maxSize);
    void autoSizeIfNeeded();

private:
    void removeAutoSizeMode();

    RefPtr<FrameView> m_frameView;

    bool m_inAutoSize;
    // True if autosize has been run since m_shouldAutoSize was set.
    bool m_didRunAutosize;
    // The lower bound on the size when autosizing.
    IntSize m_minAutoSize;
    // The upper bound on the size when autosizing.
    IntSize m_maxAutoSize;
};

} // namespace blink

#endif // FrameViewAutoSizeInfo_h
