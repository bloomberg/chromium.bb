// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_CHROMEOS_MOCK_IME_INPUT_CONTEXT_HANDLER_H_
#define UI_BASE_IME_CHROMEOS_MOCK_IME_INPUT_CONTEXT_HANDLER_H_

#include "chromeos/ime/ibus_text.h"
#include "ui/base/ime/chromeos/ibus_bridge.h"
#include "ui/base/ui_export.h"

namespace chromeos {

class UI_EXPORT MockIMEInputContextHandler
    : public IBusInputContextHandlerInterface {
 public:
  struct UpdatePreeditTextArg {
    IBusText ibus_text;
    uint32 cursor_pos;
    bool is_visible;
  };

  struct DeleteSurroundingTextArg {
    int32 offset;
    uint32 length;
  };

  MockIMEInputContextHandler();
  virtual ~MockIMEInputContextHandler();

  virtual void CommitText(const std::string& text) OVERRIDE;
  virtual void UpdatePreeditText(const IBusText& text,
                                 uint32 cursor_pos,
                                 bool visible) OVERRIDE;
  virtual void DeleteSurroundingText(int32 offset, uint32 length) OVERRIDE;

  int commit_text_call_count() const { return commit_text_call_count_; }

  int update_preedit_text_call_count() const {
    return update_preedit_text_call_count_;
  }

  int delete_surrounding_text_call_count() const {
    return delete_surrounding_text_call_count_;
  }

  const std::string& last_commit_text() const {
    return last_commit_text_;
  };

  const UpdatePreeditTextArg& last_update_preedit_arg() const {
    return last_update_preedit_arg_;
  }

  const DeleteSurroundingTextArg& last_delete_surrounding_text_arg() const {
    return last_delete_surrounding_text_arg_;
  }

  // Resets all call count.
  void Reset();

 private:
  int commit_text_call_count_;
  int update_preedit_text_call_count_;
  int delete_surrounding_text_call_count_;
  std::string last_commit_text_;
  UpdatePreeditTextArg last_update_preedit_arg_;
  DeleteSurroundingTextArg last_delete_surrounding_text_arg_;
};

}  // namespace chromeos

#endif  // UI_BASE_IME_CHROMEOS_MOCK_IME_INPUT_CONTEXT_HANDLER_H_
