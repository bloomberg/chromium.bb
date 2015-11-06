// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_CHROMEOS_MOCK_IME_ENGINE_HANDLER_H_
#define UI_BASE_IME_CHROMEOS_MOCK_IME_ENGINE_HANDLER_H_

#include "ui/base/ime/ime_engine_handler_interface.h"
#include "ui/base/ime/ui_base_ime_export.h"
#include "ui/events/event.h"

namespace chromeos {

class UI_BASE_IME_EXPORT MockIMEEngineHandler
    : public ui::IMEEngineHandlerInterface {
 public:
  MockIMEEngineHandler();
  ~MockIMEEngineHandler() override;

  void FocusIn(const InputContext& input_context) override;
  void FocusOut() override;
  void Enable(const std::string& component_id) override;
  void Disable() override;
  void PropertyActivate(const std::string& property_name) override;
  void Reset() override;
  bool IsInterestedInKeyEvent() const override;
  void ProcessKeyEvent(const ui::KeyEvent& key_event,
                       KeyEventDoneCallback& callback) override;
  void CandidateClicked(uint32 index) override;
  void SetSurroundingText(const std::string& text,
                          uint32 cursor_pos,
                          uint32 anchor_pos,
                          uint32 offset_pos) override;
  void SetCompositionBounds(const std::vector<gfx::Rect>& bounds) override;

  int focus_in_call_count() const { return focus_in_call_count_; }
  int focus_out_call_count() const { return focus_out_call_count_; }
  int reset_call_count() const { return reset_call_count_; }
  int set_surrounding_text_call_count() const {
    return set_surrounding_text_call_count_;
  }
  int process_key_event_call_count() const {
    return process_key_event_call_count_;
  }

  const InputContext& last_text_input_context() const {
    return last_text_input_context_;
  }

  std::string last_activated_property() const {
    return last_activated_property_;
  }

  std::string last_set_surrounding_text() const {
    return last_set_surrounding_text_;
  }

  uint32 last_set_surrounding_cursor_pos() const {
    return last_set_surrounding_cursor_pos_;
  }

  uint32 last_set_surrounding_anchor_pos() const {
    return last_set_surrounding_anchor_pos_;
  }

  const ui::KeyEvent* last_processed_key_event() const {
    return last_processed_key_event_.get();
  }

  const KeyEventDoneCallback& last_passed_callback() const {
    return last_passed_callback_;
  }

  bool SetComposition(int context_id,
                      const char* text,
                      int selection_start,
                      int selection_end,
                      int cursor,
                      const std::vector<SegmentInfo>& segments,
                      std::string* error) override;

  bool ClearComposition(int context_id, std::string* error) override;

  bool CommitText(int context_id,
                  const char* text,
                  std::string* error) override;

  bool SendKeyEvents(int context_id,
                     const std::vector<KeyboardEvent>& events) override;

  bool IsActive() const override;

  const std::string& GetActiveComponentId() const override;

  bool DeleteSurroundingText(int context_id,
                             int offset,
                             size_t number_of_chars,
                             std::string* error) override;

  const CandidateWindowProperty& GetCandidateWindowProperty() const override;

  void SetCandidateWindowProperty(
      const CandidateWindowProperty& property) override {}

  bool SetCandidateWindowVisible(bool visible, std::string* error) override;

  bool SetCandidates(int context_id,
                     const std::vector<Candidate>& candidates,
                     std::string* error) override;

  bool SetCursorPosition(int context_id,
                         int candidate_id,
                         std::string* error) override;

  bool SetMenuItems(const std::vector<MenuItem>& items) override;

  bool UpdateMenuItems(const std::vector<MenuItem>& items) override;

  void HideInputView() override {}

 private:
  int focus_in_call_count_;
  int focus_out_call_count_;
  int set_surrounding_text_call_count_;
  int process_key_event_call_count_;
  int reset_call_count_;
  InputContext last_text_input_context_;
  std::string last_activated_property_;
  std::string last_set_surrounding_text_;
  uint32 last_set_surrounding_cursor_pos_;
  uint32 last_set_surrounding_anchor_pos_;
  scoped_ptr<ui::KeyEvent> last_processed_key_event_;
  KeyEventDoneCallback last_passed_callback_;
  std::string active_component_id_;
  CandidateWindowProperty candidate_window_property_;
};

}  // namespace chromeos

#endif  // UI_BASE_IME_CHROMEOS_MOCK_IME_ENGINE_HANDLER_H_
