// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PaintChunker_h
#define PaintChunker_h

#include "platform/PlatformExport.h"
#include "platform/graphics/paint/PaintChunk.h"
#include "platform/graphics/paint/PaintProperties.h"
#include "wtf/Vector.h"

namespace blink {

// Accepts information about changes to |PaintProperties| as drawings are
// accumulated, and produces a series of paint chunks: contiguous ranges of the
// display list with identical |PaintProperties|.
class PLATFORM_EXPORT PaintChunker {
public:
    PaintChunker();
    ~PaintChunker();

    bool isInInitialState() const { return m_chunks.isEmpty() && m_currentProperties == PaintProperties(); }

    void updateCurrentPaintProperties(const PaintProperties&);

    void incrementDisplayItemIndex();
    void decrementDisplayItemIndex();

    // Releases the generated paint chunk list and resets the state of this
    // object.
    Vector<PaintChunk> releasePaintChunks();

private:
    Vector<PaintChunk> m_chunks;
    PaintProperties m_currentProperties;
};

} // namespace blink

#endif // PaintChunker_h
