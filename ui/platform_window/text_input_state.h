// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_PLATFORM_WINDOW_TEXT_INPUT_STATE_H_
#define UI_PLATFORM_WINDOW_TEXT_INPUT_STATE_H_

#include <string>

namespace ui {

// Text input type which is based on blink::WebTextInputType.
enum TextInputType {
  TEXT_INPUT_TYPE_NONE,
  TEXT_INPUT_TYPE_TEXT,
  TEXT_INPUT_TYPE_PASSWORD,
  TEXT_INPUT_TYPE_SEARCH,
  TEXT_INPUT_TYPE_EMAIL,
  TEXT_INPUT_TYPE_NUMBER,
  TEXT_INPUT_TYPE_TELEPHONE,
  TEXT_INPUT_TYPE_URL,
  TEXT_INPUT_TYPE_DATE,
  TEXT_INPUT_TYPE_DATE_TIME,
  TEXT_INPUT_TYPE_DATE_TIME_LOCAL,
  TEXT_INPUT_TYPE_MONTH,
  TEXT_INPUT_TYPE_TIME,
  TEXT_INPUT_TYPE_WEEK,
  TEXT_INPUT_TYPE_TEXT_AREA,
  TEXT_INPUT_TYPE_LAST = TEXT_INPUT_TYPE_TEXT_AREA,
};

// Text input flag which is based on blink::WebTextInputFlags.
enum TextInputFlag {
  TEXT_INPUT_FLAG_NONE,
  TEXT_INPUT_FLAG_AUTO_COMPLETE_ON = 1 << 0,
  TEXT_INPUT_FLAG_AUTO_COMPLETE_OFF = 1 << 1,
  TEXT_INPUT_FLAG_AUTO_CORRECT_ON = 1 << 2,
  TEXT_INPUT_FLAG_AUTO_CORRECT_OFF = 1 << 3,
  TEXT_INPUT_FLAG_SPELL_CHECK_ON = 1 << 4,
  TEXT_INPUT_FLAG_SPELL_CHECK_OFF = 1 << 5,
  TEXT_INPUT_FLAG_AUTO_CAPITALIZE_NONE = 1 << 6,
  TEXT_INPUT_FLAG_AUTO_CAPITALIZE_CHARACTERS = 1 << 7,
  TEXT_INPUT_FLAG_AUTO_CAPITALIZE_WORDS = 1 << 8,
  TEXT_INPUT_FLAG_AUTO_CAPITALIZE_SENTENCES = 1 << 9,
  TEXT_INPUT_FLAG_ALL = (TEXT_INPUT_FLAG_AUTO_CAPITALIZE_SENTENCES << 1) - 1,
};

// Text input info which is based on blink::WebTextInputInfo.
struct TextInputState {
  TextInputState()
      : type(TEXT_INPUT_TYPE_NONE),
        flags(TEXT_INPUT_FLAG_NONE),
        selection_start(0),
        selection_end(0),
        composition_start(0),
        composition_end(0),
        can_compose_inline(false),
        show_ime_if_needed(false) {}

  // The type of input field.
  TextInputType type;

  // The flags of the input field (autocorrect, autocomplete, etc.).
  int flags;

  // The value of the input field.
  std::string text;

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

  // Whether or not inline composition can be performed for the current input.
  bool can_compose_inline;

  // Whether or not the IME should be shown as a result of this update. Even if
  // true, the IME will only be shown if the type is appropriate (e.g. not
  // TEXT_INPUT_TYPE_NONE).
  bool show_ime_if_needed;
};

}  // namespace ui

#endif  // UI_PLATFORM_WINDOW_TEXT_INPUT_STATE_H_
