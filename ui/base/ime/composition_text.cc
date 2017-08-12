// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/composition_text.h"

namespace ui {

CompositionText::CompositionText() {
}

CompositionText::CompositionText(const CompositionText& other) = default;

CompositionText::~CompositionText() {
}

void CompositionText::Clear() {
  text.clear();
  ime_text_spans.clear();
  selection = gfx::Range();
}

void CompositionText::CopyFrom(const CompositionText& obj) {
  Clear();
  text = obj.text;
  for (size_t i = 0; i < obj.ime_text_spans.size(); i++) {
    ime_text_spans.push_back(obj.ime_text_spans[i]);
  }
  selection = obj.selection;
}

}  // namespace ui
