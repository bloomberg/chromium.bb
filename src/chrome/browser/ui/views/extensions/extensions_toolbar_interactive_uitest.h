// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_TOOLBAR_INTERACTIVE_UITEST_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_TOOLBAR_INTERACTIVE_UITEST_H_

#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "extensions/common/extension.h"

class ExtensionsToolbarContainer;
class ToolbarActionView;

namespace extensions {
class Extension;
}

// Base class for interactive ui tests that use the toolbar area. This is used
// for browser test fixtures that are generally related to the
// ExtensionsToolbarContainer in the ToolbarView area. For example, this is used
// by ExtensionsToolbarContainer and ExtensionsMenuView separately to clarify
// what the suite is primarily trying to test.
class ExtensionsToolbarUITest : public DialogBrowserTest {
 public:
  ExtensionsToolbarUITest(const ExtensionsToolbarUITest&) = delete;
  ExtensionsToolbarUITest& operator=(const ExtensionsToolbarUITest&) = delete;

 protected:
  ExtensionsToolbarUITest();
  ~ExtensionsToolbarUITest() override;

  void SetUpOnMainThread() override;

  Profile* profile();

  Browser* incognito_browser() { return incognito_browser_; }

  const std::vector<scoped_refptr<const extensions::Extension>>& extensions() {
    return extensions_;
  }

  // Loads and returns a test extension from |chrome::DIR_TEST_DATA|.
  // |allow_incognito| is used to declare whether the extension is allowed to
  // run in incognito.
  scoped_refptr<const extensions::Extension> LoadTestExtension(
      const std::string& path,
      bool allow_incognito = false);

  // Adds |extension| to the back of |extensions_|.
  void AppendExtension(scoped_refptr<const extensions::Extension> extension);

  // Sets up |incognito_browser_|.
  void SetUpIncognitoBrowser();

  // Gets the extensions toolbar container from the browser() toolbar.
  ExtensionsToolbarContainer* GetExtensionsToolbarContainer() const;
  // Returns the extensions toolbar container for the given `browser`.
  ExtensionsToolbarContainer* GetExtensionsToolbarContainerForBrowser(
      Browser* browser) const;

  // Gets the ToolbarActionView instances inside
  // GetExtensionsToolbarContainer().
  std::vector<ToolbarActionView*> GetToolbarActionViews() const;
  // Returns the ToolbarActionView instances within the extensions toolbar for
  // the given `browser`.
  std::vector<ToolbarActionView*> GetToolbarActionViewsForBrowser(
      Browser* browser) const;

  // Gets only the visible ToolbarActionView instances from
  // GetToolbarActionViews().
  std::vector<ToolbarActionView*> GetVisibleToolbarActionViews() const;

 private:
  raw_ptr<Browser> incognito_browser_ = nullptr;
  std::vector<scoped_refptr<const extensions::Extension>> extensions_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_TOOLBAR_INTERACTIVE_UITEST_H_
