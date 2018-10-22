// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_WEBSTORE_WIDGET_PRIVATE_APP_INSTALLER_H_
#define CHROME_BROWSER_EXTENSIONS_API_WEBSTORE_WIDGET_PRIVATE_APP_INSTALLER_H_

#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/extensions/webstore_standalone_installer.h"

namespace content {
class WebContents;
}  // namespace content

namespace webstore_widget {

// This class is used for installing apps and extensions suggested from the
// Chrome Web Store Gallery widget.
class AppInstaller : public extensions::WebstoreStandaloneInstaller {
 public:
  AppInstaller(content::WebContents* web_contents,
               const std::string& item_id,
               Profile* profile,
               bool silent_installation,
               const Callback& callback);

 protected:
  friend class base::RefCountedThreadSafe<AppInstaller>;

  ~AppInstaller() override;

  void OnWebContentsDestroyed(content::WebContents* web_contents);

  // WebstoreStandaloneInstaller implementation.
  bool CheckRequestorAlive() const override;
  const GURL& GetRequestorURL() const override;
  bool ShouldShowPostInstallUI() const override;
  bool ShouldShowAppInstalledBubble() const override;
  content::WebContents* GetWebContents() const override;
  std::unique_ptr<ExtensionInstallPrompt::Prompt> CreateInstallPrompt()
      const override;
  bool CheckInlineInstallPermitted(const base::DictionaryValue& webstore_data,
                                   std::string* error) const override;
  bool CheckRequestorPermitted(const base::DictionaryValue& webstore_data,
                               std::string* error) const override;

 private:
  class WebContentsObserver;

  bool silent_installation_;
  Callback callback_;
  content::WebContents* web_contents_;
  std::unique_ptr<WebContentsObserver> web_contents_observer_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(AppInstaller);
};

}  // namespace webstore_widget

#endif  // CHROME_BROWSER_EXTENSIONS_API_WEBSTORE_WIDGET_PRIVATE_APP_INSTALLER_H_
