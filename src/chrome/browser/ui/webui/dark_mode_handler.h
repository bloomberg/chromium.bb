// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_DARK_MODE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_DARK_MODE_HANDLER_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/scoped_observer.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "ui/native_theme/dark_mode_observer.h"
#include "ui/native_theme/native_theme.h"

namespace base {
class ListValue;
}  // namespace base

namespace content {
class WebUI;
class WebUIDataSource;
}  // namespace content

namespace ui {
class NativeTheme;
}  // namespace ui

class Profile;

class DarkModeHandler : public content::WebUIMessageHandler {
 public:
  ~DarkModeHandler() override;

  // Sets load-time constants on |source|. This handles a flicker-free initial
  // page load (i.e. $i18n{dark}, loadTimeData.getBoolean('darkMode')). Adds a
  // DarkModeHandler to |web_ui| if WebUI dark mode enhancements are enabled.
  // If enabled, continually updates |source| by name if dark mode changes. This
  // is so page refreshes will have fresh, correct data. Neither |web_ui| nor
  // |source| may be nullptr.
  static void Initialize(content::WebUI* web_ui,
                         content::WebUIDataSource* source);

 protected:
  // Protected for testing.
  static void InitializeInternal(content::WebUI* web_ui,
                                 content::WebUIDataSource* source,
                                 ui::NativeTheme* theme,
                                 Profile* profile);

  explicit DarkModeHandler(ui::NativeTheme* theme, Profile* profile);

 private:
  // content::WebUIMessageHandler:
  void RegisterMessages() override;

  // Handles the "observeDarkMode" message. No arguments. Protected for testing.
  void HandleObserveDarkMode(const base::ListValue* args);

  bool UseDarkMode() const;

  // Generates a dictionary with "dark" i18n key based on |using_dark_|. Called
  // in Initialize() and on each change for notifications.
  std::unique_ptr<base::DictionaryValue> GetDataSourceUpdate() const;

  void OnDarkModeChanged(bool dark_mode);

  // Profile to update data sources on. Injected for testing.
  Profile* const profile_;

  ui::DarkModeObserver dark_mode_observer_;

  // Populated if feature is enabled.
  std::string source_name_;

  DISALLOW_COPY_AND_ASSIGN(DarkModeHandler);
};

#endif  // CHROME_BROWSER_UI_WEBUI_DARK_MODE_HANDLER_H_
