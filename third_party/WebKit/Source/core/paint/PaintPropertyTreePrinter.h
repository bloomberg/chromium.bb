// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PaintPropertyTreePrinter_h
#define PaintPropertyTreePrinter_h

#include "core/CoreExport.h"
#include "wtf/text/WTFString.h"

#ifndef NDEBUG

namespace blink {

class ClipPaintPropertyNode;
class FrameView;
class EffectPaintPropertyNode;
class TransformPaintPropertyNode;
class ScrollPaintPropertyNode;

} // namespace blink

// Outside the blink namespace for ease of invocation from gdb.
CORE_EXPORT void showTransformPropertyTree(const blink::FrameView& rootFrame);
CORE_EXPORT void showClipPropertyTree(const blink::FrameView& rootFrame);
CORE_EXPORT void showEffectPropertyTree(const blink::FrameView& rootFrame);
CORE_EXPORT void showScrollPropertyTree(const blink::FrameView& rootFrame);
CORE_EXPORT void showPaintPropertyPath(const blink::TransformPaintPropertyNode*);
CORE_EXPORT void showPaintPropertyPath(const blink::ClipPaintPropertyNode*);
CORE_EXPORT void showPaintPropertyPath(const blink::EffectPaintPropertyNode*);
CORE_EXPORT void showPaintPropertyPath(const blink::ScrollPaintPropertyNode*);
CORE_EXPORT String paintPropertyTreeGraph(const blink::FrameView&);

#endif // ifndef NDEBUG

#endif // PaintPropertyTreePrinter_h
