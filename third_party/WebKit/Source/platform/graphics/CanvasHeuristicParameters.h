// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CanvasHeuristicParameters_h
#define CanvasHeuristicParameters_h

namespace blink {

namespace CanvasHeuristicParameters {

enum {
  // Disable Deferral overdraw parameters
  //======================================

  // Heuristic: Canvases that are overdrawn beyond this factor in a
  // single frame will be disabled deferral.
  kExpensiveOverdrawThreshold = 10,

  // Disable Acceleration heuristic parameters
  //===========================================

  // When drawing very large images to canvases, there is a point where
  // GPU acceleration becomes inefficient due to texture upload overhead,
  // especially when the image is large enough that it is likely to
  // monopolize the texture cache, and when it is being downsized to the
  // point that few of the upload texels are actually sampled. When both
  // of these conditions are met, we disable acceleration.
  kDrawImageTextureUploadSoftSizeLimit = 4096 * 4096,
  kDrawImageTextureUploadSoftSizeLimitScaleThreshold = 4,
  kDrawImageTextureUploadHardSizeLimit = 8192 * 8192,

  // GPU readback prevention heuristics
  //====================================

  kGPUReadbackForcesNoAcceleration = 1,

  // When gpu readback is successively invoked in following number of frames,
  // we disable gpu acceleration to avoid the high cost of gpu readback.
  kGPUReadbackMinSuccessiveFrames = 3,

  // When a canvas is used as a source image, if its destination is
  // non-accelerated and the source canvas is accelerated, a readback
  // from the gpu is necessary. This option causes the source canvas to
  // switch to non-accelerated when this situation is encountered to
  // prevent future canvas-to-canvas draws from requiring a readback.
  kDisableAccelerationToAvoidReadbacks = 0,

  // See description of DisableAccelerationToAvoidReadbacks. This is the
  // opposite strategy : accelerate the destination canvas. If both
  // EnableAccelerationToAvoidReadbacks and
  // DisableAccelerationToAvoidReadbacks are specified, we try to enable
  // acceleration on the destination first. If that does not succeed,
  // we disable acceleration on the source canvas. Either way, future
  // readbacks are prevented.
  kEnableAccelerationToAvoidReadbacks = 1,

};  // enum

}  // namespace CanvasHeuristicParameters

}  // namespace blink

#endif
