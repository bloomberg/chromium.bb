// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/chromeos/mock_input_method_manager.h"

#include <utility>

namespace chromeos {
namespace input_method {
MockInputMethodManager::State::State() {}

void MockInputMethodManager::State::AddInputMethodExtension(
    const std::string& extension_id,
    const InputMethodDescriptors& descriptors,
    ui::IMEEngineHandlerInterface* instance) {}

void MockInputMethodManager::State::RemoveInputMethodExtension(
    const std::string& extension_id) {}

void MockInputMethodManager::State::ChangeInputMethod(
    const std::string& input_method_id,
    bool show_message) {}

bool MockInputMethodManager::State::EnableInputMethod(
    const std::string& new_active_input_method_id) {
  return true;
}

void MockInputMethodManager::State::EnableLoginLayouts(
    const std::string& language_code,
    const std::vector<std::string>& initial_layout) {}

void MockInputMethodManager::State::EnableLockScreenLayouts() {}

void MockInputMethodManager::State::GetInputMethodExtensions(
    InputMethodDescriptors* result) {}

std::unique_ptr<InputMethodDescriptors>
MockInputMethodManager::State::GetActiveInputMethods() const {
  return nullptr;
}

const std::vector<std::string>&
MockInputMethodManager::State::GetActiveInputMethodIds() const {
  return active_input_method_ids;
}

const InputMethodDescriptor*
MockInputMethodManager::State::GetInputMethodFromId(
    const std::string& input_method_id) const {
  return nullptr;
}

size_t MockInputMethodManager::State::GetNumActiveInputMethods() const {
  return active_input_method_ids.size();
}

void MockInputMethodManager::State::SetEnabledExtensionImes(
    std::vector<std::string>* ids) {}

void MockInputMethodManager::State::SetInputMethodLoginDefault() {}

void MockInputMethodManager::State::SetInputMethodLoginDefaultFromVPD(
    const std::string& locale,
    const std::string& layout) {}

bool MockInputMethodManager::State::CanCycleInputMethod() {
  return true;
}

void MockInputMethodManager::State::SwitchToNextInputMethod() {}

void MockInputMethodManager::State::SwitchToPreviousInputMethod() {}

bool MockInputMethodManager::State::CanSwitchInputMethod(
    const ui::Accelerator& accelerator) {
  return true;
}

void MockInputMethodManager::State::SwitchInputMethod(
    const ui::Accelerator& accelerator) {}

InputMethodDescriptor MockInputMethodManager::State::GetCurrentInputMethod()
    const {
  InputMethodDescriptor descriptor;
  return descriptor;
}

bool MockInputMethodManager::State::ReplaceEnabledInputMethods(
    const std::vector<std::string>& new_active_input_method_ids) {
  return true;
}

bool MockInputMethodManager::State::SetAllowedInputMethods(
    const std::vector<std::string>& new_allowed_input_method_ids) {
  allowed_input_method_ids_ = new_allowed_input_method_ids;
  return true;
}

const std::vector<std::string>&
MockInputMethodManager::State::GetAllowedInputMethods() {
  return allowed_input_method_ids_;
}

MockInputMethodManager::State::~State() {}

MockInputMethodManager::MockInputMethodManager() {}

MockInputMethodManager::~MockInputMethodManager() {}

InputMethodManager::UISessionState MockInputMethodManager::GetUISessionState() {
  return InputMethodManager::STATE_BROWSER_SCREEN;
}

void MockInputMethodManager::AddObserver(
    InputMethodManager::Observer* observer) {}

void MockInputMethodManager::AddCandidateWindowObserver(
    InputMethodManager::CandidateWindowObserver* observer) {}

void MockInputMethodManager::AddImeMenuObserver(
    InputMethodManager::ImeMenuObserver* observer) {}

void MockInputMethodManager::RemoveObserver(
    InputMethodManager::Observer* observer) {}

void MockInputMethodManager::RemoveCandidateWindowObserver(
    InputMethodManager::CandidateWindowObserver* observer) {}

void MockInputMethodManager::RemoveImeMenuObserver(
    InputMethodManager::ImeMenuObserver* observer) {}

std::unique_ptr<InputMethodDescriptors>
MockInputMethodManager::GetSupportedInputMethods() const {
  return nullptr;
}

void MockInputMethodManager::ActivateInputMethodMenuItem(
    const std::string& key) {}

bool MockInputMethodManager::IsISOLevel5ShiftUsedByCurrentInputMethod() const {
  return false;
}

bool MockInputMethodManager::IsAltGrUsedByCurrentInputMethod() const {
  return false;
}

ImeKeyboard* MockInputMethodManager::GetImeKeyboard() {
  return nullptr;
}

InputMethodUtil* MockInputMethodManager::GetInputMethodUtil() {
  return nullptr;
}

ComponentExtensionIMEManager*
MockInputMethodManager::GetComponentExtensionIMEManager() {
  return nullptr;
}

bool MockInputMethodManager::IsLoginKeyboard(const std::string& layout) const {
  return true;
}

bool MockInputMethodManager::MigrateInputMethods(
    std::vector<std::string>* input_method_ids) {
  return false;
}
scoped_refptr<InputMethodManager::State> MockInputMethodManager::CreateNewState(
    Profile* profile) {
  return nullptr;
}

scoped_refptr<InputMethodManager::State>
MockInputMethodManager::GetActiveIMEState() {
  return nullptr;
}

void MockInputMethodManager::SetState(
    scoped_refptr<InputMethodManager::State> state) {}

void MockInputMethodManager::ImeMenuActivationChanged(bool is_active) {}

void MockInputMethodManager::NotifyImeMenuItemsChanged(
    const std::string& engine_id,
    const std::vector<InputMethodManager::MenuItem>& items) {}

void MockInputMethodManager::MaybeNotifyImeMenuActivationChanged() {}

void MockInputMethodManager::OverrideKeyboardUrlRef(const std::string& keyset) {
}

bool MockInputMethodManager::IsEmojiHandwritingVoiceOnImeMenuEnabled() {
  return true;
}

}  // namespace input_method
}  // namespace chromeos
