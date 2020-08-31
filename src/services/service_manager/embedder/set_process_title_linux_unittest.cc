// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string.h>
#include <unistd.h>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/strings/string_util.h"
#include "services/service_manager/embedder/set_process_title_linux.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const std::string kNullChr(1, '\0');

std::string ReadCmdline() {
  std::string cmdline;
  CHECK(base::ReadFileToString(base::FilePath("/proc/self/cmdline"), &cmdline));
  // The process title appears in different formats depending on Linux kernel
  // version:
  // "title"            (on Linux --4.17)
  // "title\0\0\0...\0" (on Linux 4.18--5.2)
  // "title\0"          (on Linux 5.3--)
  //
  // For unit tests, just trim trailing null characters to support all cases.
  return base::TrimString(cmdline, kNullChr, base::TRIM_TRAILING).as_string();
}

TEST(SetProcTitleLinuxTest, Simple) {
  setproctitle("a %s cat", "cute");
  EXPECT_TRUE(base::EndsWith(ReadCmdline(), " a cute cat",
                             base::CompareCase::SENSITIVE))
      << ReadCmdline();

  setproctitle("-a %s cat", "cute");
  EXPECT_EQ(ReadCmdline(), "a cute cat");
}

TEST(SetProcTitleLinuxTest, Empty) {
  setproctitle("-");
  EXPECT_EQ(ReadCmdline(), "");
}

TEST(SetProcTitleLinuxTest, Long) {
  setproctitle("-long cat is l%0100000dng", 0);
  EXPECT_TRUE(base::StartsWith(ReadCmdline(), "long cat is l00000000",
                               base::CompareCase::SENSITIVE))
      << ReadCmdline();
}

}  // namespace
