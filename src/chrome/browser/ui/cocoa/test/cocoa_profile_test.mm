// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/test/cocoa_profile_test.h"

#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/autocomplete/autocomplete_classifier_factory.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "content/public/test/test_browser_thread_bundle.h"

CocoaProfileTest::CocoaProfileTest()
    : profile_manager_(TestingBrowserProcess::GetGlobal()),
      profile_(NULL),
      thread_bundle_(new content::TestBrowserThreadBundle) {}

CocoaProfileTest::~CocoaProfileTest() {
  // Delete the testing profile on the UI thread. But first release the
  // browser, since it may trigger accesses to the profile upon destruction.
  browser_.reset();

  base::RunLoop().RunUntilIdle();

  // Some services created on the TestingProfile require deletion on the UI
  // thread. If the scoper in TestingBrowserProcess, owned by ChromeTestSuite,
  // were to delete the ProfileManager, the UI thread would at that point no
  // longer exist.
  TestingBrowserProcess::GetGlobal()->SetProfileManager(NULL);

  // Make sure any pending tasks run before we destroy other threads.
  base::RunLoop().RunUntilIdle();
}

void CocoaProfileTest::AddTestingFactories(
    const TestingProfile::TestingFactories& testing_factories) {
  for (auto testing_factory : testing_factories) {
    testing_factories_.push_back(testing_factory);
  }
}

void CocoaProfileTest::SetUp() {
  CocoaTest::SetUp();

  ASSERT_TRUE(profile_manager_.SetUp());

  profile_ = profile_manager_.CreateTestingProfile(
      "Person 1", std::unique_ptr<sync_preferences::PrefServiceSyncable>(),
      base::UTF8ToUTF16("Person 1"), 0, std::string(), testing_factories_);
  ASSERT_TRUE(profile_);

  // TODO(shess): These are needed in case someone creates a browser
  // window off of browser_.  pkasting indicates that other
  // platforms use a stub |BrowserWindow| and thus don't need to do
  // this.
  // http://crbug.com/39725
  TemplateURLServiceFactory::GetInstance()->SetTestingFactoryAndUse(
      profile_, &TemplateURLServiceFactory::BuildInstanceFor);
  AutocompleteClassifierFactory::GetInstance()->SetTestingFactoryAndUse(
      profile_, &AutocompleteClassifierFactory::BuildInstanceFor);

  profile_->CreateBookmarkModel(true);
  bookmarks::test::WaitForBookmarkModelToLoad(
      BookmarkModelFactory::GetForBrowserContext(profile_));

  browser_.reset(CreateBrowser());
  ASSERT_TRUE(browser_.get());
}

void CocoaProfileTest::TearDown() {
  if (browser_.get() && browser_->window())
    CloseBrowserWindow();

  CocoaTest::TearDown();
}

void CocoaProfileTest::CloseBrowserWindow() {
  // Check to make sure a window was actually created.
  DCHECK(browser_->window());
  browser_->tab_strip_model()->CloseAllTabs();
  chrome::CloseWindow(browser_.get());
  // |browser_| will be deleted by its BrowserWindowController.
  ignore_result(browser_.release());
}

Browser* CocoaProfileTest::CreateBrowser() {
  return new Browser(Browser::CreateParams(profile(), true));
}
