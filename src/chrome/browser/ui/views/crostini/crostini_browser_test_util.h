// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_CROSTINI_CROSTINI_BROWSER_TEST_UTIL_H_
#define CHROME_BROWSER_UI_VIEWS_CROSTINI_CROSTINI_BROWSER_TEST_UTIL_H_

#include "base/files/scoped_temp_dir.h"
#include "base/test/scoped_path_override.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "net/base/network_change_notifier.h"

class ChromeBrowserMainExtraPartsNetFactoryInstaller;

namespace chromeos {
class FakeImageLoaderClient;
}  // namespace chromeos

// Common base for Crostini dialog broswer tests. Allows tests to set network
// connection type.
class CrostiniDialogBrowserTest : public DialogBrowserTest {
 public:
  CrostiniDialogBrowserTest();

  // BrowserTestBase:
  void CreatedBrowserMainParts(
      content::BrowserMainParts* browser_main_parts) override;
  void SetUp() override;
  void SetUpOnMainThread() override;
  virtual void InitCrosTermina();

  void SetConnectionType(
      net::NetworkChangeNotifier::ConnectionType connection_type);

  void UnregisterTermina();

 protected:
  // Owned by content::Browser
  ChromeBrowserMainExtraPartsNetFactoryInstaller* extra_parts_ = nullptr;
  // Image loader client injected into, and owned by DBusThreadManager.
  chromeos::FakeImageLoaderClient* image_loader_client_ = nullptr;
  base::ScopedPathOverride dir_component_user_override_;
  base::ScopedTempDir cros_termina_resources_;

 private:
  DISALLOW_COPY_AND_ASSIGN(CrostiniDialogBrowserTest);
};

#endif  // CHROME_BROWSER_UI_VIEWS_CROSTINI_CROSTINI_BROWSER_TEST_UTIL_H_
