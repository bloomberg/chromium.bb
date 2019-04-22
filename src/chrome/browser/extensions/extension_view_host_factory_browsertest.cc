// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_view_host_factory.h"

#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_view_host.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "extensions/common/view_type.h"

namespace extensions {

typedef ExtensionBrowserTest ExtensionViewHostFactoryTest;

// Tests that ExtensionHosts are created with the correct type and profiles.
IN_PROC_BROWSER_TEST_F(ExtensionViewHostFactoryTest, CreateExtensionHosts) {
  // Load a very simple extension with just a background page.
  scoped_refptr<const Extension> extension =
      LoadExtension(test_data_dir_.AppendASCII("api_test")
                        .AppendASCII("browser_action")
                        .AppendASCII("none"));
  ASSERT_TRUE(extension.get());

  content::BrowserContext* browser_context = browser()->profile();
  {
    // Popup hosts are created with the correct type and profile.
    std::unique_ptr<ExtensionViewHost> host =
        ExtensionViewHostFactory::CreatePopupHost(extension->url(), browser());
    EXPECT_EQ(extension.get(), host->extension());
    EXPECT_EQ(browser_context, host->browser_context());
    EXPECT_EQ(VIEW_TYPE_EXTENSION_POPUP, host->extension_host_type());
    EXPECT_TRUE(host->view());
  }

  {
    // Dialog hosts are created with the correct type and profile.
    std::unique_ptr<ExtensionViewHost> host =
        ExtensionViewHostFactory::CreateDialogHost(extension->url(),
                                                   browser()->profile());
    EXPECT_EQ(extension.get(), host->extension());
    EXPECT_EQ(browser_context, host->browser_context());
    EXPECT_EQ(VIEW_TYPE_EXTENSION_DIALOG, host->extension_host_type());
    EXPECT_TRUE(host->view());
  }
}

}  // namespace extensions
