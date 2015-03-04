// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ExpensiveCanvasHeuristicParameters_h
#define ExpensiveCanvasHeuristicParameters_h

namespace blink {

namespace ExpensiveCanvasHeuristicParameters {

// FIXME (crbug.com/463239):
// The ExpensiveCanvasHeuristic should go away after slimming paint is
// completely phased in and display list canvases are modified to use
// a lightweight layering primitive instead of the SkCanvas::saveLayer.

enum {
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
}; // enum

} // ExpensiveCanvasHeuristicParameters

} // blink

#endif
