// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "OrientationIterator.h"

namespace blink {

OrientationIterator::OrientationIterator(const UChar* buffer, unsigned bufferSize, FontOrientation runOrientation)
    : m_utf16Iterator(adoptPtr(new UTF16TextIterator(buffer, bufferSize)))
    , m_bufferSize(bufferSize)
    , m_nextUChar32(0)
    , m_atEnd(bufferSize == 0)
    , m_currentRenderOrientation(OrientationInvalid)
{
    // There's not much point in segmenting by isUprightInVertical if the text
    // orientation is not "mixed".
    ASSERT(runOrientation == FontOrientation::VerticalMixed);
}

bool OrientationIterator::consume(unsigned *orientationLimit, RenderOrientation* renderOrientation)
{
    if (m_atEnd)
        return false;

    while (m_utf16Iterator->consume(m_nextUChar32)) {
        m_previousRenderOrientation = m_currentRenderOrientation;
        m_currentRenderOrientation = Character::isUprightInMixedVertical(m_nextUChar32) ? OrientationKeep : OrientationRotateSideways;

        if (m_previousRenderOrientation != m_currentRenderOrientation && m_previousRenderOrientation != OrientationInvalid) {
            *orientationLimit = m_utf16Iterator->offset();
            *renderOrientation = m_previousRenderOrientation;
            return true;
        }
        m_utf16Iterator->advance();
    }
    *orientationLimit = m_bufferSize;
    *renderOrientation = m_currentRenderOrientation;
    m_atEnd = true;
    return true;
}

}
