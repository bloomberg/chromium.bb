// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_COMPOSITION_TEXT_H_
#define UI_BASE_IME_COMPOSITION_TEXT_H_
#pragma once

#include "base/string16.h"
#include "ui/base/ime/composition_underline.h"
#include "ui/base/range/range.h"
#include "ui/base/ui_export.h"

namespace ui {

// A struct represents the status of an ongoing composition text.
struct UI_EXPORT CompositionText {
  CompositionText();
  ~CompositionText();

  void Clear();

  // Content of the composition text.
  string16 text;

  // Underline information of the composition text.
  // They must be sorted in ascending order by their start_offset and cannot be
  // overlapped with each other.
  CompositionUnderlines underlines;

  // Selection range in the composition text. It represents the caret position
  // if the range length is zero. Usually it's used for representing the target
  // clause (on Windows). Gtk doesn't have such concept, so background color is
  // usually used instead.
  Range selection;
};

}  // namespace ui

#endif  // UI_BASE_IME_COMPOSITION_TEXT_H_
