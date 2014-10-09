// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PaintInvalidationReason_h
#define PaintInvalidationReason_h

#include "platform/PlatformExport.h"

namespace blink {

enum PaintInvalidationReason {
    PaintInvalidationNone,
    PaintInvalidationIncremental,
    PaintInvalidationRectangle,
    // The following reasons will all cause full invalidation of the RenderObject.
    PaintInvalidationFull, // Any unspecified reason of full invalidation.
    PaintInvalidationBorderBoxChange,
    PaintInvalidationBoundsChange,
    PaintInvalidationLocationChange,
    PaintInvalidationBecameVisible,
    PaintInvalidationBecameInvisible,
    PaintInvalidationScroll,
    PaintInvalidationSelection,
    PaintInvalidationLayer,
    PaintInvalidationRendererInsertion,
    PaintInvalidationRendererRemoval
};

PLATFORM_EXPORT const char* paintInvalidationReasonToString(PaintInvalidationReason);

inline bool isFullPaintInvalidationReason(PaintInvalidationReason reason)
{
    return reason >= PaintInvalidationFull;
}

} // namespace blink

#endif // PaintInvalidationReason_h
