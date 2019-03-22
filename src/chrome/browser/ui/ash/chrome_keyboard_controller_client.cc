// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/chrome_keyboard_controller_client.h"

#include <memory>

#include "ash/public/interfaces/constants.mojom.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/stl_util.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "extensions/browser/api/virtual_keyboard_private/virtual_keyboard_delegate.h"
#include "extensions/browser/api/virtual_keyboard_private/virtual_keyboard_private_api.h"
#include "extensions/browser/event_router.h"
#include "extensions/common/api/virtual_keyboard_private.h"
#include "extensions/common/extension_messages.h"
#include "services/service_manager/public/cpp/connector.h"
#include "ui/base/ime/chromeos/input_method_manager.h"
#include "ui/base/ime/ime_bridge.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/keyboard/keyboard_controller.h"
#include "ui/keyboard/public/keyboard_switches.h"
#include "ui/keyboard/resources/keyboard_resource_util.h"

namespace virtual_keyboard_private = extensions::api::virtual_keyboard_private;

namespace {

static ChromeKeyboardControllerClient* g_chrome_keyboard_controller_client =
    nullptr;

}  // namespace

// static
ChromeKeyboardControllerClient* ChromeKeyboardControllerClient::Get() {
  CHECK(g_chrome_keyboard_controller_client)
      << "ChromeKeyboardControllerClient::Get() called before Initialize()";
  return g_chrome_keyboard_controller_client;
}

// static
bool ChromeKeyboardControllerClient::HasInstance() {
  return !!g_chrome_keyboard_controller_client;
}

ChromeKeyboardControllerClient::ChromeKeyboardControllerClient(
    service_manager::Connector* connector) {
  CHECK(!g_chrome_keyboard_controller_client);
  g_chrome_keyboard_controller_client = this;

  if (!connector)
    return;  // May be null in tests.

  connector->BindInterface(ash::mojom::kServiceName, &keyboard_controller_ptr_);

  // Add this as a KeyboardController observer.
  ash::mojom::KeyboardControllerObserverAssociatedPtrInfo ptr_info;
  keyboard_controller_observer_binding_.Bind(mojo::MakeRequest(&ptr_info));
  keyboard_controller_ptr_->AddObserver(std::move(ptr_info));

  // Request the initial enabled state.
  keyboard_controller_ptr_->IsKeyboardEnabled(
      base::BindOnce(&ChromeKeyboardControllerClient::OnKeyboardEnabledChanged,
                     weak_ptr_factory_.GetWeakPtr()));

  // Request the initial set of enable flags.
  keyboard_controller_ptr_->GetEnableFlags(base::BindOnce(
      &ChromeKeyboardControllerClient::OnKeyboardEnableFlagsChanged,
      weak_ptr_factory_.GetWeakPtr()));

  // Request the initial visible state.
  keyboard_controller_ptr_->IsKeyboardVisible(base::BindOnce(
      &ChromeKeyboardControllerClient::OnKeyboardVisibilityChanged,
      weak_ptr_factory_.GetWeakPtr()));

  // Request the configuration.
  keyboard_controller_ptr_->GetKeyboardConfig(
      base::BindOnce(&ChromeKeyboardControllerClient::OnKeyboardConfigChanged,
                     weak_ptr_factory_.GetWeakPtr()));
}

ChromeKeyboardControllerClient::~ChromeKeyboardControllerClient() {
  CHECK(g_chrome_keyboard_controller_client);
  g_chrome_keyboard_controller_client = nullptr;
}

void ChromeKeyboardControllerClient::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void ChromeKeyboardControllerClient::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void ChromeKeyboardControllerClient::NotifyKeyboardLoaded() {
  is_keyboard_loaded_ = true;
  for (auto& observer : observers_)
    observer.OnKeyboardLoaded();
}

keyboard::mojom::KeyboardConfig
ChromeKeyboardControllerClient::GetKeyboardConfig() {
  if (!cached_keyboard_config_) {
    // Unlikely edge case (called before the Ash mojo service replies to the
    // initial GetKeyboardConfig request). Return the default value.
    return keyboard::mojom::KeyboardConfig();
  }
  return *cached_keyboard_config_.get();
}

void ChromeKeyboardControllerClient::SetKeyboardConfig(
    const keyboard::mojom::KeyboardConfig& config) {
  // Update the cache immediately.
  cached_keyboard_config_ = keyboard::mojom::KeyboardConfig::New(config);
  keyboard_controller_ptr_->SetKeyboardConfig(config.Clone());
}

void ChromeKeyboardControllerClient::GetKeyboardEnabled(
    base::OnceCallback<void(bool)> callback) {
  keyboard_controller_ptr_->IsKeyboardEnabled(std::move(callback));
}

void ChromeKeyboardControllerClient::SetEnableFlag(
    const keyboard::mojom::KeyboardEnableFlag& flag) {
  keyboard_controller_ptr_->SetEnableFlag(flag);
}

void ChromeKeyboardControllerClient::ClearEnableFlag(
    const keyboard::mojom::KeyboardEnableFlag& flag) {
  keyboard_controller_ptr_->ClearEnableFlag(flag);
}

bool ChromeKeyboardControllerClient::IsEnableFlagSet(
    const keyboard::mojom::KeyboardEnableFlag& flag) {
  return base::ContainsKey(keyboard_enable_flags_, flag);
}

void ChromeKeyboardControllerClient::ReloadKeyboardIfNeeded() {
  keyboard_controller_ptr_->ReloadKeyboardIfNeeded();
}

void ChromeKeyboardControllerClient::RebuildKeyboardIfEnabled() {
  keyboard_controller_ptr_->RebuildKeyboardIfEnabled();
}

void ChromeKeyboardControllerClient::ShowKeyboard() {
  keyboard_controller_ptr_->ShowKeyboard();
}

void ChromeKeyboardControllerClient::HideKeyboard(
    ash::mojom::HideReason reason) {
  keyboard_controller_ptr_->HideKeyboard(reason);
}

void ChromeKeyboardControllerClient::SetContainerType(
    keyboard::mojom::ContainerType container_type,
    const base::Optional<gfx::Rect>& target_bounds,
    base::OnceCallback<void(bool)> callback) {
  keyboard_controller_ptr_->SetContainerType(container_type, target_bounds,
                                             std::move(callback));
}

void ChromeKeyboardControllerClient::SetKeyboardLocked(bool locked) {
  keyboard_controller_ptr_->SetKeyboardLocked(locked);
}

void ChromeKeyboardControllerClient::SetOccludedBounds(
    const std::vector<gfx::Rect>& bounds) {
  keyboard_controller_ptr_->SetOccludedBounds(bounds);
}

void ChromeKeyboardControllerClient::SetHitTestBounds(
    const std::vector<gfx::Rect>& bounds) {
  keyboard_controller_ptr_->SetHitTestBounds(bounds);
}

void ChromeKeyboardControllerClient::SetDraggableArea(const gfx::Rect& bounds) {
  keyboard_controller_ptr_->SetDraggableArea(bounds);
}

bool ChromeKeyboardControllerClient::IsKeyboardOverscrollEnabled() {
  DCHECK(cached_keyboard_config_);
  if (cached_keyboard_config_->overscroll_behavior !=
      keyboard::mojom::KeyboardOverscrollBehavior::kDefault) {
    return cached_keyboard_config_->overscroll_behavior ==
           keyboard::mojom::KeyboardOverscrollBehavior::kEnabled;
  }
  return !base::CommandLine::ForCurrentProcess()->HasSwitch(
      keyboard::switches::kDisableVirtualKeyboardOverscroll);
}

GURL ChromeKeyboardControllerClient::GetVirtualKeyboardUrl() {
  if (!virtual_keyboard_url_for_test_.is_empty())
    return virtual_keyboard_url_for_test_;

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          keyboard::switches::kDisableInputView)) {
    return GURL(keyboard::kKeyboardURL);
  }

  chromeos::input_method::InputMethodManager* ime_manager =
      chromeos::input_method::InputMethodManager::Get();
  if (!ime_manager || !ime_manager->GetActiveIMEState())
    return GURL(keyboard::kKeyboardURL);

  const GURL& input_view_url =
      ime_manager->GetActiveIMEState()->GetInputViewUrl();
  if (!input_view_url.is_valid())
    return GURL(keyboard::kKeyboardURL);

  return input_view_url;
}

aura::Window* ChromeKeyboardControllerClient::GetKeyboardWindow() const {
  if (::features::IsUsingWindowService()) {
    // TODO(stevenjb): When WS support is added, return the Chrome window
    // hosting the extension instead.
    return nullptr;
  }
  return keyboard::KeyboardController::Get()->GetKeyboardWindow();
}

void ChromeKeyboardControllerClient::FlushForTesting() {
  keyboard_controller_ptr_.FlushForTesting();
}

void ChromeKeyboardControllerClient::OnKeyboardEnableFlagsChanged(
    const std::vector<keyboard::mojom::KeyboardEnableFlag>& flags) {
  keyboard_enable_flags_ =
      std::set<keyboard::mojom::KeyboardEnableFlag>(flags.begin(), flags.end());
}

void ChromeKeyboardControllerClient::OnKeyboardEnabledChanged(bool enabled) {
  bool was_enabled = is_keyboard_enabled_;
  is_keyboard_enabled_ = enabled;
  if (enabled || !was_enabled)
    return;

  // When the keyboard becomes disabled, send the onKeyboardClosed event.

  Profile* profile = GetProfile();
  extensions::EventRouter* router = extensions::EventRouter::Get(profile);
  // |router| may be null in tests.
  if (!router || !router->HasEventListener(
                     virtual_keyboard_private::OnKeyboardClosed::kEventName)) {
    return;
  }

  auto event = std::make_unique<extensions::Event>(
      extensions::events::VIRTUAL_KEYBOARD_PRIVATE_ON_KEYBOARD_CLOSED,
      virtual_keyboard_private::OnKeyboardClosed::kEventName,
      std::make_unique<base::ListValue>(), profile);
  router->BroadcastEvent(std::move(event));
}

void ChromeKeyboardControllerClient::OnKeyboardConfigChanged(
    keyboard::mojom::KeyboardConfigPtr config) {
  // Only notify extensions after the initial config is received.
  bool notify = !!cached_keyboard_config_;
  cached_keyboard_config_ = std::move(config);
  if (!notify)
    return;
  extensions::VirtualKeyboardAPI* api =
      extensions::BrowserContextKeyedAPIFactory<
          extensions::VirtualKeyboardAPI>::Get(GetProfile());
  api->delegate()->OnKeyboardConfigChanged();
}

void ChromeKeyboardControllerClient::OnKeyboardVisibilityChanged(bool visible) {
  is_keyboard_visible_ = visible;
  for (auto& observer : observers_)
    observer.OnKeyboardVisibilityChanged(visible);
}

void ChromeKeyboardControllerClient::OnKeyboardVisibleBoundsChanged(
    const gfx::Rect& screen_bounds) {
  Profile* profile = GetProfile();
  extensions::EventRouter* router = extensions::EventRouter::Get(profile);
  // |router| may be null in tests.
  if (!router || !router->HasEventListener(
                     virtual_keyboard_private::OnBoundsChanged::kEventName)) {
    return;
  }

  // Note: |bounds| is in screen coordinates, which is what we want to pass to
  // the extension.
  auto event_args = std::make_unique<base::ListValue>();
  auto new_bounds = std::make_unique<base::DictionaryValue>();
  new_bounds->SetInteger("left", screen_bounds.x());
  new_bounds->SetInteger("top", screen_bounds.y());
  new_bounds->SetInteger("width", screen_bounds.width());
  new_bounds->SetInteger("height", screen_bounds.height());
  event_args->Append(std::move(new_bounds));

  auto event = std::make_unique<extensions::Event>(
      extensions::events::VIRTUAL_KEYBOARD_PRIVATE_ON_BOUNDS_CHANGED,
      virtual_keyboard_private::OnBoundsChanged::kEventName,
      std::move(event_args), profile);
  router->BroadcastEvent(std::move(event));
}

void ChromeKeyboardControllerClient::OnKeyboardOccludedBoundsChanged(
    const gfx::Rect& screen_bounds) {
  for (auto& observer : observers_)
    observer.OnKeyboardOccludedBoundsChanged(screen_bounds);
}

Profile* ChromeKeyboardControllerClient::GetProfile() {
  if (profile_for_test_)
    return profile_for_test_;

  // Always use the active profile for generating keyboard events so that any
  // virtual keyboard extensions associated with the active user are notified.
  // (Note: UI and associated extensions only exist for the active user).
  return ProfileManager::GetActiveUserProfile();
}
