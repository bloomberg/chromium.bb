// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ACCESSIBILITY_DUMP_ACCESSIBILITY_TREE_BROWSERTEST_H_
#define CONTENT_BROWSER_ACCESSIBILITY_DUMP_ACCESSIBILITY_TREE_BROWSERTEST_H_

#include "content/browser/accessibility/dump_accessibility_browsertest_base.h"

#include "ui/accessibility/accessibility_features.h"
#include "ui/accessibility/accessibility_switches.h"

namespace content {

constexpr const char kARIA[]{"aria"};
constexpr const char kAOM[]{"aom"};
constexpr const char kCSS[]{"css"};
constexpr const char kHTML[]{"html"};
constexpr const char kMathML[]{"mathml"};
constexpr const char kDisplayLocking[]{"display-locking"};
constexpr const char kRegression[]{"regression"};
constexpr const char kTestHarness[]{"test-harness"};

// See content/test/data/accessibility/readme.md for an overview.
//
// This test takes a snapshot of the platform BrowserAccessibility tree and
// tests it against an expected baseline.
//
// The flow of the test is as outlined below.
// 1. Load an html file from content/test/data/accessibility.
// 2. Read the expectation.
// 3. Browse to the page, wait for the accessibility tree to load, optionally
//    wait for certain conditions (such as @WAIT-FOR) and serialize the platform
//    specific tree into a human readable string.
// 4. Perform a comparison between actual and expected and fail if they do not
//    exactly match.
class DumpAccessibilityTreeTest : public DumpAccessibilityTestBase {
 public:
  std::vector<ui::AXPropertyFilter> DefaultFilters() const override;

  void SetUpCommandLine(base::CommandLine* command_line) override;
  std::vector<std::string> Dump() override;

  void RunAccNameTest(const base::FilePath::CharType* file_path) {
    base::FilePath test_path = GetTestFilePath("accessibility", "accname");
    {
      base::ScopedAllowBlockingForTesting allow_blocking;
      ASSERT_TRUE(base::PathExists(test_path)) << test_path.LossyDisplayName();
    }
    base::FilePath accname_file = test_path.Append(base::FilePath(file_path));
    RunTest(accname_file, "accessibility/accname", FILE_PATH_LITERAL("tree"));
  }

  void RunAriaTest(const base::FilePath::CharType* file_path) {
    RunTypedTest<kARIA>(file_path);
  }

  void RunAomTest(const base::FilePath::CharType* file_path) {
    RunTypedTest<kAOM>(file_path);
  }

  void RunCSSTest(const base::FilePath::CharType* file_path) {
    RunTypedTest<kCSS>(file_path);
  }

  void RunHtmlTest(const base::FilePath::CharType* file_path) {
    RunTypedTest<kHTML>(file_path);
  }

  void RunMathMLTest(const base::FilePath::CharType* file_path) {
    RunTypedTest<kMathML>(file_path);
  }

  void RunDisplayLockingTest(const base::FilePath::CharType* file_path) {
    RunTypedTest<kDisplayLocking>(file_path);
  }

  void RunRegressionTest(const base::FilePath::CharType* file_path) {
    RunTypedTest<kRegression>(file_path);
  }

  void RunLanguageDetectionTest(const base::FilePath::CharType* file_path) {
    base::FilePath test_path =
        GetTestFilePath("accessibility", "language-detection");
    {
      base::ScopedAllowBlockingForTesting allow_blocking;
      ASSERT_TRUE(base::PathExists(test_path)) << test_path.LossyDisplayName();
    }
    base::FilePath language_detection_file =
        test_path.Append(base::FilePath(file_path));

    // Enable language detection for both static and dynamic content.
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        ::switches::kEnableExperimentalAccessibilityLanguageDetection);
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        ::switches::kEnableExperimentalAccessibilityLanguageDetectionDynamic);

    RunTest(language_detection_file, "accessibility/language-detection");
  }

  // Testing of the Test Harness itself.
  void RunTestHarnessTest(const base::FilePath::CharType* file_path) {
    RunTypedTest<kTestHarness>(file_path);
  }

 protected:
  // Override from DumpAccessibilityTestBase.
  void ChooseFeatures(std::vector<base::Feature>* enabled_features,
                      std::vector<base::Feature>* disabled_features) override;
};

// Subclass of DumpAccessibilityTreeTest that exposes ignored nodes.
class DumpAccessibilityTreeTestWithIgnoredNodes
    : public DumpAccessibilityTreeTest {
 protected:
  // Override from DumpAccessibilityTreeTest.
  void ChooseFeatures(std::vector<base::Feature>* enabled_features,
                      std::vector<base::Feature>* disabled_features) override;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ACCESSIBILITY_DUMP_ACCESSIBILITY_TREE_BROWSERTEST_H_
