// Copyright 2015 The Crashpad Authors. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "snapshot/win/pe_image_annotations_reader.h"

#include <stdlib.h>
#include <string.h>

#include <map>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "client/crashpad_info.h"
#include "client/simple_string_dictionary.h"
#include "gtest/gtest.h"
#include "snapshot/win/pe_image_reader.h"
#include "snapshot/win/process_reader_win.h"
#include "test/gtest_disabled.h"
#include "test/test_paths.h"
#include "test/win/child_launcher.h"
#include "util/file/file_io.h"
#include "util/win/process_info.h"

namespace crashpad {
namespace test {
namespace {

enum TestType {
  // Don't crash, just test the CrashpadInfo interface.
  kDontCrash = 0,

  // The child process should crash by __debugbreak().
  kCrashDebugBreak,
};

void TestAnnotationsOnCrash(TestType type, const base::FilePath& directory) {
  // Spawn a child process, passing it the pipe name to connect to.
  std::wstring child_test_executable = directory
                                           .Append(TestPaths::Executable()
                                                       .BaseName()
                                                       .RemoveFinalExtension()
                                                       .value() +
                                                   L"_simple_annotations.exe")
                                           .value();
  ChildLauncher child(child_test_executable, L"");
  ASSERT_NO_FATAL_FAILURE(child.Start());

  // Wait for the child process to indicate that it's done setting up its
  // annotations via the CrashpadInfo interface.
  char c;
  CheckedReadFileExactly(child.stdout_read_handle(), &c, sizeof(c));

  ProcessReaderWin process_reader;
  ASSERT_TRUE(process_reader.Initialize(child.process_handle(),
                                        ProcessSuspensionState::kRunning));

  // Verify the "simple map" annotations set via the CrashpadInfo interface.
  const std::vector<ProcessInfo::Module>& modules = process_reader.Modules();
  std::map<std::string, std::string> all_annotations_simple_map;
  for (const ProcessInfo::Module& module : modules) {
    PEImageReader pe_image_reader;
    pe_image_reader.Initialize(&process_reader,
                               module.dll_base,
                               module.size,
                               base::UTF16ToUTF8(module.name));
    PEImageAnnotationsReader module_annotations_reader(
        &process_reader, &pe_image_reader, module.name);
    std::map<std::string, std::string> module_annotations_simple_map =
        module_annotations_reader.SimpleMap();
    all_annotations_simple_map.insert(module_annotations_simple_map.begin(),
                                      module_annotations_simple_map.end());
  }

  EXPECT_GE(all_annotations_simple_map.size(), 5u);
  EXPECT_EQ(all_annotations_simple_map["#TEST# pad"], "crash");
  EXPECT_EQ(all_annotations_simple_map["#TEST# key"], "value");
  EXPECT_EQ(all_annotations_simple_map["#TEST# x"], "y");
  EXPECT_EQ(all_annotations_simple_map["#TEST# longer"], "shorter");
  EXPECT_EQ(all_annotations_simple_map["#TEST# empty_value"], "");

  // Tell the child process to continue.
  DWORD expected_exit_code;
  switch (type) {
    case kDontCrash:
      c = ' ';
      expected_exit_code = 0;
      break;
    case kCrashDebugBreak:
      c = 'd';
      expected_exit_code = STATUS_BREAKPOINT;
      break;
    default:
      FAIL();
  }
  CheckedWriteFile(child.stdin_write_handle(), &c, sizeof(c));

  EXPECT_EQ(child.WaitForExit(), expected_exit_code);
}

TEST(PEImageAnnotationsReader, DontCrash) {
  TestAnnotationsOnCrash(kDontCrash, TestPaths::Executable().DirName());
}

TEST(PEImageAnnotationsReader, CrashDebugBreak) {
  TestAnnotationsOnCrash(kCrashDebugBreak, TestPaths::Executable().DirName());
}

#if defined(ARCH_CPU_64_BITS)
TEST(PEImageAnnotationsReader, DontCrashWOW64) {
  base::FilePath output_32_bit_directory = TestPaths::Output32BitDirectory();
  if (output_32_bit_directory.empty()) {
    DISABLED_TEST();
  }

  TestAnnotationsOnCrash(kDontCrash, output_32_bit_directory);
}

TEST(PEImageAnnotationsReader, CrashDebugBreakWOW64) {
  base::FilePath output_32_bit_directory = TestPaths::Output32BitDirectory();
  if (output_32_bit_directory.empty()) {
    DISABLED_TEST();
  }

  TestAnnotationsOnCrash(kCrashDebugBreak, output_32_bit_directory);
}
#endif  // ARCH_CPU_64_BITS

}  // namespace
}  // namespace test
}  // namespace crashpad
