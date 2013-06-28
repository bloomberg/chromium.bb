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

#include "WebCommon.h"
#include <stdint.h>

namespace WebKit {

// This is a clone of CompositingReasons in RenderLayer.h.
const uint64_t CompositingReasonUnknown                                = 0;
const uint64_t CompositingReason3DTransform                            = static_cast<uint64_t>(1) << 0;
const uint64_t CompositingReasonVideo                                  = static_cast<uint64_t>(1) << 1;
const uint64_t CompositingReasonCanvas                                 = static_cast<uint64_t>(1) << 2;
const uint64_t CompositingReasonPlugin                                 = static_cast<uint64_t>(1) << 3;
const uint64_t CompositingReasonIFrame                                 = static_cast<uint64_t>(1) << 4;
const uint64_t CompositingReasonBackfaceVisibilityHidden               = static_cast<uint64_t>(1) << 5;
const uint64_t CompositingReasonAnimation                              = static_cast<uint64_t>(1) << 6;
const uint64_t CompositingReasonFilters                                = static_cast<uint64_t>(1) << 7;
const uint64_t CompositingReasonPositionFixed                          = static_cast<uint64_t>(1) << 8;
const uint64_t CompositingReasonPositionSticky                         = static_cast<uint64_t>(1) << 9;
const uint64_t CompositingReasonOverflowScrollingTouch                 = static_cast<uint64_t>(1) << 10;
const uint64_t CompositingReasonBlending                               = static_cast<uint64_t>(1) << 11;
const uint64_t CompositingReasonAssumedOverlap                         = static_cast<uint64_t>(1) << 12;
const uint64_t CompositingReasonOverlap                                = static_cast<uint64_t>(1) << 13;
const uint64_t CompositingReasonNegativeZIndexChildren                 = static_cast<uint64_t>(1) << 14;
const uint64_t CompositingReasonTransformWithCompositedDescendants     = static_cast<uint64_t>(1) << 15;
const uint64_t CompositingReasonOpacityWithCompositedDescendants       = static_cast<uint64_t>(1) << 16;
const uint64_t CompositingReasonMaskWithCompositedDescendants          = static_cast<uint64_t>(1) << 17;
const uint64_t CompositingReasonReflectionWithCompositedDescendants    = static_cast<uint64_t>(1) << 18;
const uint64_t CompositingReasonFilterWithCompositedDescendants        = static_cast<uint64_t>(1) << 19;
const uint64_t CompositingReasonBlendingWithCompositedDescendants      = static_cast<uint64_t>(1) << 20;
const uint64_t CompositingReasonClipsCompositingDescendants            = static_cast<uint64_t>(1) << 21;
const uint64_t CompositingReasonPerspective                            = static_cast<uint64_t>(1) << 22;
const uint64_t CompositingReasonPreserve3D                             = static_cast<uint64_t>(1) << 23;
const uint64_t CompositingReasonReflectionOfCompositedParent           = static_cast<uint64_t>(1) << 24;
const uint64_t CompositingReasonRoot                                   = static_cast<uint64_t>(1) << 25;
const uint64_t CompositingReasonLayerForClip                           = static_cast<uint64_t>(1) << 26;
const uint64_t CompositingReasonLayerForScrollbar                      = static_cast<uint64_t>(1) << 27;
const uint64_t CompositingReasonLayerForScrollingContainer             = static_cast<uint64_t>(1) << 28;
const uint64_t CompositingReasonLayerForForeground                     = static_cast<uint64_t>(1) << 29;
const uint64_t CompositingReasonLayerForBackground                     = static_cast<uint64_t>(1) << 30;
const uint64_t CompositingReasonLayerForMask                           = static_cast<uint64_t>(1) << 31;

typedef uint64_t WebCompositingReasons;

} // namespace WebKit

#endif // WebCompositingReasons_h
