// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGInlineItem_h
#define NGInlineItem_h

#include "core/CoreExport.h"
#include "platform/LayoutUnit.h"
#include "platform/fonts/FontFallbackPriority.h"
#include "platform/fonts/SimpleFontData.h"
#include "platform/fonts/shaping/ShapeResult.h"
#include "platform/text/TextDirection.h"
#include "platform/wtf/HashSet.h"

#include <unicode/ubidi.h>

namespace blink {

class ComputedStyle;
class LayoutObject;

// Class representing a single text node or styled inline element with text
// content segmented by style, text direction, sideways rotation, font fallback
// priority (text, symbol, emoji, etc), and script (but not by font).
// In this representation TextNodes are merged up into their parent inline
// element where possible.
class CORE_EXPORT NGInlineItem {
 public:
  enum NGInlineItemType {
    kText,
    kControl,
    kAtomicInline,
    kOpenTag,
    kCloseTag,
    kFloating,
    kOutOfFlowPositioned,
    kBidiControl
    // When adding new values, make sure the bit size of |type_| is large
    // enough to store.
  };

  // Whether pre- and post-context should be used for shaping.
  enum NGLayoutInlineShapeOptions {
    kNoContext = 0,
    kPreContext = 1,
    kPostContext = 2
  };

  // The constructor and destructor can't be implicit or inlined, because they
  // require full definition of ComputedStyle.
  NGInlineItem(NGInlineItemType type,
               unsigned start,
               unsigned end,
               const ComputedStyle* style = nullptr,
               LayoutObject* layout_object = nullptr);
  ~NGInlineItem();

  NGInlineItemType Type() const { return static_cast<NGInlineItemType>(type_); }
  const char* NGInlineItemTypeToString(int val) const;

  const ShapeResult* TextShapeResult() const { return shape_result_.Get(); }
  NGLayoutInlineShapeOptions ShapeOptions() const {
    return static_cast<NGLayoutInlineShapeOptions>(shape_options_);
  }

  unsigned StartOffset() const { return start_offset_; }
  unsigned EndOffset() const { return end_offset_; }
  unsigned Length() const { return end_offset_ - start_offset_; }
  TextDirection Direction() const { return DirectionFromLevel(BidiLevel()); }
  UBiDiLevel BidiLevel() const { return static_cast<UBiDiLevel>(bidi_level_); }
  UScriptCode GetScript() const { return script_; }
  const ComputedStyle* Style() const { return style_.Get(); }
  LayoutObject* GetLayoutObject() const { return layout_object_; }

  void SetOffset(unsigned start, unsigned end);
  void SetEndOffset(unsigned);

  LayoutUnit InlineSize() const;
  LayoutUnit InlineSize(unsigned start, unsigned end) const;

  bool HasStartEdge() const;
  bool HasEndEdge() const;

  static void Split(Vector<NGInlineItem>&, unsigned index, unsigned offset);
  static unsigned SetBidiLevel(Vector<NGInlineItem>&,
                               unsigned index,
                               unsigned end_offset,
                               UBiDiLevel);

  void AssertOffset(unsigned offset) const;
  void AssertEndOffset(unsigned offset) const;

  String ToString() const;

 private:
  unsigned start_offset_;
  unsigned end_offset_;
  UScriptCode script_;
  RefPtr<const ShapeResult> shape_result_;
  RefPtr<const ComputedStyle> style_;
  LayoutObject* layout_object_;

  unsigned type_ : 3;
  unsigned bidi_level_ : 8;  // UBiDiLevel is defined as uint8_t.
  unsigned shape_options_ : 2;
  unsigned rotate_sideways_ : 1;

  // TODO(layout-ng): Do we need fallback_priority_ here? If so we should pack
  // it with the bit field above.
  FontFallbackPriority fallback_priority_;

  friend class NGInlineNode;
};

inline void NGInlineItem::AssertOffset(unsigned offset) const {
  DCHECK((offset >= start_offset_ && offset < end_offset_) ||
         (offset == start_offset_ && start_offset_ == end_offset_));
}

inline void NGInlineItem::AssertEndOffset(unsigned offset) const {
  DCHECK_GE(offset, start_offset_);
  DCHECK_LE(offset, end_offset_);
}

// A vector-like object that points to a subset of an array of |NGInlineItem|.
// The source vector must keep alive and must not resize while this object
// is alive.
class NGInlineItemRange {
  STACK_ALLOCATED();

 public:
  NGInlineItemRange(Vector<NGInlineItem>*,
                    unsigned start_index,
                    unsigned end_index);

  unsigned StartIndex() const { return start_index_; }
  unsigned EndIndex() const { return start_index_ + size_; }
  unsigned Size() const { return size_; }

  NGInlineItem& operator[](unsigned index) {
    CHECK_LT(index, size_);
    return start_item_[index];
  }
  const NGInlineItem& operator[](unsigned index) const {
    CHECK_LT(index, size_);
    return start_item_[index];
  }

  using iterator = NGInlineItem*;
  using const_iterator = const NGInlineItem*;
  iterator begin() { return start_item_; }
  iterator end() { return start_item_ + size_; }
  const_iterator begin() const { return start_item_; }
  const_iterator end() const { return start_item_ + size_; }

 private:
  NGInlineItem* start_item_;
  unsigned size_;
  unsigned start_index_;
};

}  // namespace blink

#endif  // NGInlineItem_h
