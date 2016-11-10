// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGInlineBox_h
#define NGInlineBox_h

#include "core/CoreExport.h"
#include "platform/fonts/FontFallbackPriority.h"
#include "platform/fonts/shaping/ShapeResult.h"
#include "platform/heap/Handle.h"
#include "platform/text/TextDirection.h"
#include "wtf/text/WTFString.h"
#include <unicode/uscript.h>

namespace blink {

class ComputedStyle;
class LayoutBox;
class LayoutObject;
class NGConstraintSpace;
class NGFragmentBase;
class NGLayoutAlgorithm;
class NGLayoutInlineItem;
class NGPhysicalFragment;
struct MinAndMaxContentSizes;

// Represents an inline node to be laid out.
// TODO(layout-dev): Make this and NGBox inherit from a common class.
class CORE_EXPORT NGInlineBox final
    : public GarbageCollectedFinalized<NGInlineBox> {
 public:
  using const_iterator = Vector<NGLayoutInlineItem>::const_iterator;

  explicit NGInlineBox(LayoutObject* start_inline);

  // Prepare inline and text content for layout. Must be called before
  // calling the Layout method.
  void PrepareLayout();

  // Returns true when done; when this function returns false, it has to be
  // called again. The out parameter will only be set when this function
  // returns true. The same constraint space has to be passed each time.
  // TODO(layout-ng): Should we have a StartLayout function to avoid passing
  // the same space for each Layout iteration?
  bool Layout(const NGConstraintSpace*, NGFragmentBase**);

  NGInlineBox* NextSibling();

  // Iterator for the individual NGLayoutInlineItem objects that make up the
  // inline children of a NGInlineBox instance.
  const_iterator begin() const { return items_.begin(); }
  const_iterator end() const { return items_.end(); }
  String Text(unsigned start_offset, unsigned end_offset) const {
    return text_content_.substring(start_offset, end_offset);
  }

  DECLARE_VIRTUAL_TRACE();

 private:
  void CollectNode(LayoutObject*, unsigned* offset);
  void CollectInlines(LayoutObject* start, LayoutObject* last);
  void CollapseWhiteSpace();
  void SegmentText();
  void ShapeText();

  LayoutObject* start_inline_;
  LayoutObject* last_inline_;

  Member<NGInlineBox> next_sibling_;
  Member<NGLayoutAlgorithm> layout_algorithm_;

  // Text content for all inline items represented by a single NGInlineBox
  // instance. Encoded either as UTF-16 or latin-1 depending on content.
  String text_content_;
  Vector<NGLayoutInlineItem> items_;
};

// Class representing a single text node or styled inline element with text
// content segmented by style, text direction, sideways rotation, font fallback
// priority (text, symbol, emoji, etc) and script (but not by font).
// In this representation TextNodes are merged up into their parent inline
// element where possible.
class NGLayoutInlineItem {
 public:
  NGLayoutInlineItem(const ComputedStyle* style, unsigned start, unsigned end)
      : start_offset_(start),
        end_offset_(end),
        direction_(LTR),
        script_(USCRIPT_INVALID_CODE),
        style_(style) {
    DCHECK(end >= start);
  }

  unsigned StartOffset() const { return start_offset_; }
  unsigned EndOffset() const { return end_offset_; }
  TextDirection Direection() const { return direction_; }
  UScriptCode Script() const { return script_; }

 private:
  unsigned start_offset_;
  unsigned end_offset_;
  TextDirection direction_;
  UScriptCode script_;
  // FontFallbackPriority fallback_priority_;
  // bool rotate_sideways_;
  const ComputedStyle* style_;
  RefPtr<ShapeResult> shape_result_;

  friend class NGInlineBox;
};

}  // namespace blink

#endif  // NGInlineBox_h
