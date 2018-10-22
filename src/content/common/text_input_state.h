// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_TEXT_INPUT_STATE_H_
#define CONTENT_COMMON_TEXT_INPUT_STATE_H_

#include <string>

#include "content/common/content_export.h"
#include "ui/base/ime/text_input_mode.h"
#include "ui/base/ime/text_input_type.h"

namespace content {

// The text input state information for handling IME on the browser side. The
// text input state information such as type, mode, etc. are used by the input
// method to handle IME (the usage is specific to each platform).
struct CONTENT_EXPORT TextInputState {
  TextInputState();
  TextInputState(const TextInputState& other);

  // Type of the input field.
  ui::TextInputType type;

  // The mode of input field.
  ui::TextInputMode mode;

  // The flags of input field (autocorrect, autocomplete, etc.)
  int flags;

  // The value of input field.
  std::string value;

  // The cursor position of the current selection start, or the caret position
  // if nothing is selected.
  int selection_start;

  // The cursor position of the current selection end, or the caret position if
  // nothing is selected.
  int selection_end;

  // The start position of the current composition, or -1 if there is none.
  int composition_start;

  // The end position of the current composition, or -1 if there is none.
  int composition_end;

  // Whether or not inline composition can be performed for the current input.
  bool can_compose_inline;

  // Whether or not the IME should be shown as a result of this update. Even if
  // true, the IME will only be shown if the input is appropriate (e.g. not
  // TEXT_INPUT_TYPE_NONE).
  bool show_ime_if_needed;

  // Whether or not this is a reply to a request from IME.
  bool reply_to_request;
};

}  // namespace content

#endif  // CONTENT_COMMON_TEXT_INPUT_STATE_H_
