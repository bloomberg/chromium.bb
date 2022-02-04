// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/fake_suggestion_handler.h"

namespace ash {
namespace input_method {

FakeSuggestionHandler::FakeSuggestionHandler() = default;

FakeSuggestionHandler::~FakeSuggestionHandler() = default;

bool FakeSuggestionHandler::DismissSuggestion(int context_id,
                                              std::string* error) {
  showing_suggestion_ = false;
  dismissed_suggestion_ = true;
  suggestion_text_ = u"";
  confirmed_length_ = 0;
  return true;
}

bool FakeSuggestionHandler::SetSuggestion(
    int context_id,
    const ui::ime::SuggestionDetails& details,
    std::string* error) {
  showing_suggestion_ = true;
  context_id_ = context_id;
  suggestion_text_ = details.text;
  confirmed_length_ = details.confirmed_length;
  last_suggestion_details_ = details;
  return true;
}

bool FakeSuggestionHandler::AcceptSuggestion(int context_id,
                                             std::string* error) {
  showing_suggestion_ = false;
  accepted_suggestion_ = true;
  suggestion_text_ = u"";
  confirmed_length_ = 0;
  return true;
}

void FakeSuggestionHandler::OnSuggestionsChanged(
    const std::vector<std::string>& suggestions) {}

bool FakeSuggestionHandler::SetButtonHighlighted(
    int context_id,
    const ui::ime::AssistiveWindowButton& button,
    bool highlighted,
    std::string* error) {
  highlighted_suggestion_ = highlighted;
  return false;
}

void FakeSuggestionHandler::ClickButton(
    const ui::ime::AssistiveWindowButton& button) {}

bool FakeSuggestionHandler::AcceptSuggestionCandidate(
    int context_id,
    const std::u16string& candidate,
    std::string* error) {
  return false;
}

bool FakeSuggestionHandler::SetAssistiveWindowProperties(
    int context_id,
    const AssistiveWindowProperties& assistive_window,
    std::string* error) {
  return false;
}

void FakeSuggestionHandler::Announce(const std::u16string& message) {
  announcements_.push_back(message);
}

}  // namespace input_method
}  // namespace ash
