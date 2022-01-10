// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIGNIN_SYNC_CONFIRMATION_UI_H_
#define CHROME_BROWSER_UI_WEBUI_SIGNIN_SYNC_CONFIRMATION_UI_H_

#include <memory>
#include <string>
#include <unordered_map>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/webui/signin/signin_web_dialog_ui.h"

class Browser;
class Profile;

namespace content {
class WebUIDataSource;
}

namespace ui {
class WebUI;
}

// WebUI controller for the sync confirmation dialog.
//
// Note: This controller does not set the WebUI message handler. It is
// the responsability of the caller to pass the correct message handler.
class SyncConfirmationUI : public SigninWebDialogUI {
 public:
  enum class DesignVersion { kMonotone, kColored };

  explicit SyncConfirmationUI(content::WebUI* web_ui);

  SyncConfirmationUI(const SyncConfirmationUI&) = delete;
  SyncConfirmationUI& operator=(const SyncConfirmationUI&) = delete;

  ~SyncConfirmationUI() override;

  // SigninWebDialogUI:
  void InitializeMessageHandlerWithBrowser(Browser* browser) override;

  // Initializes the message handler for the profile creation flow (when there's
  // no browser available).
  void InitializeMessageHandlerForCreationFlow(
      absl::optional<SkColor> profile_color);

 private:
  void Initialize(absl::optional<SkColor> profile_creation_flow_color,
                  DesignVersion design,
                  bool is_modal_dialog);
  void InitializeMessageHandler(Browser* browser);
  void InitializeForSyncConfirmation(
      content::WebUIDataSource* source,
      absl::optional<SkColor> profile_creation_flow_color,
      DesignVersion design,
      bool is_modal_dialog);
  void InitializeForSyncDisabled(content::WebUIDataSource* source);

  // Adds a string resource with the given GRD |ids| to the WebUI data |source|
  // named as |name|. Also stores a reverse mapping from the localized version
  // of the string to the |ids| in order to later pass it to
  // SyncConfirmationHandler.
  void AddStringResource(content::WebUIDataSource* source,
                         const std::string& name,
                         int ids);

  // For consent auditing.
  std::unordered_map<std::string, int> js_localized_string_to_ids_map_;

  const raw_ptr<Profile> profile_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_SIGNIN_SYNC_CONFIRMATION_UI_H_
