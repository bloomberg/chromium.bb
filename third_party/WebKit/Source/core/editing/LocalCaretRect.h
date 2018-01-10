// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LocalCaretRect_h
#define LocalCaretRect_h

#include "core/CoreExport.h"
#include "core/editing/Forward.h"
#include "platform/geometry/LayoutRect.h"

namespace blink {

class LayoutObject;

// A transient struct representing a caret rect local to |layout_object|.
struct LocalCaretRect {
  STACK_ALLOCATED();

  const LayoutObject* layout_object = nullptr;
  LayoutRect rect;

  LocalCaretRect() = default;
  LocalCaretRect(const LayoutObject* layout_object, const LayoutRect& rect)
      : layout_object(layout_object), rect(rect) {}

  bool IsEmpty() const { return !layout_object || rect.IsEmpty(); }
};

// Rect is local to the returned layoutObject
CORE_EXPORT LocalCaretRect
LocalCaretRectOfPosition(const PositionWithAffinity&);
CORE_EXPORT LocalCaretRect
LocalCaretRectOfPosition(const PositionInFlatTreeWithAffinity&);

LocalCaretRect LocalSelectionRectOfPosition(const PositionWithAffinity&);

}  // namespace blink

#endif  // LocalCaretRect_h
