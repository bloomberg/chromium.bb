// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_TOOLBAR_UNITTEST_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_TOOLBAR_UNITTEST_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_container.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/test_with_browser_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "content/public/test/web_contents_tester.h"
#include "extensions/common/extension.h"

namespace extensions {
class ExtensionService;
}  // namespace extensions

// Base class for unit tests that use the toolbar area. This is used for unit
// tests that are generally related to the ExtensionsToolbarContainer in the
// ToolbarView area (such as ExtensionsToolbarControls and
// ExtensionsTabbedMenuView).
// When possible, prefer creating a unit test with browser view instead of a
// interactive ui or browser test since they are faster and less flaky.
class ExtensionsToolbarUnitTest : public TestWithBrowserView {
 public:
  ExtensionsToolbarUnitTest() = default;
  ~ExtensionsToolbarUnitTest() override = default;
  ExtensionsToolbarUnitTest(const ExtensionsToolbarUnitTest&) = delete;
  const ExtensionsToolbarUnitTest& operator=(const ExtensionsToolbarUnitTest&) =
      delete;

  extensions::ExtensionService* extension_service() {
    return extension_service_;
  }

  ExtensionsToolbarContainer* extensions_container() {
    return browser_view()->toolbar()->extensions_container();
  }

  // Adds the specified `extension` with no host permissions.
  scoped_refptr<const extensions::Extension> InstallExtension(
      const std::string& name);

  // Adds the specified `extension` with the given `host_permissions`.
  scoped_refptr<const extensions::Extension>
  InstallExtensionWithHostPermissions(
      const std::string& name,
      const std::vector<std::string>& host_permissions);

  // Reloads the extension of the given `extension_id`.
  void ReloadExtension(const extensions::ExtensionId& extension_id);

  // Uninstalls the extensions of the given `extension_id`.
  void UninstallExtension(const extensions::ExtensionId& extension_id);

  // Enables the extension of the given `extension_id`.
  void EnableExtension(const extensions::ExtensionId& extension_id);

  // Disables the extension of the given `extension_id`.
  void DisableExtension(const extensions::ExtensionId& extension_id);

  // Triggers the press and release event of the given `button`.
  void ClickButton(views::Button* button) const;

  // Returns a list of the views of the currently pinned extensions, in order
  // from left to right.
  std::vector<ToolbarActionView*> GetPinnedExtensionViews();

  // Returns a list of the names of the currently pinned extensions, in order
  // from left to right.
  std::vector<std::string> GetPinnedExtensionNames();

  // Waits for the extensions container to animate (on pin, unpin, pop-out,
  // etc.)
  void WaitForAnimation();

  // Since this is a unittest, the ExtensionsToolbarContainer sometimes needs a
  // nudge to re-layout the views.
  void LayoutContainerIfNecessary();

  // Adds a new tab to the tab strip, and returns the WebContentsTester
  // associated with it.
  content::WebContentsTester* AddWebContentsAndGetTester();

  // TestWithBrowserView:
  void SetUp() override;

 private:
  raw_ptr<extensions::ExtensionService> extension_service_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_TOOLBAR_UNITTEST_H_
