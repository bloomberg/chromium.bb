// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LineLayoutText_h
#define LineLayoutText_h

#include "core/layout/LayoutText.h"
#include "core/layout/api/LineLayoutItem.h"
#include "platform/LayoutUnit.h"
#include "platform/wtf/Forward.h"

namespace blink {

class LayoutText;

class LineLayoutText : public LineLayoutItem {
 public:
  explicit LineLayoutText(LayoutText* layout_object)
      : LineLayoutItem(layout_object) {}

  explicit LineLayoutText(const LineLayoutItem& item) : LineLayoutItem(item) {
    SECURITY_DCHECK(!item || item.IsText());
  }

  explicit LineLayoutText(std::nullptr_t) : LineLayoutItem(nullptr) {}

  LineLayoutText() {}

  InlineTextBox* FirstTextBox() const { return ToText()->FirstTextBox(); }

  InlineTextBox* LastTextBox() const { return ToText()->LastTextBox(); }

  InlineTextBox* CreateInlineTextBox(int start, unsigned short length) {
    return ToText()->CreateInlineTextBox(start, length);
  }

  void ExtractTextBox(InlineTextBox* inline_text_box) {
    ToText()->ExtractTextBox(inline_text_box);
  }

  void AttachTextBox(InlineTextBox* inline_text_box) {
    ToText()->AttachTextBox(inline_text_box);
  }

  void RemoveTextBox(InlineTextBox* inline_text_box) {
    ToText()->RemoveTextBox(inline_text_box);
  }

  bool IsWordBreak() const { return ToText()->IsWordBreak(); }

  bool IsAllCollapsibleWhitespace() const {
    return ToText()->IsAllCollapsibleWhitespace();
  }

  OnlyWhitespaceOrNbsp ContainsOnlyWhitespaceOrNbsp() const {
    return ToText()->ContainsOnlyWhitespaceOrNbsp();
  }

  UChar CharacterAt(unsigned offset) const {
    return ToText()->CharacterAt(offset);
  }

  UChar UncheckedCharacterAt(unsigned offset) const {
    return ToText()->UncheckedCharacterAt(offset);
  }

  UChar32 CodepointAt(unsigned offset) const {
    return ToText()->CodepointAt(offset);
  }

  bool Is8Bit() const { return ToText()->Is8Bit(); }

  const LChar* Characters8() const { return ToText()->Characters8(); }

  const UChar* Characters16() const { return ToText()->Characters16(); }

  bool HasEmptyText() const { return ToText()->HasEmptyText(); }

  unsigned TextLength() const { return ToText()->TextLength(); }

  unsigned ResolvedTextLength() const { return ToText()->ResolvedTextLength(); }

  const String& GetText() const { return ToText()->GetText(); }

  bool ContainsOnlyWhitespace(unsigned from, unsigned len) const {
    return ToText()->ContainsOnlyWhitespace(from, len);
  }

  float Width(unsigned from,
              unsigned len,
              const Font& font,
              LayoutUnit x_pos,
              TextDirection text_direction,
              HashSet<const SimpleFontData*>* fallback_fonts,
              FloatRect* glyph_bounds,
              float expansion = 0) const {
    return ToText()->Width(from, len, font, x_pos, text_direction,
                           fallback_fonts, glyph_bounds, expansion);
  }

  float Width(unsigned from,
              unsigned len,
              LayoutUnit x_pos,
              TextDirection text_direction,
              bool first_line,
              HashSet<const SimpleFontData*>* fallback_fonts = nullptr,
              FloatRect* glyph_bounds = nullptr,
              float expansion = 0) const {
    return ToText()->Width(from, len, x_pos, text_direction, first_line,
                           fallback_fonts, glyph_bounds, expansion);
  }

  float HyphenWidth(const Font& font, TextDirection text_direction) {
    return ToText()->HyphenWidth(font, text_direction);
  }

  unsigned TextStartOffset() const { return ToText()->TextStartOffset(); }

  float MinLogicalWidth() const { return ToText()->MinLogicalWidth(); }

  UChar PreviousCharacter() const { return ToText()->PreviousCharacter(); }

 private:
  LayoutText* ToText() { return ToLayoutText(GetLayoutObject()); }
  const LayoutText* ToText() const { return ToLayoutText(GetLayoutObject()); }
};

}  // namespace blink

#endif  // LineLayoutText_h
