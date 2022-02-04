// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/virtual_keyboard_private/chrome_virtual_keyboard_delegate.h"

#include <memory>
#include <string>
#include <utility>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/clipboard_history_controller.h"
#include "ash/public/cpp/clipboard_image_model_factory.h"
#include "ash/public/cpp/keyboard/keyboard_switches.h"
#include "ash/public/cpp/keyboard/keyboard_types.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "chrome/browser/ash/login/lock/screen_locker.h"
#include "chrome/browser/ash/login/ui/user_adding_screen.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/keyboard/chrome_keyboard_controller_client.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chrome/browser/ui/webui/settings/chromeos/constants/routes.mojom.h"
#include "chrome/common/url_constants.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/audio_service.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/event_router.h"
#include "extensions/common/api/virtual_keyboard.h"
#include "extensions/common/api/virtual_keyboard_private.h"
#include "media/audio/audio_system.h"
#include "ui/aura/event_injector.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/ime/ash/ime_bridge.h"
#include "ui/base/ime/constants.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/base/ui_base_features.h"
#include "ui/events/event_utils.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/dom_key.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/events/keycodes/keyboard_code_conversion.h"

namespace keyboard_api = extensions::api::virtual_keyboard_private;

namespace {

// The hotrod keyboard must be enabled for each session and will remain enabled
// until / unless it is explicitly disabled.
bool g_hotrod_keyboard_enabled = false;

std::string GenerateFeatureFlag(const std::string& feature, bool enabled) {
  return feature + (enabled ? "-enabled" : "-disabled");
}

keyboard::ContainerType ConvertKeyboardModeToContainerType(int mode) {
  switch (mode) {
    case keyboard_api::KEYBOARD_MODE_FULL_WIDTH:
      return keyboard::ContainerType::kFullWidth;
    case keyboard_api::KEYBOARD_MODE_FLOATING:
      return keyboard::ContainerType::kFloating;
  }

  NOTREACHED();
  return keyboard::ContainerType::kFullWidth;
}

// Returns the ui::TextInputClient of the active InputMethod or nullptr.
ui::TextInputClient* GetFocusedTextInputClient() {
  ui::InputMethod* input_method =
      ui::IMEBridge::Get()->GetInputContextHandler()->GetInputMethod();
  if (!input_method)
    return nullptr;

  return input_method->GetTextInputClient();
}

const char kKeyDown[] = "keydown";
const char kKeyUp[] = "keyup";

void SendProcessKeyEvent(ui::EventType type, aura::WindowTreeHost* host) {
  ui::KeyEvent event(type, ui::VKEY_PROCESSKEY, ui::DomCode::NONE,
                     ui::EF_IS_SYNTHESIZED, ui::DomKey::PROCESS,
                     ui::EventTimeForNow());
  ui::EventDispatchDetails details = aura::EventInjector().Inject(host, &event);
  CHECK(!details.dispatcher_destroyed);
}

// Sends a fabricated key event, where |type| is the event type (which must be
// "keydown" or "keyup"), |key_value| is the unicode value of the character,
// |key_code| is the legacy key code value, |key_name| is the name of the key as
// defined in the DOM3 key event specification, and |modifier| indicates if any
// modifier keys are being virtually pressed. The event is dispatched to the
// active TextInputClient associated with |host|.
bool SendKeyEventImpl(const std::string& type,
                      int key_value,
                      int key_code,
                      const std::string& key_name,
                      int modifiers,
                      aura::WindowTreeHost* host) {
  ui::EventType event_type;
  if (type == kKeyDown)
    event_type = ui::ET_KEY_PRESSED;
  else if (type == kKeyUp)
    event_type = ui::ET_KEY_RELEASED;
  else
    return false;

  ui::KeyboardCode code = static_cast<ui::KeyboardCode>(key_code);

  if (code == ui::VKEY_UNKNOWN) {
    // Handling of special printable characters (e.g. accented characters) for
    // which there is no key code.
    if (event_type == ui::ET_KEY_RELEASED) {
      // This can be null if no text input field is focused.
      ui::TextInputClient* tic = GetFocusedTextInputClient();

      SendProcessKeyEvent(ui::ET_KEY_PRESSED, host);

      ui::KeyEvent char_event(key_value, code, ui::DomCode::NONE, ui::EF_NONE);
      if (tic)
        tic->InsertChar(char_event);
      SendProcessKeyEvent(ui::ET_KEY_RELEASED, host);
    }
    return true;
  }

  ui::DomCode dom_code = ui::KeycodeConverter::CodeStringToDomCode(key_name);
  if (dom_code == ui::DomCode::NONE)
    dom_code = ui::UsLayoutKeyboardCodeToDomCode(code);
  CHECK(dom_code != ui::DomCode::NONE);

  ui::KeyEvent event(event_type, code, dom_code, modifiers);

  // Indicate that the simulated key event is from the Virtual Keyboard.
  ui::Event::Properties properties;
  properties[ui::kPropertyFromVK] =
      std::vector<uint8_t>(ui::kPropertyFromVKSize);
  event.SetProperties(properties);

  ui::EventDispatchDetails details = aura::EventInjector().Inject(host, &event);
  CHECK(!details.dispatcher_destroyed);
  return true;
}

std::string GetKeyboardLayout() {
  // TODO(bshe): layout string is currently hard coded. We should use more
  // standard keyboard layouts.
  return ChromeKeyboardControllerClient::Get()->IsEnableFlagSet(
             keyboard::KeyboardEnableFlag::kAccessibilityEnabled)
             ? "system-qwerty"
             : "qwerty";
}

// Returns a nullptr if a router could not be found or if the the router does
// not have an event listener for the given `event_name`.
extensions::EventRouter* GetRouterForEventName(content::BrowserContext* context,
                                               const std::string& event_name) {
  extensions::EventRouter* router = extensions::EventRouter::Get(context);
  if (!router || !router->HasEventListener(event_name)) {
    return nullptr;
  }
  return router;
}

}  // namespace

namespace extensions {

ChromeVirtualKeyboardDelegate::ChromeVirtualKeyboardDelegate(
    content::BrowserContext* browser_context)
    : browser_context_(browser_context) {
  weak_this_ = weak_factory_.GetWeakPtr();

  ash::ClipboardHistoryController* clipboard_history_controller =
      ash::ClipboardHistoryController::Get();
  if (clipboard_history_controller) {
    clipboard_history_controller->AddObserver(this);
  }
}

ChromeVirtualKeyboardDelegate::~ChromeVirtualKeyboardDelegate() {
  ash::ClipboardHistoryController* clipboard_history_controller =
      ash::ClipboardHistoryController::Get();
  if (clipboard_history_controller)
    clipboard_history_controller->RemoveObserver(this);
}

void ChromeVirtualKeyboardDelegate::GetKeyboardConfig(
    OnKeyboardSettingsCallback on_settings_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!audio_system_)
    audio_system_ = content::CreateAudioSystemForAudioService();
  audio_system_->HasInputDevices(
      base::BindOnce(&ChromeVirtualKeyboardDelegate::OnHasInputDevices,
                     weak_this_, std::move(on_settings_callback)));
}

void ChromeVirtualKeyboardDelegate::OnKeyboardConfigChanged() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  GetKeyboardConfig(base::BindOnce(
      &ChromeVirtualKeyboardDelegate::DispatchConfigChangeEvent, weak_this_));
}

bool ChromeVirtualKeyboardDelegate::HideKeyboard() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  auto* keyboard_client = ChromeKeyboardControllerClient::Get();
  if (!keyboard_client->is_keyboard_enabled())
    return false;

  // Pass HIDE_REASON_MANUAL since calls to HideKeyboard as part of this API
  // would be user generated.
  keyboard_client->HideKeyboard(ash::HideReason::kUser);
  return true;
}

bool ChromeVirtualKeyboardDelegate::InsertText(const std::u16string& text) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  ui::TextInputClient* tic = GetFocusedTextInputClient();
  if (!tic || tic->GetTextInputType() == ui::TEXT_INPUT_TYPE_NONE)
    return false;

  tic->InsertText(
      text,
      ui::TextInputClient::InsertTextCursorBehavior::kMoveCursorAfterText);
  return true;
}

bool ChromeVirtualKeyboardDelegate::OnKeyboardLoaded() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  base::RecordAction(base::UserMetricsAction("VirtualKeyboardLoaded"));
  return true;
}

void ChromeVirtualKeyboardDelegate::SetHotrodKeyboard(bool enable) {
  if (g_hotrod_keyboard_enabled == enable)
    return;

  g_hotrod_keyboard_enabled = enable;

  // This reloads virtual keyboard even if it exists. This ensures virtual
  // keyboard gets the correct state of the hotrod keyboard through
  // chrome.virtualKeyboardPrivate.getKeyboardConfig.
  ChromeKeyboardControllerClient::Get()->RebuildKeyboardIfEnabled();
}

bool ChromeVirtualKeyboardDelegate::LockKeyboard(bool state) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  auto* keyboard_client = ChromeKeyboardControllerClient::Get();
  if (!keyboard_client->is_keyboard_enabled())
    return false;

  keyboard_client->SetKeyboardLocked(state);
  return true;
}

bool ChromeVirtualKeyboardDelegate::SendKeyEvent(const std::string& type,
                                                 int char_value,
                                                 int key_code,
                                                 const std::string& key_name,
                                                 int modifiers) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  aura::Window* window =
      ChromeKeyboardControllerClient::Get()->GetKeyboardWindow();
  return window && SendKeyEventImpl(type, char_value, key_code, key_name,
                                    modifiers, window->GetHost());
}

bool ChromeVirtualKeyboardDelegate::ShowLanguageSettings() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  auto* keyboard_client = ChromeKeyboardControllerClient::Get();
  if (keyboard_client->is_keyboard_enabled())
    keyboard_client->HideKeyboard(ash::HideReason::kUser);

  base::RecordAction(
      base::UserMetricsAction("VirtualKeyboard.OpenLanguageSettings"));
  chrome::SettingsWindowManager::GetInstance()->ShowOSSettings(
      ProfileManager::GetActiveUserProfile(),
      chromeos::settings::mojom::kInputSubpagePath);
  return true;
}

bool ChromeVirtualKeyboardDelegate::ShowSuggestionSettings() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  auto* keyboard_client = ChromeKeyboardControllerClient::Get();
  if (keyboard_client->is_keyboard_enabled())
    keyboard_client->HideKeyboard(ash::HideReason::kUser);

  base::RecordAction(
      base::UserMetricsAction("VirtualKeyboard.OpenSuggestionSettings"));
  chrome::SettingsWindowManager::GetInstance()->ShowOSSettings(
      ProfileManager::GetActiveUserProfile(),
      chromeos::settings::mojom::kSmartInputsSubpagePath);
  return true;
}

bool ChromeVirtualKeyboardDelegate::SetVirtualKeyboardMode(
    int mode_enum,
    gfx::Rect target_bounds,
    OnSetModeCallback on_set_mode_callback) {
  auto* keyboard_client = ChromeKeyboardControllerClient::Get();
  if (!keyboard_client->is_keyboard_enabled())
    return false;

  keyboard_client->SetContainerType(
      ConvertKeyboardModeToContainerType(mode_enum), target_bounds,
      std::move(on_set_mode_callback));
  return true;
}

bool ChromeVirtualKeyboardDelegate::SetOccludedBounds(
    const std::vector<gfx::Rect>& bounds) {
  auto* keyboard_client = ChromeKeyboardControllerClient::Get();
  if (!keyboard_client->is_keyboard_enabled())
    return false;

  keyboard_client->SetOccludedBounds(bounds);
  return true;
}

bool ChromeVirtualKeyboardDelegate::SetHitTestBounds(
    const std::vector<gfx::Rect>& bounds) {
  auto* keyboard_client = ChromeKeyboardControllerClient::Get();
  if (!keyboard_client->is_keyboard_enabled())
    return false;

  keyboard_client->SetHitTestBounds(bounds);
  return true;
}

bool ChromeVirtualKeyboardDelegate::SetAreaToRemainOnScreen(
    const gfx::Rect& bounds) {
  auto* keyboard_client = ChromeKeyboardControllerClient::Get();
  if (!keyboard_client->is_keyboard_enabled())
    return false;

  return keyboard_client->SetAreaToRemainOnScreen(bounds);
}

bool ChromeVirtualKeyboardDelegate::SetWindowBoundsInScreen(
    const gfx::Rect& bounds_in_screen) {
  auto* keyboard_client = ChromeKeyboardControllerClient::Get();
  if (!keyboard_client->is_keyboard_enabled())
    return false;

  return keyboard_client->SetWindowBoundsInScreen(bounds_in_screen);
}

void ChromeVirtualKeyboardDelegate::GetClipboardHistory(
    const std::set<std::string>& item_ids_filter,
    OnGetClipboardHistoryCallback get_history_callback) {
  ash::ClipboardHistoryController* clipboard_history_controller =
      ash::ClipboardHistoryController::Get();
  if (!clipboard_history_controller)
    return;

  // Begin renderng all items in the clipboard history. Current items will
  // render even if Deactivate() is called on the ClipboardImageModelFactory.
  if (ash::ClipboardImageModelFactory::Get()) {
    ash::ClipboardImageModelFactory::Get()->RenderCurrentPendingRequests();
  }

  clipboard_history_controller->GetHistoryValues(
      item_ids_filter, std::move(get_history_callback));
}

bool ChromeVirtualKeyboardDelegate::PasteClipboardItem(
    const std::string& clipboard_item_id) {
  ash::ClipboardHistoryController* clipboard_history_controller =
      ash::ClipboardHistoryController::Get();
  if (!clipboard_history_controller)
    return false;

  return clipboard_history_controller->PasteClipboardItemById(
      clipboard_item_id);
}

bool ChromeVirtualKeyboardDelegate::DeleteClipboardItem(
    const std::string& clipboard_item_id) {
  ash::ClipboardHistoryController* clipboard_history_controller =
      ash::ClipboardHistoryController::Get();
  if (!clipboard_history_controller)
    return false;

  return clipboard_history_controller->DeleteClipboardItemById(
      clipboard_item_id);
}

bool ChromeVirtualKeyboardDelegate::SetDraggableArea(
    const api::virtual_keyboard_private::Bounds& rect) {
  auto* keyboard_client = ChromeKeyboardControllerClient::Get();
  // Since controller will be destroyed when system switch from VK to
  // physical keyboard, return true to avoid unnecessary exception.
  if (!keyboard_client->is_keyboard_enabled())
    return true;

  keyboard_client->SetDraggableArea(
      gfx::Rect(rect.left, rect.top, rect.width, rect.height));
  return true;
}

bool ChromeVirtualKeyboardDelegate::SetRequestedKeyboardState(int state_enum) {
  using keyboard::KeyboardEnableFlag;
  auto* client = ChromeKeyboardControllerClient::Get();
  keyboard_api::KeyboardState state =
      static_cast<keyboard_api::KeyboardState>(state_enum);
  switch (state) {
    case keyboard_api::KEYBOARD_STATE_ENABLED:
      client->SetEnableFlag(KeyboardEnableFlag::kExtensionEnabled);
      break;
    case keyboard_api::KEYBOARD_STATE_DISABLED:
      client->SetEnableFlag(KeyboardEnableFlag::kExtensionDisabled);
      break;
    case keyboard_api::KEYBOARD_STATE_AUTO:
    case keyboard_api::KEYBOARD_STATE_NONE:
      client->ClearEnableFlag(KeyboardEnableFlag::kExtensionDisabled);
      client->ClearEnableFlag(KeyboardEnableFlag::kExtensionEnabled);
      break;
  }
  return true;
}

bool ChromeVirtualKeyboardDelegate::IsSettingsEnabled() {
  return (user_manager::UserManager::Get()->IsUserLoggedIn() &&
          !ash::UserAddingScreen::Get()->IsRunning() &&
          !(ash::ScreenLocker::default_screen_locker() &&
            ash::ScreenLocker::default_screen_locker()->locked()));
}

void ChromeVirtualKeyboardDelegate::OnClipboardHistoryItemListAddedOrRemoved() {
  EventRouter* router = GetRouterForEventName(
      browser_context_, keyboard_api::OnClipboardHistoryChanged::kEventName);
  if (!router)
    return;

  ash::ClipboardHistoryController* clipboard_history_controller =
      ash::ClipboardHistoryController::Get();
  if (!clipboard_history_controller)
    return;

  auto item_ids = clipboard_history_controller->GetHistoryItemIds();

  auto ids = keyboard_api::OnClipboardHistoryChanged::Create(item_ids);

  auto event = std::make_unique<extensions::Event>(
      extensions::events::VIRTUAL_KEYBOARD_PRIVATE_ON_CLIPBOARD_HISTORY_CHANGED,
      keyboard_api::OnClipboardHistoryChanged::kEventName, std::move(ids),
      browser_context_);
  router->BroadcastEvent(std::move(event));
}

void ChromeVirtualKeyboardDelegate::OnClipboardHistoryItemsUpdated(
    const std::vector<base::UnguessableToken>& menu_item_ids) {
  EventRouter* router = GetRouterForEventName(
      browser_context_, keyboard_api::OnClipboardItemUpdated::kEventName);
  if (!router)
    return;

  ash::ClipboardHistoryController* clipboard_history_controller =
      ash::ClipboardHistoryController::Get();
  if (!clipboard_history_controller)
    return;

  std::set<std::string> item_ids_filter;
  for (const auto& id : menu_item_ids) {
    item_ids_filter.insert(id.ToString());
  }
  // Make call to get the updated clipboard items.
  clipboard_history_controller->GetHistoryValues(
      item_ids_filter,
      base::BindOnce(
          &ChromeVirtualKeyboardDelegate::OnGetHistoryValuesAfterItemsUpdated,
          weak_this_));
}

void ChromeVirtualKeyboardDelegate::OnGetHistoryValuesAfterItemsUpdated(
    base::Value updated_items) {
  EventRouter* router = GetRouterForEventName(
      browser_context_, keyboard_api::OnClipboardItemUpdated::kEventName);
  if (!router)
    return;

  // Broadcast an api event for each updated item.
  for (auto& item : updated_items.GetList()) {
    keyboard_api::ClipboardItem clipboard_item;
    if (item.FindKey("imageData")) {
      clipboard_item.image_data =
          std::make_unique<std::string>(item.FindKey("imageData")->GetString());
    }
    if (item.FindKey("textData")) {
      clipboard_item.text_data =
          std::make_unique<std::string>(item.FindKey("textData")->GetString());
    }
    if (item.FindKey("idToken")) {
      clipboard_item.id = item.FindKey("idToken")->GetString();
    }

    auto item_value =
        keyboard_api::OnClipboardItemUpdated::Create(clipboard_item);

    auto event = std::make_unique<extensions::Event>(
        extensions::events::VIRTUAL_KEYBOARD_PRIVATE_ON_CLIPBOARD_ITEM_UPDATED,
        keyboard_api::OnClipboardItemUpdated::kEventName, std::move(item_value),
        browser_context_);
    router->BroadcastEvent(std::move(event));
  }
}

void ChromeVirtualKeyboardDelegate::OnHasInputDevices(
    OnKeyboardSettingsCallback on_settings_callback,
    bool has_audio_input_devices) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  auto* keyboard_client = ChromeKeyboardControllerClient::Get();

  std::unique_ptr<base::DictionaryValue> results(new base::DictionaryValue());
  results->SetString("layout", GetKeyboardLayout());

  // TODO(bshe): Consolidate a11y, hotrod and normal mode into a mode enum. See
  // crbug.com/529474.
  results->SetBoolean("a11ymode",
                      keyboard_client->IsEnableFlagSet(
                          keyboard::KeyboardEnableFlag::kAccessibilityEnabled));
  results->SetBoolean("hotrodmode", g_hotrod_keyboard_enabled);
  base::Value features(base::Value::Type::LIST);

  keyboard::KeyboardConfig config = keyboard_client->GetKeyboardConfig();
  // TODO(oka): Change this to use config.voice_input.
  features.Append(GenerateFeatureFlag(
      "voiceinput", has_audio_input_devices && config.voice_input));
  features.Append(GenerateFeatureFlag("autocomplete", config.auto_complete));
  features.Append(GenerateFeatureFlag("autocorrect", config.auto_correct));
  features.Append(GenerateFeatureFlag("spellcheck", config.spell_check));
  features.Append(GenerateFeatureFlag("handwriting", config.handwriting));
  features.Append(GenerateFeatureFlag(
      "handwritinggesture",
      base::FeatureList::IsEnabled(features::kHandwritingGesture)));
  features.Append(
      GenerateFeatureFlag("handwritinggestureediting",
                          base::FeatureList::IsEnabled(
                              chromeos::features::kHandwritingGestureEditing)));
  features.Append(GenerateFeatureFlag(
      "handwritinglegacyrecognition",
      base::FeatureList::IsEnabled(
          chromeos::features::kHandwritingLegacyRecognition)));
  features.Append(GenerateFeatureFlag(
      "handwritinglegacyrecognitionall",
      base::FeatureList::IsEnabled(
          chromeos::features::kHandwritingLegacyRecognitionAllLang)));
  features.Append(GenerateFeatureFlag(
      "multiword", chromeos::features::IsAssistiveMultiWordEnabled()));
  features.Append(GenerateFeatureFlag(
      "stylushandwriting",
      base::FeatureList::IsEnabled(chromeos::features::kImeStylusHandwriting)));
  features.Append(GenerateFeatureFlag(
      "darkmode", base::FeatureList::IsEnabled(
                      chromeos::features::kVirtualKeyboardDarkMode)));

  // Flag used to enable system built-in IME decoder instead of NaCl.
  features.Append(GenerateFeatureFlag("usemojodecoder", true));
  // Enabling MojoDecoder implies the 2 previous flags are auto-enabled.
  //   * fstinputlogic
  //   * hmminputlogic
  // TODO(b/171846787): Remove the 3 flags after they are removed from clients.
  features.Append(GenerateFeatureFlag("fstinputlogic", true));
  features.Append(GenerateFeatureFlag("hmminputlogic", true));

  features.Append(GenerateFeatureFlag(
      "borderedkey", base::FeatureList::IsEnabled(
                         chromeos::features::kVirtualKeyboardBorderedKey)));
  features.Append(GenerateFeatureFlag(
      "assistiveAutoCorrect",
      base::FeatureList::IsEnabled(chromeos::features::kAssistAutoCorrect)));
  features.Append(GenerateFeatureFlag(
      "systemchinesephysicaltyping",
      chromeos::features::IsSystemChinesePhysicalTypingEnabled()));
  features.Append(GenerateFeatureFlag(
      "systemjapanesephysicaltyping",
      chromeos::features::IsSystemJapanesePhysicalTypingEnabled()));
  features.Append(GenerateFeatureFlag(
      "systemkoreanphysicaltyping",
      chromeos::features::IsSystemKoreanPhysicalTypingEnabled()));
  features.Append(GenerateFeatureFlag("systemlatinphysicaltyping", true));
  features.Append(GenerateFeatureFlag(
      "multilingualtyping",
      base::FeatureList::IsEnabled(chromeos::features::kMultilingualTyping)));
  features.Append(GenerateFeatureFlag(
      "imeoptionsinsettings",
      base::FeatureList::IsEnabled(chromeos::features::kImeOptionsInSettings)));

  results->SetKey("features", std::move(features));

  std::move(on_settings_callback).Run(std::move(results));
}

void ChromeVirtualKeyboardDelegate::DispatchConfigChangeEvent(
    std::unique_ptr<base::DictionaryValue> settings) {
  EventRouter* router = GetRouterForEventName(
      browser_context_, keyboard_api::OnKeyboardConfigChanged::kEventName);
  if (!router)
    return;

  auto event_args = std::make_unique<base::ListValue>();
  event_args->Append(std::move(settings));

  auto event = std::make_unique<extensions::Event>(
      extensions::events::VIRTUAL_KEYBOARD_PRIVATE_ON_KEYBOARD_CONFIG_CHANGED,
      keyboard_api::OnKeyboardConfigChanged::kEventName,
      std::move(*event_args).TakeList(), browser_context_);
  router->BroadcastEvent(std::move(event));
}

api::virtual_keyboard::FeatureRestrictions
ChromeVirtualKeyboardDelegate::RestrictFeatures(
    const api::virtual_keyboard::RestrictFeatures::Params& params) {
  const api::virtual_keyboard::FeatureRestrictions& restrictions =
      params.restrictions;
  api::virtual_keyboard::FeatureRestrictions update;
  keyboard::KeyboardConfig current_config =
      ChromeKeyboardControllerClient::Get()->GetKeyboardConfig();
  keyboard::KeyboardConfig config(current_config);
  if (restrictions.spell_check_enabled &&
      config.spell_check != *restrictions.spell_check_enabled) {
    update.spell_check_enabled =
        std::make_unique<bool>(*restrictions.spell_check_enabled);
    config.spell_check = *restrictions.spell_check_enabled;
  }
  if (restrictions.auto_complete_enabled &&
      config.auto_complete != *restrictions.auto_complete_enabled) {
    update.auto_complete_enabled =
        std::make_unique<bool>(*restrictions.auto_complete_enabled);
    config.auto_complete = *restrictions.auto_complete_enabled;
  }
  if (restrictions.auto_correct_enabled &&
      config.auto_correct != *restrictions.auto_correct_enabled) {
    update.auto_correct_enabled =
        std::make_unique<bool>(*restrictions.auto_correct_enabled);
    config.auto_correct = *restrictions.auto_correct_enabled;
  }
  if (restrictions.voice_input_enabled &&
      config.voice_input != *restrictions.voice_input_enabled) {
    update.voice_input_enabled =
        std::make_unique<bool>(*restrictions.voice_input_enabled);
    config.voice_input = *restrictions.voice_input_enabled;
  }
  if (restrictions.handwriting_enabled &&
      config.handwriting != *restrictions.handwriting_enabled) {
    update.handwriting_enabled =
        std::make_unique<bool>(*restrictions.handwriting_enabled);
    config.handwriting = *restrictions.handwriting_enabled;
  }

  if (config != current_config) {
    ChromeKeyboardControllerClient::Get()->SetKeyboardConfig(config);
    // This reloads the virtual keyboard (VK) even if it exists, so it can get
    // new restrictFeatures via chrome.virtualKeyboardPrivate.getKeyboardConfig.
    // However, this reload is unnecessary as the API specs do NOT require
    // restrictFeatures to take effect immediately midway through a VK session.
    // Keeping this unnecessary reload for now, just to avoid behaviour changes.
    ChromeKeyboardControllerClient::Get()->RebuildKeyboardIfEnabled();
  }
  return update;
}

}  // namespace extensions
