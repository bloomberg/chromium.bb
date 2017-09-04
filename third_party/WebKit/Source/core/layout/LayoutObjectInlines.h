// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LayoutObjectInlines_h
#define LayoutObjectInlines_h

#include "core/css/StyleEngine.h"
#include "core/layout/LayoutObject.h"

namespace blink {

// The following methods are inlined for performance but not put in
// LayoutObject.h because that would unnecessarily tie LayoutObject.h
// to StyleEngine.h for all users of LayoutObject.h that don't use
// these methods.

inline const ComputedStyle* LayoutObject::FirstLineStyle() const {
  return GetDocument().GetStyleEngine().UsesFirstLineRules()
             ? CachedFirstLineStyle()
             : Style();
}

inline const ComputedStyle& LayoutObject::FirstLineStyleRef() const {
  const ComputedStyle* style = FirstLineStyle();
  DCHECK(style);
  return *style;
}

inline const ComputedStyle* LayoutObject::Style(bool first_line) const {
  return first_line ? FirstLineStyle() : Style();
}

inline const ComputedStyle& LayoutObject::StyleRef(bool first_line) const {
  const ComputedStyle* style = this->Style(first_line);
  DCHECK(style);
  return *style;
}

}  // namespace blink

#endif
