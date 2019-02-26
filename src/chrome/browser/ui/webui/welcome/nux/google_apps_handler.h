// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_WELCOME_NUX_GOOGLE_APPS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_WELCOME_NUX_GOOGLE_APPS_HANDLER_H_

#include "base/macros.h"
#include "base/values.h"
#include "content/public/browser/web_ui_message_handler.h"

namespace nux {

extern const char* kGoogleAppsInteractionHistogram;

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class GoogleAppsInteraction {
  kPromptShown = 0,
  kNoThanks = 1,
  kGetStarted = 2,
  kCount,
};

class GoogleAppsHandler : public content::WebUIMessageHandler {
 public:
  GoogleAppsHandler();
  ~GoogleAppsHandler() override;

  // WebUIMessageHandler:
  void RegisterMessages() override;

  // Callbacks for JS APIs.
  void HandleCacheGoogleAppIcon(const base::ListValue* args);
  void HandleGetGoogleAppsList(const base::ListValue* args);

  DISALLOW_COPY_AND_ASSIGN(GoogleAppsHandler);
};

}  // namespace nux

#endif  // CHROME_BROWSER_UI_WEBUI_WELCOME_NUX_GOOGLE_APPS_HANDLER_H_
