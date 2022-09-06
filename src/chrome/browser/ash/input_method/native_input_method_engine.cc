// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/native_input_method_engine.h"

#include <utility>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/services/ime/public/mojom/input_method.mojom.h"
#include "base/feature_list.h"
#include "base/i18n/i18n_constants.h"
#include "base/i18n/icu_string_conversions.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_offset_string_conversions.h"
#include "base/strings/utf_string_conversion_utils.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/input_method/assistive_suggester_client_filter.h"
#include "chrome/browser/ash/input_method/assistive_suggester_prefs.h"
#include "chrome/browser/ash/input_method/assistive_suggester_switch.h"
#include "chrome/browser/ash/input_method/autocorrect_manager.h"
#include "chrome/browser/ash/input_method/diacritics_checker.h"
#include "chrome/browser/ash/input_method/get_browser_url.h"
#include "chrome/browser/ash/input_method/grammar_service_client.h"
#include "chrome/browser/ash/input_method/input_method_quick_settings_helpers.h"
#include "chrome/browser/ash/input_method/input_method_settings.h"
#include "chrome/browser/ash/input_method/native_input_method_engine_observer.h"
#include "chrome/browser/ash/input_method/suggestions_service_client.h"
#include "chrome/browser/ash/input_method/ui/input_method_menu_manager.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chrome/browser/ui/webui/settings/chromeos/constants/routes.mojom.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "ui/base/ime/ash/extension_ime_util.h"
#include "ui/base/ime/ash/ime_bridge.h"
#include "ui/base/ime/ash/ime_keyboard.h"
#include "ui/base/ime/ash/input_method_manager.h"
#include "ui/base/ime/ash/input_method_ukm.h"
#include "ui/events/keycodes/dom/keycode_converter.h"

namespace ash {

namespace input_method {

NativeInputMethodEngine::NativeInputMethodEngine()
    : NativeInputMethodEngine(/*use_ime_service=*/true) {}

NativeInputMethodEngine::NativeInputMethodEngine(bool use_ime_service)
    : use_ime_service_(use_ime_service) {}

// static
std::unique_ptr<NativeInputMethodEngine>
NativeInputMethodEngine::CreateForTesting(bool use_ime_service) {
  return base::WrapUnique<NativeInputMethodEngine>(
      new NativeInputMethodEngine(use_ime_service));
}

NativeInputMethodEngine::~NativeInputMethodEngine() = default;

NativeInputMethodEngine::NativeInputMethodEngine(
    std::unique_ptr<AssistiveSuggesterSwitch> suggester_switch)
    : suggester_switch_(std::move(suggester_switch)) {}

void NativeInputMethodEngine::Initialize(
    std::unique_ptr<InputMethodEngineObserver> observer,
    const char* extension_id,
    Profile* profile) {
  // TODO(crbug/1141231): refactor the mix of unique and raw ptr here.
  std::unique_ptr<AssistiveSuggester> assistive_suggester =
      std::make_unique<AssistiveSuggester>(
          this, profile,
          suggester_switch_ ? std::move(suggester_switch_)
                            : std::make_unique<AssistiveSuggesterClientFilter>(
                                  base::BindRepeating(&GetFocusedTabUrl)),
          nullptr);
  assistive_suggester_ = assistive_suggester.get();
  std::unique_ptr<AutocorrectManager> autocorrect_manager =
      std::make_unique<AutocorrectManager>(this);
  autocorrect_manager_ = autocorrect_manager.get();

  auto suggestions_service_client =
      features::IsAssistiveMultiWordEnabled()
          ? std::make_unique<SuggestionsServiceClient>()
          : nullptr;

  auto suggestions_collector =
      features::IsAssistiveMultiWordEnabled()
          ? std::make_unique<SuggestionsCollector>(
                assistive_suggester_, std::move(suggestions_service_client))
          : nullptr;

  chrome_keyboard_controller_client_observer_.Observe(
      ChromeKeyboardControllerClient::Get());

  // Wrap the given observer in our observer that will decide whether to call
  // Mojo directly or forward to the extension.
  auto native_observer = std::make_unique<NativeInputMethodEngineObserver>(
      profile->GetPrefs(), std::move(observer), std::move(assistive_suggester),
      std::move(autocorrect_manager), std::move(suggestions_collector),
      std::make_unique<GrammarManager>(
          profile, std::make_unique<GrammarServiceClient>(), this),
      use_ime_service_);
  InputMethodEngine::Initialize(std::move(native_observer), extension_id,
                                profile);
}

bool NativeInputMethodEngine::ShouldRouteToNativeMojoEngine(
    const std::string& engine_id) const {
  return use_ime_service_ && CanRouteToNativeMojoEngine(engine_id);
}

void NativeInputMethodEngine::CandidateClicked(uint32_t index) {
  // The parent implementation will try to convert `index` into a candidate ID.
  // The native Mojo engine doesn't use candidate IDs, so we just treat `index`
  // as the ID, without doing a mapping.
  if (ShouldRouteToNativeMojoEngine(GetActiveComponentId())) {
    GetNativeObserver()->OnCandidateClicked(GetActiveComponentId(), index,
                                            MOUSE_BUTTON_LEFT);
  } else {
    InputMethodEngine::CandidateClicked(index);
  }
}

void NativeInputMethodEngine::OnKeyboardEnabledChanged(bool enabled) {
  // Re-activate the engine whenever the virtual keyboard is enabled or disabled
  // so that the native or extension state is reset correctly.
  Enable(GetActiveComponentId());
}

void NativeInputMethodEngine::OnProfileWillBeDestroyed(Profile* profile) {
  InputMethodEngine::OnProfileWillBeDestroyed(profile);
  GetNativeObserver()->OnProfileWillBeDestroyed();
}

void NativeInputMethodEngine::FlushForTesting() {
  GetNativeObserver()->FlushForTesting();
}

bool NativeInputMethodEngine::IsConnectedForTesting() const {
  return GetNativeObserver()->IsConnectedForTesting();
}

void NativeInputMethodEngine::OnAutocorrect(
    const std::u16string& typed_word,
    const std::u16string& corrected_word,
    int start_index) {
  autocorrect_manager_->HandleAutocorrect(
      gfx::Range(start_index, start_index + corrected_word.length()),
      typed_word, corrected_word);
}

NativeInputMethodEngineObserver* NativeInputMethodEngine::GetNativeObserver()
    const {
  return static_cast<NativeInputMethodEngineObserver*>(observer_.get());
}

bool NativeInputMethodEngine::UpdateMenuItems(
    const std::vector<InputMethodManager::MenuItem>& items,
    std::string* error) {
  // Ignore calls to UpdateMenuItems when the native Mojo engine is active.
  // The menu items are stored in a singleton that is shared between the native
  // Mojo engine and the extension. This method is called when the extension
  // wants to update the menu items.
  // Ignore this if the native Mojo engine is active to prevent the extension
  // from overriding the menu items set by the native Mojo engine.
  if (ShouldRouteToNativeMojoEngine(GetActiveComponentId())) {
    return true;
  }

  return InputMethodEngine::UpdateMenuItems(items, error);
}

void NativeInputMethodEngine::OnInputMethodOptionsChanged() {
  if (ShouldRouteToNativeMojoEngine(GetActiveComponentId())) {
    Enable(GetActiveComponentId());
  } else {
    InputMethodEngine::OnInputMethodOptionsChanged();
  }
}

}  // namespace input_method
}  // namespace ash
