// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_TEST_ASSISTANT_TEST_API_H_
#define ASH_PUBLIC_CPP_TEST_ASSISTANT_TEST_API_H_

#include <memory>
#include <string>

#include "ash/ash_export.h"
#include "chromeos/services/assistant/public/cpp/assistant_prefs.h"

namespace aura {
class Window;
}  // namespace aura

namespace views {
class Textfield;
class View;
}  // namespace views

namespace ash {

class AssistantState;

// Public test API for the Assistant UI.
// This API works both for ash and browser tests.
class ASH_EXPORT AssistantTestApi {
 public:
  static std::unique_ptr<AssistantTestApi> Create();

  AssistantTestApi() = default;
  virtual ~AssistantTestApi() = default;

  virtual void DisableAnimations() = 0;

  // Returns true if the Assistant UI is visible.
  virtual bool IsVisible() = 0;

  // Sends a text query. This requires the UI to be ready to accept text
  // queries, i.e. the input text field must be visible and focussed.
  virtual void SendTextQuery(const std::string& query) = 0;

  // Enables/Disables Assistant in settings.
  // This will ensure the new value is propagated to the |AssistantState|.
  virtual void SetAssistantEnabled(bool enabled) = 0;

  // Enables/Disables Assistant in settings.
  // This will ensure the new value is propagated to the |AssistantState|.
  virtual void SetScreenContextEnabled(bool enabled) = 0;

  virtual void SetTabletMode(bool enable) = 0;

  // Changes the user preference controlling the status of user consent.
  virtual void SetConsentStatus(chromeos::assistant::prefs::ConsentStatus) = 0;

  // Changes the user setting controlling whether the user prefers voice or
  // keyboard (internally called |kAssistantLaunchWithMicOpen|).
  // This will ensure the new value is propagated to the |AssistantState|.
  virtual void SetPreferVoice(bool value) = 0;

  virtual void StartOverview() = 0;

  // Returns the interface to get/set Assistant related prefs and states.
  virtual AssistantState* GetAssistantState() = 0;

  // Wait for all Assistant related actions to settle.
  virtual void WaitUntilIdle() = 0;

  // Returns the top-level Assistant specific view.
  // Can only be used after the Assistant UI has been shown at least once.
  virtual views::View* page_view() = 0;

  // Returns the Assistant main view.
  // Can only be used after the Assistant UI has been shown at least once.
  virtual views::View* main_view() = 0;

  // Returns the Assistant UI element container view, which contains all the
  // responses.
  // Can only be used after the Assistant UI has been shown at least once.
  virtual views::View* ui_element_container() = 0;

  // Returns the text field used for inputting new queries.
  // Can only be used after the Assistant UI has been shown at least once.
  virtual views::Textfield* input_text_field() = 0;

  // Returns the mic field used for dictating new queries.
  // Can only be used after the Assistant UI has been shown at least once.
  virtual views::View* mic_view() = 0;

  // Returns the greeting label shown when the Assistant is displayed.
  // Can only be used after the Assistant UI has been shown at least once.
  virtual views::View* greeting_label() = 0;

  // Returns the button to enable voice mode.
  // Can only be used after the Assistant UI has been shown at least once.
  virtual views::View* voice_input_toggle() = 0;

  // Returns the button to enable text mode.
  // Can only be used after the Assistant UI has been shown at least once.
  virtual views::View* keyboard_input_toggle() = 0;

  // Returns the button to launch Assistant onboarding.
  // Can only be used after the Assistant UI has been shown at least once.
  virtual views::View* opt_in_view() = 0;

  // Returns the view containing the suggestion chips.
  // Can only be used after the Assistant UI has been shown at least once.
  virtual views::View* suggestion_chip_container() = 0;

  // Returns the window containing the Assistant UI.
  // Note that this window is shared for all components of the |AppList|.
  // Can only be used after the Assistant UI has been shown at least once.
  virtual aura::Window* window() = 0;

  // Returns the app list view hosting the Assistant UI.
  // Can only be used after the Assistant UI has been shown at least once.
  virtual views::View* app_list_view() = 0;

  // Returns the root window containing the Assistant UI (and the Ash shell).
  // This can be used even when the Assistant UI has never been shown.
  virtual aura::Window* root_window() = 0;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_TEST_ASSISTANT_TEST_API_H_
