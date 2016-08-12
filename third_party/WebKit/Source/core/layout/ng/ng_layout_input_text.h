// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGLayoutInputText_h
#define NGLayoutInputText_h

#include "core/CoreExport.h"
#include "core/style/ComputedStyle.h"
#include "platform/fonts/shaping/CachingWordShaper.h"
#include "platform/fonts/shaping/ShapeResult.h"
#include "platform/heap/Handle.h"
#include "wtf/Vector.h"
#include "wtf/text/WTFString.h"

namespace blink {

// Struct representing a single text node or styled inline element with text
// content. In this representation TextNodes are merged up into their parent
// inline element where possible.
struct NGLayoutInputTextItem {
  unsigned start_offset;
  unsigned end_offset;
  RefPtr<ComputedStyle> style;
};

// Class representing all text content for a given inline layout root in the
// layout input tree.
// The content is stored as a single string with collapsed white-space (if
// applicable) to allow cross-element shaping. Each item contains a start and
// end offset representing the content of said item.
class CORE_EXPORT NGLayoutInputText final
    : public GarbageCollectedFinalized<NGLayoutInputText> {
 public:
  using const_iterator = Vector<NGLayoutInputTextItem>::const_iterator;

  NGLayoutInputText() {}

  void Shape();
  const_iterator begin() const { return items_.begin(); }
  const_iterator end() const { return items_.end(); }
  String Text(unsigned start_offset, unsigned end_offset) const {
    return text_.substring(start_offset, end_offset);
  }

  DEFINE_INLINE_TRACE() {}

 private:
  // TODO(eae): This should probably always be utf-8 to reduce the
  // memory and conversion overhead. We can't really re-use the
  // TextContent string as this is stored post white-space
  // collapsing. Also note that shaping can be done in either
  // utf-8 or utf-16. Currently we always shape in utf16 and
  // upconvert latin-1.
  String text_;
  RefPtr<ShapeResult> shape_result_;
  Vector<NGLayoutInputTextItem> items_;
};

}  // namespace blink

#endif  // NGLayoutInputText_h
