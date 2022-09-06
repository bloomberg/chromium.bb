// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/emoji/emoji_page_handler.h"

#include "ash/constants/ash_features.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/trace_event/trace_event.h"
#include "chrome/browser/ui/webui/chromeos/emoji/emoji_ui.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/base/ime/ash/ime_bridge.h"
#include "ui/base/ime/input_method_observer.h"

namespace chromeos {

// Keep in sync with entry in enums.xml.
enum class EmojiVariantType {
  // smaller entries only used by Chrome OS VK
  kEmojiPickerBase = 4,
  kEmojiPickerVariant = 5,
  kMaxValue = kEmojiPickerVariant,
};

void LogInsertEmoji(bool is_variant, int16_t search_length) {
  EmojiVariantType insert_value = is_variant
                                      ? EmojiVariantType::kEmojiPickerVariant
                                      : EmojiVariantType::kEmojiPickerBase;
  base::UmaHistogramEnumeration("InputMethod.SystemEmojiPicker.TriggerType",
                                insert_value);
  base::UmaHistogramCounts100("InputMethod.SystemEmojiPicker.SearchLength",
                              search_length);
}

void LogInsertEmojiDelay(base::TimeDelta delay) {
  base::UmaHistogramMediumTimes("InputMethod.SystemEmojiPicker.Delay", delay);
}

void CopyEmojiToClipboard(const std::string& emoji_to_copy) {
  if (base::FeatureList::IsEnabled(
          chromeos::features::kImeSystemEmojiPickerClipboard)) {
    auto clipboard = std::make_unique<ui::ScopedClipboardWriter>(
        ui::ClipboardBuffer::kCopyPaste, nullptr);
    clipboard->WriteText(base::UTF8ToUTF16(emoji_to_copy));
  }
}

// Used to insert an emoji after WebUI handler is destroyed, before
// self-destructing.
class EmojiObserver : public ui::InputMethodObserver {
 public:
  explicit EmojiObserver(const std::string& emoji_to_insert,
                         ui::InputMethod* ime)
      : emoji_to_insert_(emoji_to_insert), ime_(ime) {
    delete_timer_.Start(
        FROM_HERE, base::Seconds(1),
        base::BindOnce(&EmojiObserver::DestroySelf, base::Unretained(this)));
  }
  ~EmojiObserver() override {
    if (!inserted_) {
      CopyEmojiToClipboard(emoji_to_insert_);
    }
    ime_->RemoveObserver(this);
  }

  void OnTextInputStateChanged(const ui::TextInputClient* client) override {
    focus_change_count_++;
    // 2 focus changes - 1 for loss of focus in emoji picker, second for
    // focusing in the new text field.  You would expect this to fail if
    // the emoji picker window does not have focus in the text field, but
    // waiting for 2 focus changes is still correct behavior.
    if (focus_change_count_ == 2) {
      // Need to get the client via the IME as InsertText is non-const.
      // Can't use this->ime_ either as it may not be active, want to ensure
      // that we get the active IME.
      ui::InputMethod* input_method =
          ui::IMEBridge::Get()->GetInputContextHandler()->GetInputMethod();

      if (!input_method) {
        return;
      }
      ui::TextInputClient* input_client = input_method->GetTextInputClient();

      if (!input_client) {
        return;
      }
      if (input_client->GetTextInputType() ==
          ui::TextInputType::TEXT_INPUT_TYPE_NONE) {
        // In some clients (e.g. Sheets), there is an extra focus before the
        // "real" text input field.
        focus_change_count_--;
        return;
      }

      input_client->InsertText(
          base::UTF8ToUTF16(emoji_to_insert_),
          ui::TextInputClient::InsertTextCursorBehavior::kMoveCursorAfterText);
      inserted_ = true;
      DestroySelf();
      return;
    }
    if (focus_change_count_ > 2) {
      DestroySelf();
    }
  }
  void OnFocus() override {}
  void OnBlur() override {}
  void OnCaretBoundsChanged(const ui::TextInputClient* client) override {}
  void OnInputMethodDestroyed(const ui::InputMethod* client) override {}

 private:
  void DestroySelf() { delete this; }
  int focus_change_count_ = 0;
  std::string emoji_to_insert_;
  base::OneShotTimer delete_timer_;
  ui::InputMethod* ime_;
  bool inserted_ = false;
};

EmojiPageHandler::EmojiPageHandler(
    mojo::PendingReceiver<emoji_picker::mojom::PageHandler> receiver,
    content::WebUI* web_ui,
    EmojiUI* webui_controller,
    bool incognito_mode,
    bool no_text_field)
    : receiver_(this, std::move(receiver)),
      webui_controller_(webui_controller),
      incognito_mode_(incognito_mode),
      no_text_field_(no_text_field) {}

EmojiPageHandler::~EmojiPageHandler() {}

void EmojiPageHandler::ShowUI() {
  auto embedder = webui_controller_->embedder();
  // Embedder may not exist in some cases (e.g. user browses to
  // chrome://emoji-picker directly rather than using right click on
  // text field -> emoji).
  if (embedder) {
    embedder->ShowUI();
  }
  shown_time_ = base::TimeTicks::Now();
}

void EmojiPageHandler::IsIncognitoTextField(
    IsIncognitoTextFieldCallback callback) {
  std::move(callback).Run(incognito_mode_);
}

void EmojiPageHandler::GetFeatureList(GetFeatureListCallback callback) {
  std::vector<emoji_picker::mojom::Feature> enabled_features;
  if (base::FeatureList::IsEnabled(
          chromeos::features::kImeSystemEmojiPickerExtension)) {
    enabled_features.push_back(
        emoji_picker::mojom::Feature::EMOJI_PICKER_EXTENSION);
  }
  if (base::FeatureList::IsEnabled(
          chromeos::features::kImeSystemEmojiPickerSearchExtension)) {
    enabled_features.push_back(
        emoji_picker::mojom::Feature::EMOJI_PICKER_SEARCH_EXTENSION);
  }
  std::move(callback).Run(enabled_features);
}

void EmojiPageHandler::InsertEmoji(const std::string& emoji_to_insert,
                                   bool is_variant,
                                   int16_t search_length) {
  LogInsertEmoji(is_variant, search_length);
  LogInsertEmojiDelay(base::TimeTicks::Now() - shown_time_);
  // In theory, we are returning focus to the input field where the user
  // originally selected emoji. However, the input field may not exist anymore
  // e.g. JS has mutated the web page while emoji picker was open, so check
  // that a valid input client is available as part of inserting the emoji.
  ui::InputMethod* input_method =
      ui::IMEBridge::Get()->GetInputContextHandler()->GetInputMethod();
  if (!input_method) {
    DLOG(WARNING) << "no input_method found";
    CopyEmojiToClipboard(emoji_to_insert);
    return;
  }
  if (no_text_field_) {
    CopyEmojiToClipboard(emoji_to_insert);
    return;
  }

  // It seems like this might leak EmojiObserver, but the EmojiObserver
  // destroys itself on a timer (complex behavior needed since the observer
  // needs to outlive the page handler)
  input_method->AddObserver(new EmojiObserver(emoji_to_insert, input_method));

  // By hiding the emoji picker, we restore focus to the original text field
  // so we can insert the text.
  auto embedder = webui_controller_->embedder();
  if (embedder) {
    embedder->CloseUI();
  }
}

}  // namespace chromeos
