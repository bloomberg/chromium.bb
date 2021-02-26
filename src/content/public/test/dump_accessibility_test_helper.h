// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_DUMP_ACCESSIBILITY_TEST_HELPER_H_
#define CONTENT_PUBLIC_TEST_DUMP_ACCESSIBILITY_TEST_HELPER_H_

#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/optional.h"

namespace base {
class FilePath;
}

namespace ui {
struct AXNodeFilter;
struct AXPropertyFilter;
}  // namespace ui

namespace content {

// A helper class for writing accessibility tree dump tests.
class DumpAccessibilityTestHelper {
 public:
  explicit DumpAccessibilityTestHelper(const char* expectation_type);
  ~DumpAccessibilityTestHelper() = default;

  // Returns a path to an expectation file for the current platform. If no
  // suitable expectation file can be found, logs an error message and returns
  // an empty path.
  base::FilePath GetExpectationFilePath(const base::FilePath& test_file_path);

  // Parses property filter directive, if the line is a valid property filter
  // directive, then a new property filter is created and appneded to the list,
  // true is returned, otherwise false.
  bool ParsePropertyFilter(const std::string& line,
                           std::vector<ui::AXPropertyFilter>* filters) const;

  // Parses node filter directive, if the line is a valid node filter directive
  // then a new node filter is created and appneded to the list, true is
  // returned, otherwise false.
  bool ParseNodeFilter(const std::string& line,
                       std::vector<ui::AXNodeFilter>* filters) const;

  struct Directive {
    enum Type {
      // No directive.
      kNone,

      // Instructs to not wait for document load for url defined by the
      // directive.
      kNoLoadExpected,

      // Delays a test unitl a string defined by the directive is present
      // in the dump.
      kWaitFor,

      // Delays a test until a string returned by a script defined by the
      // directive is present in the dump.
      kExecuteAndWaitFor,

      // Indicates event recording should continue at least until a specific
      // event has been received.
      kRunUntil,

      // Invokes default action on an accessible object defined by the
      // directive.
      kDefaultActionOn,
    } type;

    std::string value;
  };

  // Parses directives from the given line.
  Directive ParseDirective(const std::string& line) const;

  // Loads the given expectation file and returns the contents. An expectation
  // file may be empty, in which case an empty vector is returned.
  // Returns nullopt if the file contains a skip marker.
  static base::Optional<std::vector<std::string>> LoadExpectationFile(
      const base::FilePath& expected_file);

  // Compares the given actual dump against the given expectation and generates
  // a new expectation file if switches::kGenerateAccessibilityTestExpectations
  // has been set. Returns true if the result matches the expectation.
  static bool ValidateAgainstExpectation(
      const base::FilePath& test_file_path,
      const base::FilePath& expected_file,
      const std::vector<std::string>& actual_lines,
      const std::vector<std::string>& expected_lines);

 private:
  // Suffix of the expectation file corresponding to html file.
  // Overridden by each platform subclass.
  // Example:
  // HTML test:      test-file.html
  // Expected:       test-file-expected-mac.txt.
  base::FilePath::StringType GetExpectedFileSuffix() const;

  // Some Platforms expect different outputs depending on the version.
  // Most test outputs are identical but this allows a version specific
  // expected file to be used.
  base::FilePath::StringType GetVersionSpecificExpectedFileSuffix() const;

  FRIEND_TEST_ALL_PREFIXES(DumpAccessibilityTestHelperTest, TestDiffLines);

  // Utility helper that does a comment-aware equality check.
  // Returns array of lines from expected file which are different.
  static std::vector<int> DiffLines(
      const std::vector<std::string>& expected_lines,
      const std::vector<std::string>& actual_lines);

  std::string expectation_type_;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_DUMP_ACCESSIBILITY_TEST_HELPER_H_
