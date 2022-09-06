// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_ASSISTANT_PASSWORD_CHANGE_ASSISTANT_ONBOARDING_PROMPT_H_
#define CHROME_BROWSER_UI_AUTOFILL_ASSISTANT_PASSWORD_CHANGE_ASSISTANT_ONBOARDING_PROMPT_H_

#include "base/memory/weak_ptr.h"

class AssistantOnboardingController;

namespace content {
class WebContents;
}  // namespace content

// Abstract interface to describe the methods of an onboarding prompt view
// necessary for the controller to control it.
class AssistantOnboardingPrompt {
 public:
  // Factory function to create onboarding prompts on desktop platforms. The
  // actual implementation is in the `assistant_onboarding_view.cc` file.
  static base::WeakPtr<AssistantOnboardingPrompt> Create(
      base::WeakPtr<AssistantOnboardingController> controller);

  AssistantOnboardingPrompt() = default;
  virtual ~AssistantOnboardingPrompt() = default;

  // Shows the view of the prompt.
  virtual void Show(content::WebContents* web_contents) = 0;

  // Notifies that view that the controller was destroyed so that the view can
  // close.
  virtual void OnControllerGone() = 0;
};

#endif  // CHROME_BROWSER_UI_AUTOFILL_ASSISTANT_PASSWORD_CHANGE_ASSISTANT_ONBOARDING_PROMPT_H_
