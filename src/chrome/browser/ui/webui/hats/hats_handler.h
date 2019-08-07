// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_HATS_HATS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_HATS_HATS_HANDLER_H_

#include "base/macros.h"
#include "content/public/browser/web_ui_message_handler.h"

namespace base {
class ListValue;
}

// Used by UMA histogram, so entries shouldn't be reordered or removed.
enum class HappinessTrackingSurveyDesktopResponse {
  kShown = 0,
  kCompleted = 1,
  kDimsissed = 2,
  kMaxValue = kDimsissed,
};

// The handler for Javascript messages for the about:flags page.
class HatsHandler : public content::WebUIMessageHandler {
 public:
  HatsHandler();
  ~HatsHandler() override;

 private:
  // WebUIMessageHandler implementation.
  void RegisterMessages() override;

  // Callback for the "afterShow" message.
  void HandleAfterShow(const base::ListValue* args);

  DISALLOW_COPY_AND_ASSIGN(HatsHandler);
};

#endif  // CHROME_BROWSER_UI_WEBUI_HATS_HATS_HANDLER_H_
