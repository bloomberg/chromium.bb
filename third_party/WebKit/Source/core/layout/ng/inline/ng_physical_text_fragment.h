// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGPhysicalTextFragment_h
#define NGPhysicalTextFragment_h

#include "core/CoreExport.h"
#include "core/layout/ng/inline/ng_text_end_effect.h"
#include "core/layout/ng/ng_physical_fragment.h"
#include "platform/fonts/FontBaseline.h"
#include "platform/fonts/NGTextFragmentPaintInfo.h"
#include "platform/fonts/shaping/ShapeResult.h"
#include "platform/wtf/text/StringView.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

class ShapeResult;
struct NGPhysicalOffsetRect;

// In CSS Writing Modes Levle 4, line orientation for layout and line
// orientation for paint are not always the same.
//
// Specifically, 'sideways-lr' typesets as if lines are horizontal flow, but
// rotates counterclockwise.
enum class NGLineOrientation {
  // Lines are horizontal.
  kHorizontal,
  // Lines are vertical, rotated clockwise. Inside of the line, it may be
  // typeset using vertical characteristics, horizontal characteristics, or
  // mixed. Lines flow left to right, or right to left.
  kClockWiseVertical,
  // Lines are vertical, rotated counterclockwise. Inside of the line is typeset
  // as if horizontal flow. Lines flow left to right.
  kCounterClockWiseVertical

  // When adding new values, ensure NGPhysicalTextFragment has enough bits.
};

class CORE_EXPORT NGPhysicalTextFragment final : public NGPhysicalFragment {
 public:
  NGPhysicalTextFragment(LayoutObject* layout_object,
                         const ComputedStyle& style,
                         const String& text,
                         unsigned item_index,
                         unsigned start_offset,
                         unsigned end_offset,
                         NGPhysicalSize size,
                         int expansion,
                         NGLineOrientation line_orientation,
                         NGTextEndEffect end_effect,
                         scoped_refptr<const ShapeResult> shape_result)
      : NGPhysicalFragment(layout_object, style, size, kFragmentText),
        text_(text),
        item_index_(item_index),
        start_offset_(start_offset),
        end_offset_(end_offset),
        shape_result_(shape_result),
        expansion_(expansion),
        line_orientation_(static_cast<unsigned>(line_orientation)),
        end_effect_(static_cast<unsigned>(end_effect)) {}

  unsigned Length() const { return end_offset_ - start_offset_; }
  StringView Text() const { return StringView(text_, start_offset_, Length()); }

  const ShapeResult* TextShapeResult() const { return shape_result_.get(); }

  // Deprecating ItemIndex in favor of storing and accessing each component;
  // e.g., text, style, ShapeResult, etc. Currently used for CreateBidiRuns and
  // tests.
  unsigned ItemIndexDeprecated() const { return item_index_; }

  // Start/end offset to the text of the block container.
  unsigned StartOffset() const { return start_offset_; }
  unsigned EndOffset() const { return end_offset_; }

  // The amount of expansion for justification.
  // Not used in NG paint, only to copy to InlineTextBox::SetExpansion().
  // TODO(layout-dev): crbug.com/714962 Remove once fragment painting is enabled
  // by default.
  int Expansion() const { return expansion_; }

  NGLineOrientation LineOrientation() const {
    return static_cast<NGLineOrientation>(line_orientation_);
  }
  bool IsHorizontal() const {
    return LineOrientation() == NGLineOrientation::kHorizontal;
  }
  FontBaseline BaselineType() const {
    return IsHorizontal() ? kAlphabeticBaseline : kIdeographicBaseline;
  }

  // The visual bounding box that includes glpyh bounding box and CSS
  // properties, in local coordinates.
  NGPhysicalOffsetRect SelfVisualRect() const;

  NGTextEndEffect EndEffect() const {
    return static_cast<NGTextEndEffect>(end_effect_);
  }

  scoped_refptr<NGPhysicalFragment> CloneWithoutOffset() const {
    return base::AdoptRef(new NGPhysicalTextFragment(
        layout_object_, Style(), text_, item_index_, start_offset_, end_offset_,
        size_, expansion_, LineOrientation(), EndEffect(), shape_result_));
  }

  NGTextFragmentPaintInfo PaintInfo() const {
    return NGTextFragmentPaintInfo{text_, StartOffset(), EndOffset(),
                                   TextShapeResult()};
  }

  bool IsLineBreak() const;

  // Returns true if the text is generated (from, e.g., list marker,
  // pseudo-element, ...) instead of from a DOM text node.
  bool IsAnonymousText() const;

  // Returns the text offset in the fragment placed closest to the given point.
  unsigned TextOffsetForPoint(const NGPhysicalOffset&) const;

  PositionWithAffinity PositionForPoint(const NGPhysicalOffset&) const override;

 private:
  // The text of NGInlineNode; i.e., of a parent block. The text for this
  // fragment is a substring(start_offset_, end_offset_) of this string.
  const String text_;

  // Deprecating, ItemIndexDeprecated().
  unsigned item_index_;

  // Start and end offset of the parent block text.
  unsigned start_offset_;
  unsigned end_offset_;

  scoped_refptr<const ShapeResult> shape_result_;

  int expansion_;

  unsigned line_orientation_ : 2;  // NGLineOrientation
  unsigned end_effect_ : 1;        // NGTextEndEffect
};

DEFINE_TYPE_CASTS(NGPhysicalTextFragment,
                  NGPhysicalFragment,
                  fragment,
                  fragment->IsText(),
                  fragment.IsText());

}  // namespace blink

#endif  // NGPhysicalTextFragment_h
