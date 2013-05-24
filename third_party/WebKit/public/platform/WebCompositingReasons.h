/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WebCompositingReasons_h
#define WebCompositingReasons_h

namespace WebKit {

// This is a clone of CompositingReasons enum in RenderLayer.h,
enum {
    CompositingReasonUnknown                                = 0,
    CompositingReason3DTransform                            = 1 << 0,
    CompositingReasonVideo                                  = 1 << 1,
    CompositingReasonCanvas                                 = 1 << 2,
    CompositingReasonPlugin                                 = 1 << 3,
    CompositingReasonIFrame                                 = 1 << 4,
    CompositingReasonBackfaceVisibilityHidden               = 1 << 5,
    CompositingReasonAnimation                              = 1 << 6,
    CompositingReasonFilters                                = 1 << 7,
    CompositingReasonPositionFixed                          = 1 << 8,
    CompositingReasonPositionSticky                         = 1 << 9,
    CompositingReasonOverflowScrollingTouch                 = 1 << 10,
    CompositingReasonBlending                               = 1 << 11,
    CompositingReasonAssumedOverlap                         = 1 << 12,
    CompositingReasonOverlap                                = 1 << 13,
    CompositingReasonNegativeZIndexChildren                 = 1 << 14,
    CompositingReasonTransformWithCompositedDescendants     = 1 << 15,
    CompositingReasonOpacityWithCompositedDescendants       = 1 << 16,
    CompositingReasonMaskWithCompositedDescendants          = 1 << 17,
    CompositingReasonReflectionWithCompositedDescendants    = 1 << 18,
    CompositingReasonFilterWithCompositedDescendants        = 1 << 19,
    CompositingReasonBlendingWithCompositedDescendants      = 1 << 20,
    CompositingReasonClipsCompositingDescendants            = 1 << 21,
    CompositingReasonPerspective                            = 1 << 22,
    CompositingReasonPreserve3D                             = 1 << 23,
    CompositingReasonReflectionOfCompositedParent           = 1 << 24,
    CompositingReasonRoot                                   = 1 << 25,
    CompositingReasonLayerForClip                           = 1 << 26,
    CompositingReasonLayerForScrollbar                      = 1 << 27,
    CompositingReasonLayerForScrollingContainer             = 1 << 28,
    CompositingReasonLayerForForeground                     = 1 << 29,
    CompositingReasonLayerForBackground                     = 1 << 30,
    CompositingReasonLayerForMask                           = 1 << 31,
};

// It is a typedef to allow bitwise operators to be used without type casts.
typedef unsigned WebCompositingReasons;

} // namespace WebKit

#endif // WebCompositingReasons_h
