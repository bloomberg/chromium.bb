// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OrientationIterator_h
#define OrientationIterator_h

#include "platform/fonts/FontOrientation.h"
#include "platform/fonts/ScriptRunIterator.h"
#include "platform/fonts/UTF16TextIterator.h"
#include "wtf/Allocator.h"
#include "wtf/Noncopyable.h"

namespace blink {

class PLATFORM_EXPORT OrientationIterator {
    USING_FAST_MALLOC(OrientationIterator);
    WTF_MAKE_NONCOPYABLE(OrientationIterator);
public:
    enum RenderOrientation {
        OrientationKeep,
        OrientationRotateSideways,
        OrientationInvalid
    };

    OrientationIterator(const UChar* buffer, unsigned bufferSize, FontOrientation runOrientation);

    bool consume(unsigned* orientationLimit, RenderOrientation*);
private:
    OwnPtr<UTF16TextIterator> m_utf16Iterator;
    unsigned m_bufferSize;
    UChar32 m_nextUChar32;
    bool m_atEnd;

    RenderOrientation m_currentRenderOrientation;
    RenderOrientation m_previousRenderOrientation;
};

} // namespace blink

#endif
