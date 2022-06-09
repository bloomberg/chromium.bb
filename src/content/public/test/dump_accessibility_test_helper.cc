// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/dump_accessibility_test_helper.h"

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "content/public/common/content_switches.h"
#include "ui/accessibility/accessibility_switches.h"
#include "ui/accessibility/platform/inspect/ax_api_type.h"
#include "ui/accessibility/platform/inspect/ax_inspect_scenario.h"
#include "ui/base/buildflags.h"

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#endif
#if BUILDFLAG(USE_ATK)
extern "C" {
#include <atk/atk.h>
}
#endif

namespace content {

using base::FilePath;
using ui::AXNodeFilter;
using ui::AXPropertyFilter;

namespace {
const char kCommentToken = '#';
const char kMarkSkipFile[] = "#<skip";
const char kSignalDiff[] = "*";
const char kMarkEndOfFile[] = "<-- End-of-file -->";

using SetUpCommandLine = void (*)(base::CommandLine*);

struct TypeInfo {
  std::string type;
  struct Mapping {
    std::string directive_prefix;
    base::FilePath::StringType expectations_file_postfix;
    SetUpCommandLine setup_command_line;
  } mapping;
};

const TypeInfo kTypeInfos[] = {
    {
        "android",
        {
            "@ANDROID-",
            FILE_PATH_LITERAL("-android"),
            [](base::CommandLine*) {},
        },
    },
    {
        "blink",
        {
            "@BLINK-",
            FILE_PATH_LITERAL("-blink"),
            [](base::CommandLine*) {},
        },
    },
    {
        "fuchsia",
        {
            "@FUCHSIA-",
            FILE_PATH_LITERAL("-fuchsia"),
            [](base::CommandLine*) {},
        },
    },
    {
        "linux",
        {
            "@AURALINUX-",
            FILE_PATH_LITERAL("-auralinux"),
            [](base::CommandLine*) {},
        },
    },
    {
        "mac",
        {
            "@MAC-",
            FILE_PATH_LITERAL("-mac"),
            [](base::CommandLine*) {},
        },
    },
    {
        "content",
        {
            "@",
            FILE_PATH_LITERAL(""),
            [](base::CommandLine*) {},
        },
    },
    {
        "uia",
        {
            "@UIA-WIN-",
            FILE_PATH_LITERAL("-uia-win"),
            [](base::CommandLine* command_line) {
#if defined(OS_WIN)
              command_line->AppendSwitch(
                  ::switches::kEnableExperimentalUIAutomation);
#endif
            },
        },
    },
    {
        "ia2",
        {
            "@WIN-",
            FILE_PATH_LITERAL("-win"),
            [](base::CommandLine* command_line) {
#if defined(OS_WIN)
              command_line->RemoveSwitch(
                  ::switches::kEnableExperimentalUIAutomation);
#endif
            },
        },
    }};

const TypeInfo::Mapping* TypeMapping(const std::string& type) {
  const TypeInfo::Mapping* mapping = nullptr;
  for (const auto& info : kTypeInfos) {
    if (info.type == type) {
      mapping = &info.mapping;
    }
  }
  CHECK(mapping) << "Unknown dump accessibility type " << type;
  return mapping;
}

}  // namespace

DumpAccessibilityTestHelper::DumpAccessibilityTestHelper(
    ui::AXApiType::Type type)
    : expectation_type_(type) {}

DumpAccessibilityTestHelper::DumpAccessibilityTestHelper(
    const char* expectation_type)
    : expectation_type_(expectation_type) {}

base::FilePath DumpAccessibilityTestHelper::GetExpectationFilePath(
    const base::FilePath& test_file_path,
    const base::FilePath::StringType& expectations_qualifier) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::FilePath expected_file_path;

  // Try to get version specific expected file.
  base::FilePath::StringType expected_file_suffix =
      GetVersionSpecificExpectedFileSuffix(expectations_qualifier);
  if (expected_file_suffix != FILE_PATH_LITERAL("")) {
    expected_file_path = base::FilePath(
        test_file_path.RemoveExtension().value() + expected_file_suffix);
    if (base::PathExists(expected_file_path))
      return expected_file_path;
  }

  // If a version specific file does not exist, get the generic one.
  expected_file_suffix = GetExpectedFileSuffix(expectations_qualifier);
  expected_file_path = base::FilePath(test_file_path.RemoveExtension().value() +
                                      expected_file_suffix);
  if (base::PathExists(expected_file_path))
    return expected_file_path;

  // If no expected file could be found, display error.
  LOG(INFO) << "File not found: " << expected_file_path.LossyDisplayName();
  LOG(INFO) << "To run this test, create "
            << expected_file_path.LossyDisplayName()
            << " (it can be empty) and then run this test "
            << "with the switch: --"
            << switches::kGenerateAccessibilityTestExpectations;
  return base::FilePath();
}

void DumpAccessibilityTestHelper::SetUpCommandLine(
    base::CommandLine* command_line) const {
  const TypeInfo::Mapping* mapping = TypeMapping(expectation_type_);
  if (mapping) {
    mapping->setup_command_line(command_line);
  }
}

ui::AXInspectScenario DumpAccessibilityTestHelper::ParseScenario(
    const std::vector<std::string>& lines,
    const std::vector<ui::AXPropertyFilter>& default_filters) {
  const TypeInfo::Mapping* mapping = TypeMapping(expectation_type_);
  if (!mapping)
    return ui::AXInspectScenario();
  return ui::AXInspectScenario::From(mapping->directive_prefix, lines,
                                     default_filters);
}

absl::optional<ui::AXInspectScenario>
DumpAccessibilityTestHelper::ParseScenario(
    const base::FilePath& scenario_path,
    const std::vector<ui::AXPropertyFilter>& default_filters) {
  const TypeInfo::Mapping* mapping = TypeMapping(expectation_type_);
  if (!mapping)
    return ui::AXInspectScenario();
  return ui::AXInspectScenario::From(mapping->directive_prefix, scenario_path,
                                     default_filters);
}

// static
std::vector<ui::AXApiType::Type> DumpAccessibilityTestHelper::TreeTestPasses() {
  return
#if !BUILDFLAG(HAS_PLATFORM_ACCESSIBILITY_SUPPORT)
      {ui::AXApiType::kBlink};
#elif defined(OS_WIN)
      {ui::AXApiType::kBlink, ui::AXApiType::kWinIA2, ui::AXApiType::kWinUIA};
#elif defined(OS_MAC)
      {ui::AXApiType::kBlink, ui::AXApiType::kMac};
#elif defined(OS_ANDROID)
      {ui::AXApiType::kAndroid};
#elif defined(OS_FUCHSIA)
      {ui::AXApiType::kFuchsia};
#else  // linux
      {ui::AXApiType::kBlink, ui::AXApiType::kLinux};
#endif
}

// static
std::vector<ui::AXApiType::Type>
DumpAccessibilityTestHelper::EventTestPasses() {
  return
#if defined(OS_WIN)
      {ui::AXApiType::kWinIA2, ui::AXApiType::kWinUIA};
#elif defined(OS_MAC)
      {ui::AXApiType::kMac};
#elif BUILDFLAG(USE_ATK)
      {ui::AXApiType::kLinux};
#else
      {};
#endif
}

// static
absl::optional<std::vector<std::string>>
DumpAccessibilityTestHelper::LoadExpectationFile(
    const base::FilePath& expected_file) {
  base::ScopedAllowBlockingForTesting allow_blocking;

  std::string expected_contents_raw;
  base::ReadFileToString(expected_file, &expected_contents_raw);

  // Tolerate Windows-style line endings (\r\n) in the expected file:
  // normalize by deleting all \r from the file (if any) to leave only \n.
  std::string expected_contents;
  base::RemoveChars(expected_contents_raw, "\r", &expected_contents);

  if (!expected_contents.compare(0, strlen(kMarkSkipFile), kMarkSkipFile)) {
    return absl::nullopt;
  }

  std::vector<std::string> expected_lines =
      base::SplitString(expected_contents, "\n", base::KEEP_WHITESPACE,
                        base::SPLIT_WANT_NONEMPTY);

  return expected_lines;
}

// static
bool DumpAccessibilityTestHelper::ValidateAgainstExpectation(
    const base::FilePath& test_file_path,
    const base::FilePath& expected_file,
    const std::vector<std::string>& actual_lines,
    const std::vector<std::string>& expected_lines) {
  // Output the test path to help anyone who encounters a failure and needs
  // to know where to look.
  LOG(INFO) << "Testing: "
            << test_file_path.NormalizePathSeparatorsTo('/').LossyDisplayName();
  LOG(INFO) << "Expected output: "
            << expected_file.NormalizePathSeparatorsTo('/').LossyDisplayName();

  // Perform a diff (or write the initial baseline).
  std::vector<int> diff_lines = DiffLines(expected_lines, actual_lines);
  bool is_different = diff_lines.size() > 0;
  if (is_different) {
    std::string diff;

    // Mark the expected lines which did not match actual output with a *.
    diff += "* Line Expected\n";
    diff += "- ---- --------\n";
    for (int line = 0, diff_index = 0;
         line < static_cast<int>(expected_lines.size()); ++line) {
      bool is_diff = false;
      if (diff_index < static_cast<int>(diff_lines.size()) &&
          diff_lines[diff_index] == line) {
        is_diff = true;
        ++diff_index;
      }
      diff += base::StringPrintf("%1s %4d %s\n", is_diff ? kSignalDiff : "",
                                 line + 1, expected_lines[line].c_str());
    }
    diff += "\nActual\n";
    diff += "------\n";
    diff += base::JoinString(actual_lines, "\n");
    diff += "\n";

    // This is used by rebase_dump_accessibility_tree_test.py to signify
    // the end of the file when parsing the actual output from remote logs.
    diff += kMarkEndOfFile;
    diff += "\n";
    LOG(ERROR) << "Diff:\n" << diff;
  } else {
    LOG(INFO) << "Test output matches expectations.";
  }

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kGenerateAccessibilityTestExpectations)) {
    base::ScopedAllowBlockingForTesting allow_blocking;
    std::string actual_contents_for_output =
        base::JoinString(actual_lines, "\n") + "\n";
    CHECK(base::WriteFile(expected_file, actual_contents_for_output));
    LOG(INFO) << "Wrote expectations to: " << expected_file.LossyDisplayName();
#if defined(OS_ANDROID)
    LOG(INFO) << "Generated expectations written to file on test device.";
    LOG(INFO) << "To fetch, run: adb pull " << expected_file.LossyDisplayName();
#endif
  }

  return !is_different;
}

FilePath::StringType DumpAccessibilityTestHelper::GetExpectedFileSuffix(
    const base::FilePath::StringType& expectations_qualifier) const {
  const TypeInfo::Mapping* mapping = TypeMapping(expectation_type_);
  if (!mapping) {
    return FILE_PATH_LITERAL("");
  }

  FilePath::StringType suffix;
  if (!expectations_qualifier.empty())
    suffix = FILE_PATH_LITERAL("-") + expectations_qualifier;

  return suffix + FILE_PATH_LITERAL("-expected") +
         mapping->expectations_file_postfix + FILE_PATH_LITERAL(".txt");
}

FilePath::StringType
DumpAccessibilityTestHelper::GetVersionSpecificExpectedFileSuffix(
    const base::FilePath::StringType& expectations_qualifier) const {
#if defined(OS_WIN)
  if (expectation_type_ == "uia" &&
      base::win::GetVersion() == base::win::Version::WIN7) {
    FilePath::StringType suffix;
    if (!expectations_qualifier.empty())
      suffix = FILE_PATH_LITERAL("-") + expectations_qualifier;
    return suffix + FILE_PATH_LITERAL("-expected-uia-win7.txt");
  }
#endif
#if BUILDFLAG(USE_ATK)
  if (expectation_type_ == "linux") {
    FilePath::StringType version_name;
    switch (atk_get_minor_version()) {
      case 10:
        version_name = FILE_PATH_LITERAL("trusty");
        break;
      case 18:
        version_name = FILE_PATH_LITERAL("xenial");
        break;
      default:
        return FILE_PATH_LITERAL("");
    }

    FilePath::StringType suffix;
    if (!expectations_qualifier.empty())
      suffix = FILE_PATH_LITERAL("-") + expectations_qualifier;
    return suffix + FILE_PATH_LITERAL("-expected-auralinux-") + version_name +
           FILE_PATH_LITERAL(".txt");
  }
#endif
#if defined(OS_CHROMEOS)
  if (expectation_type_ == "blink") {
    FilePath::StringType suffix;
    if (!expectations_qualifier.empty())
      suffix = FILE_PATH_LITERAL("-") + expectations_qualifier;
    return suffix + FILE_PATH_LITERAL("-expected-blink-cros.txt");
  }
#endif
  return FILE_PATH_LITERAL("");
}

std::vector<int> DumpAccessibilityTestHelper::DiffLines(
    const std::vector<std::string>& expected_lines,
    const std::vector<std::string>& actual_lines) {
  int actual_lines_count = actual_lines.size();
  int expected_lines_count = expected_lines.size();
  std::vector<int> diff_lines;
  int i = 0, j = 0;
  while (i < actual_lines_count && j < expected_lines_count) {
    if (expected_lines[j].size() == 0 ||
        expected_lines[j][0] == kCommentToken) {
      // Skip comment lines and blank lines in expected output.
      ++j;
      continue;
    }

    if (actual_lines[i] != expected_lines[j])
      diff_lines.push_back(j);
    ++i;
    ++j;
  }

  // Report a failure if there are additional expected lines or
  // actual lines.
  if (i < actual_lines_count) {
    diff_lines.push_back(j);
  } else {
    while (j < expected_lines_count) {
      if (expected_lines[j].size() > 0 &&
          expected_lines[j][0] != kCommentToken) {
        diff_lines.push_back(j);
      }
      j++;
    }
  }

  // Actual file has been fully checked.
  return diff_lines;
}

}  // namespace content
