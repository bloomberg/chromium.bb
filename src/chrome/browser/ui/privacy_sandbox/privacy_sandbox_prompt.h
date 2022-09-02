// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PRIVACY_SANDBOX_PRIVACY_SANDBOX_PROMPT_H_
#define CHROME_BROWSER_UI_PRIVACY_SANDBOX_PRIVACY_SANDBOX_PROMPT_H_

#include "chrome/browser/privacy_sandbox/privacy_sandbox_service.h"

class Browser;

// Creates and shows a dialog or bubble for |browser| displaying the Privacy
// Sandbox notice or consent to the user.
void ShowPrivacySandboxPrompt(Browser* browser,
                              PrivacySandboxService::PromptType prompt_type);

// Creates and shows a dialog for |browser| displaying the Privacy Sandbox
// notice or consent to the user. Specific implementations are responsible for
// altering the content as appropriate based on |prompt_type|.
void ShowPrivacySandboxDialog(Browser* browser,
                              PrivacySandboxService::PromptType prompt_type);

// Creates and shows a bubble for |browser| displaying the Privacy Sandbox
// notice the user.
void ShowPrivacySandboxNoticeBubble(Browser* browser);

#endif  // CHROME_BROWSER_UI_PRIVACY_SANDBOX_PRIVACY_SANDBOX_PROMPT_H_
