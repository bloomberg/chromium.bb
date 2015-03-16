// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ExpensiveCanvasHeuristicParameters_h
#define ExpensiveCanvasHeuristicParameters_h

namespace blink {

namespace ExpensiveCanvasHeuristicParameters {

enum {
    // Layer promotion heuristic parameters
    //======================================

    // FIXME (crbug.com/463239):
    // The Layer promotion heuristics should go away after slimming paint
    // is completely phased in and display list canvases are modified to
    // use a lightweight layering primitive instead of the
    // SkCanvas::saveLayer.

    // Heuristic: Canvases that are overdrawn beyond this factor in a
    // single frame are promoted to a direct composited layer so that
    // their contents not be re-rasterized by the compositor when the
    // containing layer is the object of a paint invalidation.
    ExpensiveOverdrawThreshold = 3,

    ExpensivePathPointCount = 50,

    SVGImageSourcesAreExpensive = 1,

    ConcavePathsAreExpensive = 1,

    ComplexClipsAreExpensive = 1,

    BlurredShadowsAreExpensive = 1,

    // Display list fallback heuristic parameters
    //============================================

    // Frames ending with more than this number of levels remaining
    // on the state stack at the end of a frame are too expensive to
    // remain in display list mode. This criterion is motivated by an
    // O(N) cost in carying over state from one frame to the next when
    // in display list mode. The value of this parameter should be high
    // enough to almost never kick in other than for cases with unmatched
    // save()/restore() calls are low enough to kick in before state
    // management becomes measurably expensive.
    ExpensiveRecordingStackDepth = 50,

}; // enum

} // ExpensiveCanvasHeuristicParameters

} // blink

#endif
