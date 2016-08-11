// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PaintPropertyTreePrinter_h
#define PaintPropertyTreePrinter_h

#include "core/CoreExport.h"

#ifndef NDEBUG
namespace blink {

class FrameView;

CORE_EXPORT void showTransformPropertyTree(const FrameView& rootFrame);
CORE_EXPORT void showClipPropertyTree(const FrameView& rootFrame);
CORE_EXPORT void showEffectPropertyTree(const FrameView& rootFrame);

} // namespace blink
#endif

#endif // PaintPropertyTreePrinter_h
