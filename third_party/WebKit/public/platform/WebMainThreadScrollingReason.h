// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebMainThreadScrollingReason_h
#define WebMainThreadScrollingReason_h

#include "WebCommon.h"

namespace blink {

// Ensure this stays in sync with cc::MainThreadScrollingReason.
namespace WebMainThreadScrollingReason {
enum WebMainThreadScrollingReason {
    NotScrollingOnMain = 0,
    HasBackgroundAttachmentFixedObjects = 1 << 0,
    HasNonLayerViewportConstrainedObjects = 1 << 1,
    ThreadedScrollingDisabled = 1 << 2,
    ScrollBarScrolling = 1 << 3,
    PageOverlay = 1 << 4
};
} // namespace MainThreadScrollingReason

} // namespace blink

#endif
