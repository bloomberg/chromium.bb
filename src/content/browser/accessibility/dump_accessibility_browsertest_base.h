// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ACCESSIBILITY_DUMP_ACCESSIBILITY_BROWSERTEST_BASE_H_
#define CONTENT_BROWSER_ACCESSIBILITY_DUMP_ACCESSIBILITY_BROWSERTEST_BASE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/files/file_util.h"
#include "base/test/scoped_feature_list.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/ax_inspect_factory.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/accessibility_notification_waiter.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/dump_accessibility_test_helper.h"
#include "third_party/blink/public/common/features.h"
#include "ui/accessibility/platform/inspect/ax_api_type.h"
#include "ui/accessibility/platform/inspect/ax_inspect_scenario.h"

namespace content {

class BrowserAccessibility;
class BrowserAccessibilityManager;
class DumpAccessibilityTestHelper;

// Base class for an accessibility browsertest that takes an HTML file as
// input, loads it into a tab, dumps some accessibility data in text format,
// then compares that text to an expectation file in the same directory.
//
// The system was inspired by WebKit/Blink LayoutTests, but customized for
// testing accessibility in Chromium.
//
// See content/test/data/accessibility/readme.md for an overview.
class DumpAccessibilityTestBase
    : public ContentBrowserTest,
      public ::testing::WithParamInterface<ui::AXApiType::Type> {
 public:
  DumpAccessibilityTestBase();
  ~DumpAccessibilityTestBase() override;

  // Given a path to an HTML file relative to the test directory,
  // loads the HTML, loads the accessibility tree, calls Dump(), then
  // compares the output to the expected result and has the test succeed
  // or fail based on the diff.
  void RunTest(const base::FilePath file_path,
               const char* file_dir,
               const base::FilePath::StringType& expectations_qualifier =
                   FILE_PATH_LITERAL(""));

  template <const char* type>
  void RunTypedTest(const base::FilePath::CharType* file_path) {
    base::FilePath test_path = GetTestFilePath("accessibility", type);
    {
      base::ScopedAllowBlockingForTesting allow_blocking;
      ASSERT_TRUE(base::PathExists(test_path)) << test_path.LossyDisplayName();
    }
    base::FilePath test_file = test_path.Append(base::FilePath(file_path));

    std::string dir(std::string() + "accessibility/" + type);
    RunTest(test_file, dir.c_str());
  }

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override;
  void SetUpOnMainThread() override;
  void SetUp() override;

  //
  // For subclasses to override:
  //

  // This is called by RunTest after the document has finished loading,
  // including the load complete accessibility event. The subclass should
  // dump whatever that specific test wants to dump, returning the result
  // as a sequence of strings.
  virtual std::vector<std::string> Dump() = 0;

  // Add the default filters that are applied to all tests.
  virtual std::vector<ui::AXPropertyFilter> DefaultFilters() const = 0;

  // This gets called if the diff didn't match; the test can print
  // additional useful info.
  virtual void OnDiffFailed() {}

  // Choose which feature flags to enable or disable.
  virtual void ChooseFeatures(std::vector<base::Feature>* enabled_features,
                              std::vector<base::Feature>* disabled_features);

  //
  // Helpers
  //

  // Dump the whole accessibility tree, without applying any filters,
  // and return it as a string.
  std::string DumpUnfilteredAccessibilityTreeAsString();

  void RunTestForPlatform(const base::FilePath file_path,
                          const char* file_dir,
                          const base::FilePath::StringType&
                              expectations_qualifier = FILE_PATH_LITERAL(""));

  // Retrieve the accessibility node that matches the accessibility name. There
  // is an optional search_root parameter that defaults to the document root if
  // not provided.
  BrowserAccessibility* FindNode(
      const std::string& name,
      BrowserAccessibility* search_root = nullptr) const;

  // Retrieve the browser accessibility manager object for the current web
  // contents.
  BrowserAccessibilityManager* GetManager() const;

  std::unique_ptr<ui::AXTreeFormatter> CreateFormatter() const;

  // Returns a list of captured events fired after the invoked action.
  using InvokeAction = base::OnceCallback<base::Value()>;
  std::pair<base::Value, std::vector<std::string>> CaptureEvents(
      InvokeAction invoke_action);

  // Test scenario loaded from the test file.
  ui::AXInspectScenario scenario_;

  // Whether we should enable accessibility after navigating to the page,
  // otherwise we enable it first.
  bool enable_accessibility_after_navigating_;

  base::test::ScopedFeatureList scoped_feature_list_;

  bool HasHtmlAttribute(BrowserAccessibility& node,
                        const char* attr,
                        const std::string& value) const;

  BrowserAccessibility* FindNodeByHTMLAttribute(const char* attr,
                                                const std::string& value) const;

 protected:
  DumpAccessibilityTestHelper test_helper_;

  WebContentsImpl* GetWebContents() const;

  // Wait until all accessibility events and dirty objects have been processed.
  void WaitForEndOfTest() const;

  // Perform any requested default actions and wait until a notification is
  // received that each action is performed.
  void PerformAndWaitForDefaultActions();

  // Support the @WAIT-FOR directive (node, tree tests only).
  void WaitForExpectedText();

  // Wait for default action, expected text and then end of test signal.
  void WaitForFinalTreeContents();

 private:
  BrowserAccessibility* FindNodeInSubtree(BrowserAccessibility& node,
                                          const std::string& name) const;

  BrowserAccessibility* FindNodeByHTMLAttributeInSubtree(
      BrowserAccessibility& node,
      const char* attr,
      const std::string& value) const;

  std::vector<std::string> CollectAllFrameUrls(
      const std::vector<std::string>& skip_urls);

  // Wait until all initial content is completely loaded, included within
  // subframes, objects and portals.
  void WaitForAllFramesLoaded();

  void OnEventRecorded(const std::string& event) const {
    LOG(INFO) << "++ Platform event: " << event;
  }

  bool has_performed_default_actions_ = false;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ACCESSIBILITY_DUMP_ACCESSIBILITY_BROWSERTEST_BASE_H_
