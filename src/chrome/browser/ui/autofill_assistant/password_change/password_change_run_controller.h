// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_ASSISTANT_PASSWORD_CHANGE_PASSWORD_CHANGE_RUN_CONTROLLER_H_
#define CHROME_BROWSER_UI_AUTOFILL_ASSISTANT_PASSWORD_CHANGE_PASSWORD_CHANGE_RUN_CONTROLLER_H_

#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/autofill_assistant/password_change/proto/extensions.pb.h"

class PasswordChangeRunDisplay;

// Abstract interface for a controller of an `PasswordChangeRunDisplay`.
class PasswordChangeRunController {
 public:
  // Defines the current UI state to restore pre-interrupt UI.
  // Interrupts are not triggered during prompts, therefore
  // there is no need to keep their state.
  struct Model {
    std::u16string title;
    autofill_assistant::password_change::TopIcon top_icon;
    std::u16string description;
    autofill_assistant::password_change::ProgressStep progress_step;
  };

  // Factory function to create the controller.
  static std::unique_ptr<PasswordChangeRunController> Create();

  virtual ~PasswordChangeRunController() = default;

  // Shows the |PasswordChangeRunDisplay|.
  virtual void Show(
      base::WeakPtr<PasswordChangeRunDisplay> password_change_run_display) = 0;

  // The below methods are used to set UI.
  // They all persist state to a model owned by the controller and call the
  // sibling view methods.
  virtual void SetTopIcon(
      autofill_assistant::password_change::TopIcon top_icon) = 0;
  virtual void SetTitle(const std::u16string& title) = 0;
  virtual void SetDescription(const std::u16string& description) = 0;
  virtual void SetProgressBarStep(
      autofill_assistant::password_change::ProgressStep progress_step) = 0;
  virtual void ShowBasePrompt(
      const autofill_assistant::password_change::BasePrompt& base_prompt) = 0;
  virtual void OnBasePromptOptionSelected(int option_index) = 0;
  virtual void ShowSuggestedPasswordPrompt(
      const std::u16string& suggested_password) = 0;
  virtual void OnSuggestedPasswordSelected(bool selected) = 0;

  // Returns a weak pointer to this controller.
  virtual base::WeakPtr<PasswordChangeRunController> GetWeakPtr() = 0;

 protected:
  PasswordChangeRunController() = default;
};

#endif  // CHROME_BROWSER_UI_AUTOFILL_ASSISTANT_PASSWORD_CHANGE_PASSWORD_CHANGE_RUN_CONTROLLER_H_
