// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PaintArtifact_h
#define PaintArtifact_h

#include "platform/PlatformExport.h"
#include "platform/graphics/paint/DisplayItemList.h"
#include "platform/graphics/paint/PaintChunk.h"
#include "wtf/Allocator.h"
#include "wtf/Noncopyable.h"
#include "wtf/Vector.h"

namespace blink {

class GraphicsContext;
class WebDisplayItemList;

// The output of painting, consisting of a series of drawings in paint order,
// partitioned into discontiguous chunks with a common set of paint properties
// (i.e. associated with the same transform, clip, effects, etc.).
//
// It represents a particular state of the world, and should be immutable
// (const) to most of its users.
class PLATFORM_EXPORT PaintArtifact final {
    DISALLOW_NEW();
    WTF_MAKE_NONCOPYABLE(PaintArtifact);
public:
    PaintArtifact();
    ~PaintArtifact();

    bool isEmpty() const { return m_displayItemList.isEmpty(); }

    DisplayItemList& displayItemList() { return m_displayItemList; }
    const DisplayItemList& displayItemList() const { return m_displayItemList; }

    Vector<PaintChunk>& paintChunks() { return m_paintChunks; }
    const Vector<PaintChunk>& paintChunks() const { return m_paintChunks; }

    // Resets to an empty paint artifact.
    void reset();

    // Returns the approximate memory usage, excluding memory likely to be
    // shared with the embedder after copying to WebDisplayItemList.
    size_t approximateUnsharedMemoryUsage() const;

    // Draws the paint artifact to a GraphicsContext.
    void replay(GraphicsContext&) const;

    // Writes the paint artifact into a WebDisplayItemList.
    void appendToWebDisplayItemList(WebDisplayItemList*) const;

private:
    DisplayItemList m_displayItemList;
    Vector<PaintChunk> m_paintChunks;
};

} // namespace blink

#endif // PaintArtifact_h
