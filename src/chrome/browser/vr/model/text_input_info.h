// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_MODEL_TEXT_INPUT_INFO_H_
#define CHROME_BROWSER_VR_MODEL_TEXT_INPUT_INFO_H_

#include <string>

#include "base/strings/string16.h"
#include "chrome/browser/vr/text_edit_action.h"
#include "chrome/browser/vr/vr_base_export.h"

namespace vr {

// Represents the state of an editable text field.
struct VR_BASE_EXPORT TextInputInfo {
 public:
  TextInputInfo();
  explicit TextInputInfo(base::string16 t);
  TextInputInfo(base::string16 t, int selection_start, int selection_end);
  TextInputInfo(base::string16 t,
                int selection_start,
                int selection_end,
                int composition_start,
                int compositon_end);
  TextInputInfo(const TextInputInfo& other);

  static const int kDefaultCompositionIndex = -1;

  bool operator==(const TextInputInfo& other) const;
  bool operator!=(const TextInputInfo& other) const;

  size_t SelectionSize() const;
  size_t CompositionSize() const;

  base::string16 CommittedTextBeforeCursor() const;
  base::string16 ComposingText() const;

  // The value of the input field.
  base::string16 text;

  // The cursor position of the current selection start, or the caret position
  // if nothing is selected.
  int selection_start;

  // The cursor position of the current selection end, or the caret position
  // if nothing is selected.
  int selection_end;

  // The start position of the current composition, or -1 if there is none.
  int composition_start;

  // The end position of the current composition, or -1 if there is none.
  int composition_end;

  std::string ToString() const;

 private:
  void ClampIndices();
};

// A superset of TextInputInfo, consisting of a current and previous text field
// state.  A keyboard can return this structure, allowing clients to derive
// deltas in keyboard state.
struct VR_BASE_EXPORT EditedText {
 public:
  EditedText();
  EditedText(const EditedText& other);
  explicit EditedText(const TextInputInfo& current);
  EditedText(const TextInputInfo& current, const TextInputInfo& previous);
  explicit EditedText(base::string16 t);

  bool operator==(const EditedText& other) const;
  bool operator!=(const EditedText& other) const { return !(*this == other); }

  void Update(const TextInputInfo& info);

  TextEdits GetDiff() const;

  std::string ToString() const;

  TextInputInfo current;
  TextInputInfo previous;
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_MODEL_TEXT_INPUT_INFO_H_
