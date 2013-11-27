// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_CHROMEOS_MOCK_IME_CANDIDATE_WINDOW_HANDLER_H_
#define UI_BASE_IME_CHROMEOS_MOCK_IME_CANDIDATE_WINDOW_HANDLER_H_

#include "chromeos/ime/candidate_window.h"
#include "ui/base/ime/chromeos/ibus_bridge.h"
#include "ui/base/ui_export.h"

namespace chromeos {

class UI_EXPORT MockIMECandidateWindowHandler
    : public IBusPanelCandidateWindowHandlerInterface {
 public:
  struct UpdateLookupTableArg {
    input_method::CandidateWindow lookup_table;
    bool is_visible;
  };

  struct UpdateAuxiliaryTextArg {
    std::string text;
    bool is_visible;
  };

  MockIMECandidateWindowHandler();
  virtual ~MockIMECandidateWindowHandler();

  // IBusPanelCandidateWindowHandlerInterface override.
  virtual void UpdateLookupTable(
      const input_method::CandidateWindow& candidate_window,
      bool visible) OVERRIDE;
  virtual void UpdateAuxiliaryText(const std::string& text,
                                   bool visible) OVERRIDE;
  virtual void UpdatePreeditText(const std::string& text, uint32 cursor_pos,
                                 bool visible) OVERRIDE;
  virtual void SetCursorBounds(const gfx::Rect& cursor_bounds,
                               const gfx::Rect& composition_head) OVERRIDE;

  int set_cursor_bounds_call_count() const {
    return set_cursor_bounds_call_count_;
  }

  int update_lookup_table_call_count() const {
    return update_lookup_table_call_count_;
  }

  int update_auxiliary_text_call_count() const {
    return update_auxiliary_text_call_count_;
  }

  const UpdateLookupTableArg& last_update_lookup_table_arg() {
    return last_update_lookup_table_arg_;
  }

  const UpdateAuxiliaryTextArg& last_update_auxiliary_text_arg() {
    return last_update_auxiliary_text_arg_;
  }

  // Resets all call count.
  void Reset();

 private:
  int set_cursor_bounds_call_count_;
  int update_lookup_table_call_count_;
  int update_auxiliary_text_call_count_;
  UpdateLookupTableArg last_update_lookup_table_arg_;
  UpdateAuxiliaryTextArg last_update_auxiliary_text_arg_;
};

}  // namespace chromeos

#endif  // UI_BASE_IME_CHROMEOS_MOCK_IME_CANDIDATE_WINDOW_HANDLER_H_
