// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/inline/ng_bidi_paragraph.h"

#include "core/layout/ng/inline/ng_inline_node.h"
#include "core/layout/ng/ng_layout_result.h"
#include "core/layout/ng/ng_unpositioned_float.h"
#include "core/style/ComputedStyle.h"
#include "platform/text/ICUError.h"

namespace blink {

NGBidiParagraph::~NGBidiParagraph() {
  ubidi_close(ubidi_);
}

bool NGBidiParagraph::SetParagraph(const String& text,
                                   const ComputedStyle& block_style) {
  DCHECK(!ubidi_);
  ubidi_ = ubidi_open();

  bool use_heuristic_base_direction =
      block_style.GetUnicodeBidi() == UnicodeBidi::kPlaintext;
  UBiDiLevel para_level;
  if (use_heuristic_base_direction) {
    para_level = UBIDI_DEFAULT_LTR;
  } else {
    base_direction_ = block_style.Direction();
    para_level = IsLtr(base_direction_) ? UBIDI_LTR : UBIDI_RTL;
  }

  ICUError error;
  ubidi_setPara(ubidi_, text.Characters16(), text.length(), para_level, nullptr,
                &error);
  if (U_FAILURE(error)) {
    NOTREACHED();
    ubidi_close(ubidi_);
    ubidi_ = nullptr;
    return false;
  }

  if (use_heuristic_base_direction)
    base_direction_ = DirectionFromLevel(ubidi_getParaLevel(ubidi_));

  return true;
}

unsigned NGBidiParagraph::GetLogicalRun(unsigned start,
                                        UBiDiLevel* level) const {
  int32_t end;
  ubidi_getLogicalRun(ubidi_, start, &end, level);
  return end;
}

void NGBidiParagraph::IndicesInVisualOrder(
    const Vector<UBiDiLevel, 32>& levels,
    Vector<int32_t, 32>* indices_in_visual_order_out) {
  // Check the size before passing the raw pointers to ICU.
  CHECK_EQ(levels.size(), indices_in_visual_order_out->size());
  ubidi_reorderVisual(levels.data(), levels.size(),
                      indices_in_visual_order_out->data());
}

}  // namespace blink
