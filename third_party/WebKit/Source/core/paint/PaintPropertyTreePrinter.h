// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PaintPropertyTreePrinter_h
#define PaintPropertyTreePrinter_h

#include "core/CoreExport.h"
#include "platform/wtf/text/WTFString.h"

#if DCHECK_IS_ON()

namespace blink {

class LocalFrameView;

}  // namespace blink

// Outside the blink namespace for ease of invocation from gdb.
CORE_EXPORT void showAllPropertyTrees(const blink::LocalFrameView& rootFrame);
CORE_EXPORT void showTransformPropertyTree(
    const blink::LocalFrameView& rootFrame);
CORE_EXPORT void showClipPropertyTree(const blink::LocalFrameView& rootFrame);
CORE_EXPORT void showEffectPropertyTree(const blink::LocalFrameView& rootFrame);
CORE_EXPORT void showScrollPropertyTree(const blink::LocalFrameView& rootFrame);
CORE_EXPORT String
transformPropertyTreeAsString(const blink::LocalFrameView& rootFrame);
CORE_EXPORT String
clipPropertyTreeAsString(const blink::LocalFrameView& rootFrame);
CORE_EXPORT String
effectPropertyTreeAsString(const blink::LocalFrameView& rootFrame);
CORE_EXPORT String
scrollPropertyTreeAsString(const blink::LocalFrameView& rootFrame);

CORE_EXPORT String paintPropertyTreeGraph(const blink::LocalFrameView&);

#endif  // if DCHECK_IS_ON()

#endif  // PaintPropertyTreePrinter_h
