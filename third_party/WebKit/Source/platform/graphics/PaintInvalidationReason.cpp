// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "platform/graphics/PaintInvalidationReason.h"

#include "wtf/Assertions.h"

namespace blink {

const char* paintInvalidationReasonToString(PaintInvalidationReason reason)
{
    switch (reason) {
    case PaintInvalidationNone:
        return "none";
    case PaintInvalidationIncremental:
        return "incremental";
    case PaintInvalidationRectangle:
        return "invalidate paint rectangle";
    case PaintInvalidationFull:
        return "full";
    case PaintInvalidationBorderBoxChange:
        return "border box change";
    case PaintInvalidationBoundsChange:
        return "bounds change";
    case PaintInvalidationLocationChange:
        return "location change";
    case PaintInvalidationBecameVisible:
        return "became visible";
    case PaintInvalidationBecameInvisible:
        return "became invisible";
    case PaintInvalidationScroll:
        return "scroll";
    case PaintInvalidationSelection:
        return "selection";
    case PaintInvalidationLayer:
        return "layer";
    case PaintInvalidationRendererInsertion:
        return "renderer insertion";
    case PaintInvalidationRendererRemoval:
        return "renderer removal";
    }
    ASSERT_NOT_REACHED();
    return "";
}

}
