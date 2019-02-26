// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/mock_ime_input_context_handler.h"

#include "ui/base/ime/composition_text.h"
#include "ui/base/ime/input_method.h"

namespace ui {

MockIMEInputContextHandler::MockIMEInputContextHandler()
    : commit_text_call_count_(0),
      update_preedit_text_call_count_(0),
      delete_surrounding_text_call_count_(0),
      last_sent_key_event_(ui::ET_KEY_PRESSED, ui::VKEY_SPACE, 0) {}

MockIMEInputContextHandler::~MockIMEInputContextHandler() {}

void MockIMEInputContextHandler::CommitText(const std::string& text) {
  ++commit_text_call_count_;
  last_commit_text_ = text;
}

void MockIMEInputContextHandler::UpdateCompositionText(
    const CompositionText& text,
    uint32_t cursor_pos,
    bool visible) {
  ++update_preedit_text_call_count_;
  last_update_composition_arg_.composition_text = text;
  last_update_composition_arg_.cursor_pos = cursor_pos;
  last_update_composition_arg_.is_visible = visible;
}

void MockIMEInputContextHandler::DeleteSurroundingText(int32_t offset,
                                                       uint32_t length) {
  ++delete_surrounding_text_call_count_;
  last_delete_surrounding_text_arg_.offset = offset;
  last_delete_surrounding_text_arg_.length = length;
}

SurroundingTextInfo MockIMEInputContextHandler::GetSurroundingTextInfo() {
  return SurroundingTextInfo();
}

void MockIMEInputContextHandler::Reset() {
  commit_text_call_count_ = 0;
  update_preedit_text_call_count_ = 0;
  delete_surrounding_text_call_count_ = 0;
  last_commit_text_.clear();
  last_sent_key_event_ = ui::KeyEvent(ui::ET_KEY_PRESSED, ui::VKEY_SPACE, 0);
}

void MockIMEInputContextHandler::SendKeyEvent(KeyEvent* event) {
  last_sent_key_event_ = *event;
}

InputMethod* MockIMEInputContextHandler::GetInputMethod() {
  return nullptr;
}
}  // namespace ui
