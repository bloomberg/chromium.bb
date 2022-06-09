// Copyright (c) 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/grammar_manager.h"

#include "ash/constants/ash_features.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/timer/timer.h"
#include "chrome/browser/ash/input_method/assistive_window_properties.h"
#include "chrome/browser/ash/input_method/ui/suggestion_details.h"
#include "ui/base/ime/ash/ime_bridge.h"
#include "ui/base/ime/ash/ime_input_context_handler_interface.h"
#include "ui/base/ime/text_input_flags.h"
#include "ui/events/keycodes/dom/dom_code.h"

namespace ash {
namespace input_method {
namespace {

constexpr base::TimeDelta kCheckDelay = base::Seconds(2);
const uint64_t HashMultiplier = 1LL << 32;

const char16_t kShowGrammarSuggestionMessage[] =
    u"Grammar correction suggested. Press tab to access; escape to dismiss.";
const char16_t kDismissGrammarSuggestionMessage[] = u"Suggestion dismissed.";
const char16_t kAcceptGrammarSuggestionMessage[] = u"Suggestion accepted.";
const char16_t kIgnoreGrammarSuggestionMessage[] = u"Suggestion ignored.";
const char kSuggestionButtonMessageTemplate[] =
    "Suggestion %s. Button. Press enter to accept; escape to dismiss.";
const char16_t kIgnoreButtonMessage[] =
    u"Ignore suggestion. Button. Press enter to ignore the suggestion; escape "
    u"to dismiss.";

void RecordGrammarAction(GrammarActions action,
                         bool is_capitalization_correction) {
  base::UmaHistogramEnumeration("InputMethod.Assistive.Grammar.Actions",
                                action);
  if (is_capitalization_correction) {
    base::UmaHistogramEnumeration(
        "InputMethod.Assistive.Grammar.CapitalizationCorrection", action);
  }
}

bool IsCapitalizationCorrection(const ui::GrammarFragment& fragment) {
  return base::ToLowerASCII(fragment.suggestion) ==
         base::ToLowerASCII(fragment.original_text);
}

bool IsValidSentence(const std::u16string& text, const Sentence& sentence) {
  uint32_t start = sentence.original_range.start();
  uint32_t end = sentence.original_range.end();
  if (start >= end || start >= text.size() || end > text.size())
    return false;

  return FindCurrentSentence(text, start) == sentence;
}

uint64_t RangeHash(const gfx::Range& range) {
  return range.start() * HashMultiplier + range.end();
}

}  // namespace

GrammarManager::GrammarManager(
    Profile* profile,
    std::unique_ptr<GrammarServiceClient> grammar_client,
    SuggestionHandlerInterface* suggestion_handler)
    : profile_(profile),
      grammar_client_(std::move(grammar_client)),
      suggestion_handler_(suggestion_handler),
      current_fragment_(gfx::Range(), std::string()),
      suggestion_button_(ui::ime::AssistiveWindowButton{
          .id = ui::ime::ButtonId::kSuggestion,
          .window_type = ui::ime::AssistiveWindowType::kGrammarSuggestion,
          .announce_string = u"",
      }),
      ignore_button_(ui::ime::AssistiveWindowButton{
          .id = ui::ime::ButtonId::kIgnoreSuggestion,
          .window_type = ui::ime::AssistiveWindowType::kGrammarSuggestion,
          .announce_string = kIgnoreButtonMessage,
      }) {}

GrammarManager::~GrammarManager() = default;

bool GrammarManager::IsOnDeviceGrammarEnabled() {
  return base::FeatureList::IsEnabled(features::kOnDeviceGrammarCheck);
}

void GrammarManager::OnFocus(int context_id, int text_input_flags) {
  if (context_id != context_id_) {
    current_text_ = u"";
    last_sentence_ = Sentence();
    new_to_context_ = true;
    delay_timer_.Stop();
    ignored_marker_hashes_.clear();
    recorded_marker_hashes_.clear();
  }
  context_id_ = context_id;
  text_input_flags_ = text_input_flags;
}

bool GrammarManager::OnKeyEvent(const ui::KeyEvent& event) {
  if (!suggestion_shown_ || event.type() != ui::ET_KEY_PRESSED)
    return false;

  if (event.code() == ui::DomCode::ESCAPE) {
    DismissSuggestion();
    suggestion_handler_->Announce(kDismissGrammarSuggestionMessage);
    return true;
  }

  switch (highlighted_button_) {
    case ui::ime::ButtonId::kNone:
      if (event.code() == ui::DomCode::TAB ||
          event.code() == ui::DomCode::ARROW_UP) {
        highlighted_button_ = ui::ime::ButtonId::kSuggestion;
        SetButtonHighlighted(suggestion_button_, true);
        return true;
      }
      break;
    case ui::ime::ButtonId::kSuggestion:
      switch (event.code()) {
        case ui::DomCode::TAB:
          highlighted_button_ = ui::ime::ButtonId::kIgnoreSuggestion;
          SetButtonHighlighted(ignore_button_, true);
          return true;
        case ui::DomCode::ARROW_DOWN:
          highlighted_button_ = ui::ime::ButtonId::kNone;
          SetButtonHighlighted(suggestion_button_, false);
          return true;
        case ui::DomCode::ENTER:
          // SetComposingRange and CommitText in AcceptSuggestion will not be
          // executed immediately if we are in middle of handling a key event,
          // instead they will be delayed and CommitText will always be executed
          // first. So we need to call AcceptSuggestion in a post task.
          // TODO(crbug.com/1230961): remove PostTask after we remove the delay
          // logics.
          base::SequencedTaskRunnerHandle::Get()->PostTask(
              FROM_HERE, base::BindOnce(&GrammarManager::AcceptSuggestion,
                                        base::Unretained(this)));
          return true;
        default:
          break;
      }
      break;
    case ui::ime::ButtonId::kIgnoreSuggestion:
      if (event.code() == ui::DomCode::ENTER) {
        IgnoreSuggestion();
        return true;
      }
      break;
    default:
      break;
  }
  return false;
}

void GrammarManager::OnSurroundingTextChanged(const std::u16string& text,
                                              int cursor_pos,
                                              int anchor_pos) {
  if (text_input_flags_ & ui::TEXT_INPUT_FLAG_SPELLCHECK_OFF)
    return;

  if (suggestion_shown_)
    DismissSuggestion();

  bool text_updated = text != current_text_;
  current_text_ = text;
  current_sentence_ = FindCurrentSentence(text, cursor_pos);

  if (new_to_context_) {
    new_to_context_ = false;
  } else if (text_updated) {
    ui::IMEInputContextHandlerInterface* input_context =
        ui::IMEBridge::Get()->GetInputContextHandler();
    if (!input_context)
      return;

    // Grammar check is cpu consuming, so we only send request to ml service
    // when the user has finished a sentence or stopped typing for some time.
    Sentence last_sentence = FindLastSentence(text, cursor_pos);
    if (last_sentence_ != last_sentence) {
      last_sentence_ = last_sentence;
      input_context->ClearGrammarFragments(last_sentence.original_range);
      Check(last_sentence);
    }

    input_context->ClearGrammarFragments(current_sentence_.original_range);

    delay_timer_.Start(
        FROM_HERE, kCheckDelay,
        base::BindOnce(&GrammarManager::Check, base::Unretained(this),
                       current_sentence_));
    return;
  }

  // Do not show the suggestion when the user is selecting a range of text, so
  // that we will not show conflict with the system copy/paste popup.
  if (cursor_pos != anchor_pos)
    return;

  ui::IMEInputContextHandlerInterface* input_context =
      ui::IMEBridge::Get()->GetInputContextHandler();
  if (!input_context)
    return;

  // Do not show suggestion when the cursor is within an auto correct range.
  const gfx::Range range = input_context->GetAutocorrectRange();
  if (!range.is_empty() && cursor_pos >= range.start() &&
      cursor_pos <= range.end()) {
    return;
  }

  absl::optional<ui::GrammarFragment> grammar_fragment_opt =
      input_context->GetGrammarFragment(gfx::Range(cursor_pos));

  if (grammar_fragment_opt) {
    if (current_fragment_ != grammar_fragment_opt.value()) {
      current_fragment_ = grammar_fragment_opt.value();
      RecordGrammarAction(GrammarActions::kWindowShown,
                          IsCapitalizationCorrection(current_fragment_));
    }
    std::string error;
    AssistiveWindowProperties properties;
    properties.type = ui::ime::AssistiveWindowType::kGrammarSuggestion;
    properties.candidates = {base::UTF8ToUTF16(current_fragment_.suggestion)};
    properties.visible = true;
    properties.announce_string = kShowGrammarSuggestionMessage;
    suggestion_button_.announce_string = base::UTF8ToUTF16(
        base::StringPrintf(kSuggestionButtonMessageTemplate,
                           current_fragment_.suggestion.c_str()));
    suggestion_handler_->SetAssistiveWindowProperties(context_id_, properties,
                                                      &error);
    if (!error.empty()) {
      LOG(ERROR) << "Fail to show suggestion. " << error;
    }
    highlighted_button_ = ui::ime::ButtonId::kNone;
    suggestion_shown_ = true;
  }
}

void GrammarManager::Check(const Sentence& sentence) {
  if (!IsValidSentence(current_text_, sentence))
    return;

  grammar_client_->RequestTextCheck(
      profile_, sentence.text,
      base::BindOnce(&GrammarManager::OnGrammarCheckDone,
                     base::Unretained(this), sentence));
}

void GrammarManager::OnGrammarCheckDone(
    const Sentence& sentence,
    bool success,
    const std::vector<ui::GrammarFragment>& results) {
  if (!success || !IsValidSentence(current_text_, sentence) || results.empty())
    return;

  std::vector<ui::GrammarFragment> corrected_results;
  auto it = ignored_marker_hashes_.find(sentence.text);
  for (const ui::GrammarFragment& fragment : results) {
    if (it == ignored_marker_hashes_.end() ||
        it->second.find(RangeHash(fragment.range)) == it->second.end()) {
      corrected_results.emplace_back(
          gfx::Range(fragment.range.start() + sentence.original_range.start(),
                     fragment.range.end() + sentence.original_range.start()),
          fragment.suggestion,
          base::UTF16ToUTF8(current_text_.substr(
              fragment.range.start() + sentence.original_range.start(),
              fragment.range.length())));
    }
  }

  ui::IMEInputContextHandlerInterface* input_context =
      ui::IMEBridge::Get()->GetInputContextHandler();
  if (!input_context)
    return;

  if (input_context->AddGrammarFragments(corrected_results)) {
    for (const ui::GrammarFragment& fragment : corrected_results) {
      uint64_t hashValue = RangeHash(fragment.range);
      // The de-dup could be incorrect in some cases but it is good enough for
      // collecting metrics.
      if (recorded_marker_hashes_.find(hashValue) ==
          recorded_marker_hashes_.end()) {
        recorded_marker_hashes_.insert(hashValue);
        RecordGrammarAction(GrammarActions::kUnderlined,
                            IsCapitalizationCorrection(fragment));
      }
    }
  }
}

void GrammarManager::DismissSuggestion() {
  std::string error;
  suggestion_handler_->DismissSuggestion(context_id_, &error);
  if (!error.empty()) {
    LOG(ERROR) << "Failed to dismiss suggestion. " << error;
    return;
  }
  suggestion_shown_ = false;
}

void GrammarManager::AcceptSuggestion() {
  if (!suggestion_shown_)
    return;

  DismissSuggestion();

  ui::IMEInputContextHandlerInterface* input_context =
      ui::IMEBridge::Get()->GetInputContextHandler();
  if (!input_context) {
    LOG(ERROR) << "Failed to commit grammar suggestion.";
  }

  if (input_context->HasCompositionText()) {
    input_context->SetComposingRange(current_fragment_.range.start(),
                                     current_fragment_.range.end(), {});
    input_context->CommitText(
        base::UTF8ToUTF16(current_fragment_.suggestion),
        ui::TextInputClient::InsertTextCursorBehavior::kMoveCursorAfterText);
  } else {
    // NOTE: GetSurroundingTextInfo() could return a stale cache that no
    // longer reflects reality, due to async-ness between IMF and
    // TextInputClient.
    // TODO(crbug/1194424): Work around the issue or fix
    // GetSurroundingTextInfo().
    const ui::SurroundingTextInfo surrounding_text =
        input_context->GetSurroundingTextInfo();

    // Delete the incorrect grammar fragment.
    input_context->DeleteSurroundingText(
        -static_cast<int>(surrounding_text.selection_range.start() -
                          current_fragment_.range.start()),
        current_fragment_.range.length() -
            surrounding_text.selection_range.length());
    input_context->SetSelectionRange(current_fragment_.range.start(),
                                     current_fragment_.range.start());
    // Insert the suggestion and put cursor after it.
    input_context->CommitText(
        base::UTF8ToUTF16(current_fragment_.suggestion),
        ui::TextInputClient::InsertTextCursorBehavior::kMoveCursorAfterText);
  }

  suggestion_handler_->Announce(kAcceptGrammarSuggestionMessage);
  RecordGrammarAction(GrammarActions::kAccepted,
                      IsCapitalizationCorrection(current_fragment_));
}

void GrammarManager::IgnoreSuggestion() {
  if (!suggestion_shown_)
    return;

  DismissSuggestion();

  ui::IMEInputContextHandlerInterface* input_context =
      ui::IMEBridge::Get()->GetInputContextHandler();
  if (!input_context)
    return;

  input_context->ClearGrammarFragments(current_fragment_.range);
  if (ignored_marker_hashes_.find(current_sentence_.text) ==
      ignored_marker_hashes_.end()) {
    ignored_marker_hashes_[current_sentence_.text] =
        std::unordered_set<uint64_t>();
  }
  ignored_marker_hashes_[current_sentence_.text].insert(
      RangeHash(gfx::Range(current_fragment_.range.start() -
                               current_sentence_.original_range.start(),
                           current_fragment_.range.end() -
                               current_sentence_.original_range.start())));

  suggestion_handler_->Announce(kIgnoreGrammarSuggestionMessage);
  RecordGrammarAction(GrammarActions::kIgnored,
                      IsCapitalizationCorrection(current_fragment_));
}

void GrammarManager::SetButtonHighlighted(
    const ui::ime::AssistiveWindowButton& button,
    bool highlighted) {
  std::string error;
  suggestion_handler_->SetButtonHighlighted(context_id_, button, highlighted,
                                            &error);
  if (!error.empty()) {
    LOG(ERROR) << "Failed to set button highlighted. " << error;
  }
}

}  // namespace input_method
}  // namespace ash
