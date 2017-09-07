/*
 * (C) 1999 Lars Knoll (knoll@kde.org)
 * (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004-2009, 2013 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef LayoutText_h
#define LayoutText_h

#include <iterator>
#include "core/CoreExport.h"
#include "core/dom/Text.h"
#include "core/layout/LayoutObject.h"
#include "core/layout/TextRunConstructor.h"
#include "platform/LengthFunctions.h"
#include "platform/wtf/Forward.h"
#include "platform/wtf/RefPtr.h"

namespace blink {

class AbstractInlineTextBox;
class InlineTextBox;

// LayoutText is the root class for anything that represents
// a text node (see core/dom/Text.h).
//
// This is a common node in the tree so to the limit memory overhead,
// this class inherits directly from LayoutObject.
// Also this class is used by both CSS and SVG layouts so LayoutObject
// was a natural choice.
//
// The actual layout of text is handled by the containing inline
// (LayoutInline) or block (LayoutBlockFlow). They will invoke the Unicode
// Bidirectional Algorithm to break the text into actual lines.
// The result of layout is the line box tree, which represents lines
// on the screen. It is stored into m_firstTextBox and m_lastTextBox.
// To understand how lines are broken by the bidi algorithm, read e.g.
// LayoutBlockFlow::layoutInlineChildren.
//
//
// ***** LINE BOXES OWNERSHIP *****
// m_firstTextBox and m_lastTextBox are not owned by LayoutText
// but are pointers into the enclosing inline / block (see LayoutInline's
// and LayoutBlockFlow's m_lineBoxes).
//
//
// This class implements the preferred logical widths computation
// for its underlying text. The widths are stored into m_minWidth
// and m_maxWidth. They are computed lazily based on
// m_preferredLogicalWidthsDirty.
//
// The previous comment applies also for painting. See e.g.
// BlockFlowPainter::paintContents in particular the use of LineBoxListPainter.
class CORE_EXPORT LayoutText : public LayoutObject {
 public:
  // FIXME: If the node argument is not a Text node or the string argument is
  // not the content of the Text node, updating text-transform property
  // doesn't re-transform the string.
  LayoutText(Node*, RefPtr<StringImpl>);
#if DCHECK_IS_ON()
  ~LayoutText() override;
#endif

  static LayoutText* CreateEmptyAnonymous(Document&);

  const char* GetName() const override { return "LayoutText"; }

  virtual bool IsTextFragment() const;
  virtual bool IsWordBreak() const;

  virtual RefPtr<StringImpl> OriginalText() const;

  void ExtractTextBox(InlineTextBox*);
  void AttachTextBox(InlineTextBox*);
  void RemoveTextBox(InlineTextBox*);

  const String& GetText() const { return text_; }
  virtual unsigned TextStartOffset() const { return 0; }
  String PlainText() const;

  InlineTextBox* CreateInlineTextBox(int start, unsigned short length);
  void DirtyOrDeleteLineBoxesIfNeeded(bool full_layout);
  void DirtyLineBoxes();

  void AbsoluteRects(Vector<IntRect>&,
                     const LayoutPoint& accumulated_offset) const final;

  void AbsoluteQuads(Vector<FloatQuad>&,
                     MapCoordinatesFlags mode = 0) const final;
  void AbsoluteQuadsForRange(Vector<FloatQuad>&,
                             unsigned start_offset = 0,
                             unsigned end_offset = INT_MAX) const;
  FloatRect LocalBoundingBoxRectForAccessibility() const final;

  enum ClippingOption { kNoClipping, kClipToEllipsis };
  enum LocalOrAbsoluteOption { kLocalQuads, kAbsoluteQuads };
  void Quads(Vector<FloatQuad>&,
             ClippingOption = kNoClipping,
             LocalOrAbsoluteOption = kAbsoluteQuads,
             MapCoordinatesFlags mode = 0) const;

  PositionWithAffinity PositionForPoint(const LayoutPoint&) override;

  bool Is8Bit() const { return text_.Is8Bit(); }
  const LChar* Characters8() const { return text_.Impl()->Characters8(); }
  const UChar* Characters16() const { return text_.Impl()->Characters16(); }
  bool HasEmptyText() const { return text_.IsEmpty(); }
  UChar CharacterAt(unsigned) const;
  UChar UncheckedCharacterAt(unsigned) const;
  UChar operator[](unsigned i) const { return UncheckedCharacterAt(i); }
  UChar32 CodepointAt(unsigned) const;
  unsigned TextLength() const {
    return text_.length();
  }  // non virtual implementation of length()
  bool ContainsOnlyWhitespace(unsigned from, unsigned len) const;
  void PositionLineBox(InlineBox*);

  virtual float Width(unsigned from,
                      unsigned len,
                      const Font&,
                      LayoutUnit x_pos,
                      TextDirection,
                      HashSet<const SimpleFontData*>* fallback_fonts = nullptr,
                      FloatRect* glyph_bounds = nullptr,
                      float expansion = 0) const;
  virtual float Width(unsigned from,
                      unsigned len,
                      LayoutUnit x_pos,
                      TextDirection,
                      bool first_line = false,
                      HashSet<const SimpleFontData*>* fallback_fonts = nullptr,
                      FloatRect* glyph_bounds = nullptr,
                      float expansion = 0) const;

  float MinLogicalWidth() const;
  float MaxLogicalWidth() const;

  void TrimmedPrefWidths(LayoutUnit lead_width,
                         LayoutUnit& first_line_min_width,
                         bool& has_breakable_start,
                         LayoutUnit& last_line_min_width,
                         bool& has_breakable_end,
                         bool& has_breakable_char,
                         bool& has_break,
                         LayoutUnit& first_line_max_width,
                         LayoutUnit& last_line_max_width,
                         LayoutUnit& min_width,
                         LayoutUnit& max_width,
                         bool& strip_front_spaces,
                         TextDirection);

  virtual LayoutRect LinesBoundingBox() const;

  // Returns the bounding box of visual overflow rects of all line boxes.
  LayoutRect VisualOverflowRect() const;

  FloatPoint FirstRunOrigin() const;
  float FirstRunX() const;
  float FirstRunY() const;

  virtual void SetText(RefPtr<StringImpl>, bool force = false);
  void SetTextWithOffset(RefPtr<StringImpl>,
                         unsigned offset,
                         unsigned len,
                         bool force = false);

  // TODO(kojii): setTextInternal() is temporarily public for NGInlineNode.
  // This will be back to protected when NGInlineNode can paint directly.
  virtual void SetTextInternal(RefPtr<StringImpl>);

  virtual void TransformText();

  bool CanBeSelectionLeaf() const override { return true; }
  void SetSelectionState(SelectionState) final;
  LayoutRect LocalSelectionRect() const final;
  LayoutRect LocalCaretRect(
      InlineBox*,
      int caret_offset,
      LayoutUnit* extra_width_to_end_of_line = nullptr) override;

  InlineTextBox* FirstTextBox() const { return first_text_box_; }
  InlineTextBox* LastTextBox() const { return last_text_box_; }

  // True if we have inline text box children which implies rendered text (or
  // whitespace) output.
  bool HasTextBoxes() const { return FirstTextBox(); }

  int CaretMinOffset() const override;
  int CaretMaxOffset() const override;
  unsigned ResolvedTextLength() const;

  bool ContainsReversedText() const { return contains_reversed_text_; }

  bool IsSecure() const {
    return Style()->TextSecurity() != ETextSecurity::kNone;
  }
  void MomentarilyRevealLastTypedCharacter(
      unsigned last_typed_character_offset);

  bool IsAllCollapsibleWhitespace() const;
  bool IsRenderedCharacter(int offset_in_node) const;

  void RemoveAndDestroyTextBoxes();

  RefPtr<AbstractInlineTextBox> FirstAbstractInlineTextBox();

  float HyphenWidth(const Font&, TextDirection);

  LayoutRect DebugRect() const override;

  void AutosizingMultiplerChanged() {
    known_to_have_no_overflow_and_no_fallback_fonts_ = false;
  }

  virtual UChar PreviousCharacter() const;

 protected:
  void WillBeDestroyed() override;

  void StyleWillChange(StyleDifference, const ComputedStyle&) final {}
  void StyleDidChange(StyleDifference, const ComputedStyle* old_style) override;

  void AddLayerHitTestRects(
      LayerHitTestRects&,
      const PaintLayer* current_layer,
      const LayoutPoint& layer_offset,
      TouchAction supported_fast_actions,
      const LayoutRect& container_rect,
      TouchAction container_whitelisted_touch_action) const override;

  virtual InlineTextBox* CreateTextBox(
      int start,
      unsigned short length);  // Subclassed by SVG.

  void InvalidateDisplayItemClients(PaintInvalidationReason) const override;

 private:
  void ComputePreferredLogicalWidths(float lead_width);
  void ComputePreferredLogicalWidths(
      float lead_width,
      HashSet<const SimpleFontData*>& fallback_fonts,
      FloatRect& glyph_bounds);

  // Make length() private so that callers that have a LayoutText*
  // will use the more efficient textLength() instead, while
  // callers with a LayoutObject* can continue to use length().
  unsigned length() const final { return TextLength(); }

  // See the class comment as to why we shouldn't call this function directly.
  void Paint(const PaintInfo&, const LayoutPoint&) const final { NOTREACHED(); }
  void UpdateLayout() final { NOTREACHED(); }
  bool NodeAtPoint(HitTestResult&,
                   const HitTestLocation&,
                   const LayoutPoint&,
                   HitTestAction) final {
    NOTREACHED();
    return false;
  }

  void DeleteTextBoxes();
  float WidthFromFont(const Font&,
                      int start,
                      int len,
                      float lead_width,
                      float text_width_so_far,
                      TextDirection,
                      HashSet<const SimpleFontData*>* fallback_fonts,
                      FloatRect* glyph_bounds_accumulation,
                      float expansion = 0) const;

  void SecureText(UChar mask);

  bool IsText() const =
      delete;  // This will catch anyone doing an unnecessary check.

  LayoutRect LocalVisualRect() const override;

  // We put the bitfield first to minimize padding on 64-bit.

  // Whether or not we can be broken into multiple lines.
  bool has_breakable_char_ : 1;
  // Whether or not we have a hard break (e.g., <pre> with '\n').
  bool has_break_ : 1;
  // Whether or not we have a variable width tab character (e.g., <pre> with
  // '\t').
  bool has_tab_ : 1;
  bool has_breakable_start_ : 1;
  bool has_breakable_end_ : 1;
  bool has_end_white_space_ : 1;
  // This bit indicates that the text run has already dirtied specific line
  // boxes, and this hint will enable layoutInlineChildren to avoid just
  // dirtying everything when character data is modified (e.g., appended/
  // inserted or removed).
  bool lines_dirty_ : 1;
  bool contains_reversed_text_ : 1;
  mutable bool known_to_have_no_overflow_and_no_fallback_fonts_ : 1;

  float min_width_;
  float max_width_;
  float first_line_min_width_;
  float last_line_line_min_width_;

  String text_;

  // The line boxes associated with this object.
  // Read the LINE BOXES OWNERSHIP section in the class header comment.
  InlineTextBox* first_text_box_;
  InlineTextBox* last_text_box_;
};

inline UChar LayoutText::UncheckedCharacterAt(unsigned i) const {
  SECURITY_DCHECK(i < TextLength());
  return Is8Bit() ? Characters8()[i] : Characters16()[i];
}

inline UChar LayoutText::CharacterAt(unsigned i) const {
  if (i >= TextLength())
    return 0;

  return UncheckedCharacterAt(i);
}

inline UChar32 LayoutText::CodepointAt(unsigned i) const {
  if (i >= TextLength())
    return 0;
  if (Is8Bit())
    return Characters8()[i];
  UChar32 c;
  U16_GET(Characters16(), 0, i, TextLength(), c);
  return c;
}

inline float LayoutText::HyphenWidth(const Font& font,
                                     TextDirection direction) {
  const ComputedStyle& style = StyleRef();
  return font.Width(ConstructTextRun(font, style.HyphenString().GetString(),
                                     style, direction));
}

DEFINE_LAYOUT_OBJECT_TYPE_CASTS(LayoutText, IsText());

inline LayoutText* Text::GetLayoutObject() const {
  return ToLayoutText(CharacterData::GetLayoutObject());
}

// Represents list of |InlineTextBox| objects associated to |LayoutText| in
// layout order.
class InlineTextBoxRange {
 public:
  class Iterator
      : public std::iterator<std::input_iterator_tag, InlineTextBox*> {
   public:
    explicit Iterator(InlineTextBox*);
    Iterator(const Iterator&) = default;

    Iterator& operator++();
    InlineTextBox* operator*() const;

    bool operator==(const Iterator& other) const {
      return current_ == other.current_;
    }
    bool operator!=(const Iterator& other) const { return !operator==(other); }

   private:
    InlineTextBox* current_;
  };

  explicit InlineTextBoxRange(const LayoutText&);

  Iterator begin() const { return Iterator(layout_text_->FirstTextBox()); }
  Iterator end() const { return Iterator(nullptr); }

 private:
  const LayoutText* layout_text_;
};

InlineTextBoxRange InlineTextBoxesOf(const LayoutText&);

void ApplyTextTransform(const ComputedStyle*, String&, UChar);

}  // namespace blink

#endif  // LayoutText_h
