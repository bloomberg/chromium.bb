// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/assistant/assistant_test_api_impl.h"

#include "ash/app_list/app_list_controller_impl.h"
#include "ash/app_list/views/app_list_main_view.h"
#include "ash/app_list/views/app_list_page.h"
#include "ash/app_list/views/app_list_view.h"
#include "ash/app_list/views/contents_view.h"
#include "ash/assistant/ui/assistant_view_ids.h"
#include "ash/public/cpp/assistant/assistant_state.h"
#include "ash/public/cpp/tablet_mode.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/wm/overview/overview_controller.h"
#include "base/bind.h"
#include "components/prefs/pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/controls/textfield/textfield.h"

namespace ash {

std::unique_ptr<AssistantTestApi> AssistantTestApi::Create() {
  return std::make_unique<AssistantTestApiImpl>();
}

AssistantTestApiImpl::AssistantTestApiImpl() = default;

AssistantTestApiImpl::~AssistantTestApiImpl() {
  EnableAnimations();
}

void AssistantTestApiImpl::DisableAnimations() {
  AppListView::SetShortAnimationForTesting(true);

  scoped_animation_duration_ =
      std::make_unique<ui::ScopedAnimationDurationScaleMode>(
          ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);
}

bool AssistantTestApiImpl::IsVisible() {
  return AppListViewsHaveBeenCreated() && page_view()->GetVisible();
}

void AssistantTestApiImpl::SendTextQuery(const std::string& query) {
  if (!input_text_field()->HasFocus()) {
    ADD_FAILURE()
        << "The TextField should be focussed before we can send a query";
  }

  input_text_field()->SetText(base::UTF8ToUTF16(query));
  // Send <return> to commit the query.
  SendKeyPress(ui::KeyboardCode::VKEY_RETURN);
}

views::View* AssistantTestApiImpl::page_view() {
  const int index = contents_view()->GetPageIndexForState(
      AppListState::kStateEmbeddedAssistant);
  return static_cast<views::View*>(contents_view()->GetPageView(index));
}

views::View* AssistantTestApiImpl::main_view() {
  return page_view()->GetViewByID(AssistantViewID::kMainView);
}

views::View* AssistantTestApiImpl::ui_element_container() {
  return page_view()->GetViewByID(AssistantViewID::kUiElementContainer);
}

views::Textfield* AssistantTestApiImpl::input_text_field() {
  return static_cast<views::Textfield*>(
      page_view()->GetViewByID(AssistantViewID::kTextQueryField));
}

views::View* AssistantTestApiImpl::mic_view() {
  return page_view()->GetViewByID(AssistantViewID::kMicView);
}

views::View* AssistantTestApiImpl::greeting_label() {
  return page_view()->GetViewByID(AssistantViewID::kGreetingLabel);
}

views::View* AssistantTestApiImpl::voice_input_toggle() {
  return page_view()->GetViewByID(AssistantViewID::kVoiceInputToggle);
}

views::View* AssistantTestApiImpl::keyboard_input_toggle() {
  return page_view()->GetViewByID(AssistantViewID::kKeyboardInputToggle);
}

views::View* AssistantTestApiImpl::suggestion_chip_container() {
  return page_view()->GetViewByID(AssistantViewID::kSuggestionContainer);
}

views::View* AssistantTestApiImpl::opt_in_view() {
  return page_view()->GetViewByID(AssistantViewID::kOptInView);
}

aura::Window* AssistantTestApiImpl::window() {
  return main_view()->GetWidget()->GetNativeWindow();
}

views::View* AssistantTestApiImpl::app_list_view() {
  return static_cast<views::View*>(contents_view()->app_list_view());
}

aura::Window* AssistantTestApiImpl::root_window() {
  return Shell::Get()->GetPrimaryRootWindow();
}

void AssistantTestApiImpl::SetAssistantEnabled(bool enabled) {
  Shell::Get()->session_controller()->GetPrimaryUserPrefService()->SetBoolean(
      chromeos::assistant::prefs::kAssistantEnabled, enabled);

  // Ensure the value has taken effect.
  ASSERT_EQ(GetAssistantState()->settings_enabled(), enabled)
      << "Changing this preference did not take effect immediately, which will "
         "cause timing issues in this test. If this trace is seen we must add "
         "a waiter here to wait for the new state to take effect.";
}

void AssistantTestApiImpl::SetScreenContextEnabled(bool enabled) {
  Shell::Get()->session_controller()->GetPrimaryUserPrefService()->SetBoolean(
      chromeos::assistant::prefs::kAssistantContextEnabled, enabled);

  // Ensure the value has taken effect.
  ASSERT_EQ(GetAssistantState()->context_enabled(), enabled)
      << "Changing this preference did not take effect immediately, which will "
         "cause timing issues in this test. If this trace is seen we must add "
         "a waiter here to wait for the new state to take effect.";
}

void AssistantTestApiImpl::SetTabletMode(bool enable) {
  TabletMode::Get()->SetEnabledForTest(enable);
}

void AssistantTestApiImpl::StartOverview() {
  Shell::Get()->overview_controller()->StartOverview();
}

void AssistantTestApiImpl::SetConsentStatus(
    chromeos::assistant::prefs::ConsentStatus consent_status) {
  Shell::Get()->session_controller()->GetPrimaryUserPrefService()->SetInteger(
      chromeos::assistant::prefs::kAssistantConsentStatus, consent_status);

  // Ensure the value has taken effect.
  ASSERT_EQ(GetAssistantState()->consent_status(), consent_status)
      << "Changing this preference did not take effect immediately, which will "
         "cause timing issues in this test. If this trace is seen we must add "
         "a waiter here to wait for the new state to take effect.";
}

void AssistantTestApiImpl::SetPreferVoice(bool value) {
  Shell::Get()->session_controller()->GetPrimaryUserPrefService()->SetBoolean(
      chromeos::assistant::prefs::kAssistantLaunchWithMicOpen, value);

  // Ensure the value has taken effect.
  ASSERT_EQ(GetAssistantState()->launch_with_mic_open(), value)
      << "Changing this preference did not take effect immediately, which will "
         "cause timing issues in this test. If this trace is seen we must add "
         "a waiter here to wait for the new state to take effect.";
}

AssistantState* AssistantTestApiImpl::GetAssistantState() {
  return AssistantState::Get();
}

void AssistantTestApiImpl::WaitUntilIdle() {
  base::RunLoop().RunUntilIdle();
}

void AssistantTestApiImpl::EnableAnimations() {
  scoped_animation_duration_ = nullptr;
  AppListView::SetShortAnimationForTesting(false);
}

bool AssistantTestApiImpl::AppListViewsHaveBeenCreated() const {
  return contents_view_or_null() != nullptr;
}

ContentsView* AssistantTestApiImpl::contents_view() {
  ContentsView* result = contents_view_or_null();
  DCHECK(result) << "App list has not been initialized yet. "
                    "Be sure to display the Assistant UI first.";
  return result;
}

ContentsView* AssistantTestApiImpl::contents_view_or_null() const {
  auto* app_list_view =
      Shell::Get()->app_list_controller()->presenter()->GetView();

  if (!app_list_view)
    return nullptr;

  return app_list_view->app_list_main_view()->contents_view();
}

void AssistantTestApiImpl::SendKeyPress(ui::KeyboardCode key) {
  ui::test::EventGenerator event_generator(root_window());
  event_generator.PressKey(key, /*flags=*/ui::EF_NONE);
}

}  // namespace ash
