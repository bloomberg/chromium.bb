// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_bidi_paragraph.h"

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
  ubidi_setPara(ubidi_, text.characters16(), text.length(),
                block_style->unicodeBidi() == Plaintext
                    ? UBIDI_DEFAULT_LTR
                    : (block_style->direction() == RTL ? UBIDI_RTL : UBIDI_LTR),
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

}  // namespace blink
