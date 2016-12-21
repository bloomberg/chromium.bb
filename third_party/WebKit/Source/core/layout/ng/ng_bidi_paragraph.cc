// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_bidi_paragraph.h"

#include "core/layout/ng/ng_inline_node.h"
#include "core/style/ComputedStyle.h"
#include "platform/text/ICUError.h"

namespace blink {

NGBidiParagraph::~NGBidiParagraph() {
  ubidi_close(ubidi_);
}

bool NGBidiParagraph::SetParagraph(const String& text,
                                   const ComputedStyle* block_style) {
  DCHECK(!ubidi_);
  ubidi_ = ubidi_open();
  ICUError error;
  ubidi_setPara(
      ubidi_, text.characters16(), text.length(),
      block_style->unicodeBidi() == Plaintext
          ? UBIDI_DEFAULT_LTR
          : (block_style->direction() == TextDirection::Rtl ? UBIDI_RTL
                                                            : UBIDI_LTR),
      nullptr, &error);
  if (U_FAILURE(error)) {
    NOTREACHED();
    ubidi_close(ubidi_);
    ubidi_ = nullptr;
    return false;
  }
  return true;
}

unsigned NGBidiParagraph::GetLogicalRun(unsigned start,
                                        UBiDiLevel* level) const {
  int32_t end;
  ubidi_getLogicalRun(ubidi_, start, &end, level);
  return end;
}

void NGBidiParagraph::IndiciesInVisualOrder(
    const NGLayoutInlineItemRange& items,
    Vector<int32_t, 32>* item_indicies_in_visual_order_out) {
  // ICU |ubidi_getVisualMap()| works for a run of characters. Since we can
  // handle the direction of each run, we use |ubidi_reorderVisual()| to reorder
  // runs instead of characters.
  // To do so, create a list of bidi levels by pretending a run is a character.
  Vector<UBiDiLevel, 32> levels;
  levels.reserveInitialCapacity(items.Size());
  for (const NGLayoutInlineItem& item : items)
    levels.append(item.BidiLevel());

  // Check the size before passing the raw pointers to ICU.
  CHECK_EQ(items.Size(), levels.size());
  CHECK_EQ(items.Size(), item_indicies_in_visual_order_out->size());
  ubidi_reorderVisual(levels.data(), items.Size(),
                      item_indicies_in_visual_order_out->data());
}

}  // namespace blink
