// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extensions_toolbar_interactive_uitest.h"

#include "base/containers/cxx20_erase.h"
#include "base/path_service.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_container.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/common/chrome_paths.h"
#include "net/dns/mock_host_resolver.h"
#include "ui/views/layout/animating_layout_manager_test_util.h"
#include "ui/views/view_utils.h"

ExtensionsToolbarUITest::ExtensionsToolbarUITest() = default;

ExtensionsToolbarUITest::~ExtensionsToolbarUITest() = default;

Profile* ExtensionsToolbarUITest::profile() {
  return browser()->profile();
}

scoped_refptr<const extensions::Extension>
ExtensionsToolbarUITest::LoadTestExtension(const std::string& path,
                                           bool allow_incognito) {
  extensions::ChromeTestExtensionLoader loader(profile());
  loader.set_allow_incognito_access(allow_incognito);
  base::FilePath test_data_dir;
  base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir);
  scoped_refptr<const extensions::Extension> extension =
      loader.LoadExtension(test_data_dir.AppendASCII(path));
  AppendExtension(extension);

  // Loading an extension can result in the container changing visibility.
  // Allow it to finish laying out appropriately.
  auto* container = GetExtensionsToolbarContainer();
  container->GetWidget()->LayoutRootViewIfNecessary();

  return extension;
}

void ExtensionsToolbarUITest::AppendExtension(
    scoped_refptr<const extensions::Extension> extension) {
  extensions_.push_back(std::move(extension));
}

void ExtensionsToolbarUITest::SetUpIncognitoBrowser() {
  incognito_browser_ = CreateIncognitoBrowser();
}

void ExtensionsToolbarUITest::SetUpOnMainThread() {
  DialogBrowserTest::SetUpOnMainThread();
  host_resolver()->AddRule("*", "127.0.0.1");
  views::test::ReduceAnimationDuration(GetExtensionsToolbarContainer());
}

ExtensionsToolbarContainer*
ExtensionsToolbarUITest::GetExtensionsToolbarContainer() const {
  return GetExtensionsToolbarContainerForBrowser(browser());
}

ExtensionsToolbarContainer*
ExtensionsToolbarUITest::GetExtensionsToolbarContainerForBrowser(
    Browser* browser) const {
  return BrowserView::GetBrowserViewForBrowser(browser)
      ->toolbar()
      ->extensions_container();
}

std::vector<ToolbarActionView*> ExtensionsToolbarUITest::GetToolbarActionViews()
    const {
  return GetToolbarActionViewsForBrowser(browser());
}

std::vector<ToolbarActionView*>
ExtensionsToolbarUITest::GetToolbarActionViewsForBrowser(
    Browser* browser) const {
  std::vector<ToolbarActionView*> views;
  for (auto* view :
       GetExtensionsToolbarContainerForBrowser(browser)->children()) {
    if (views::IsViewClass<ToolbarActionView>(view))
      views.push_back(static_cast<ToolbarActionView*>(view));
  }
  return views;
}

std::vector<ToolbarActionView*>
ExtensionsToolbarUITest::GetVisibleToolbarActionViews() const {
  auto views = GetToolbarActionViews();
  base::EraseIf(views, [](views::View* view) { return !view->GetVisible(); });
  return views;
}
