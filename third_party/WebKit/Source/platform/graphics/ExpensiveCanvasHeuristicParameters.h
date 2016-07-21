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

    // Heuristic: When drawing a source image that has more pixels than
    // the destination canvas by the following factor or more, the draw
    // is considered expensive.
    ExpensiveImageSizeRatio = 4,

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

    // GPU vs. display list heuristic parameters
    //===========================================

    // Pixel count beyond which we should always prefer to use display
    // lists. Rationale: The allocation of large textures for canvas
    // tends to starve the compositor, and increase the probability of
    // failure of subsequent allocations required for double buffering.
    PreferDisplayListOverGpuSizeThreshold = 4096 * 4096,

    // Disable Acceleration heuristic parameters
    //===========================================

    GetImageDataForcesNoAcceleration = 1,

}; // enum


// Constants and Coefficients for 2D Canvas Dynamic Rendering Mode Switching
// =========================================================================

// Approximate relative costs of different types of operations for the
// accelerated rendering pipeline and the recording rendering pipeline.
// These costs were estimated experimentally based on the performance
// of each pipeline in the animometer benchmark. Multiple factors influence
// the true cost of each type of operation, including:
// - The hardware configuration
// - Version of the project
// - Additional details about the operation:
//   - Subtype of operation (png vs svg image, arc vs line...)
//   - Scale
//   - Associated effects (patterns, gradients...)
// The coefficients are equal to 1/n, where n is equal to the number of calls
// of a type that can be performed in each frame while maintaining
// 50 frames per second. They were estimated using the animometer benchmark.

const double AcceleratedDrawPathApproximateCost     = 0.004;
const double AcceleratedGetImageDataApproximateCost = 0.1;
const double AcceleratedDrawImageApproximateCost    = 0.002;

const double RecordingDrawPathApproximateCost           = 0.0014;
const double UnacceleratedGetImageDataApproximateCost   = 0.001; // This cost is for non-display-list mode after fallback.
const double RecordingDrawImageApproximateCost          = 0.004;

// Coefficient used in the isAccelerationOptimalForCanvasContent
// heuristic to create a bias in the output.
// If set to a value greater than 1, it creates a bias towards suggesting acceleration.
// If set to a value smaller than 1, it creates a bias towards not suggesting acceleration
// For example, if its value is 1.5, then disabling gpu acceleration will only be suggested if
// recordingCost * 1.5 < acceleratedCost.
const double AcceleratedHeuristicBias = 1.5;

// Minimum number of frames that need to be rendered
// before the rendering pipeline may be switched. Having this set
// to more than 1 increases the sample size of usage data before a
// decision is made, improving the accuracy of heuristics.
const int MinFramesBeforeSwitch = 3;

} // namespace ExpensiveCanvasHeuristicParameters

} // namespace blink

#endif
