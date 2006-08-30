// Copyright (C) 2006 Google Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <stdio.h>
#include <string>
#include "source_line_resolver.h"

using std::string;
using google_airbag::SourceLineResolver;

#define ASSERT_TRUE(cond) \
  if (!(cond)) {                                                        \
    fprintf(stderr, "FAILED: %s at %s:%d\n", #cond, __FILE__, __LINE__); \
    return false; \
  }

#define ASSERT_FALSE(cond) ASSERT_TRUE(!(cond))

#define ASSERT_EQ(e1, e2) ASSERT_TRUE((e1) == (e2))

static bool VerifyEmpty(const SourceLineResolver::SourceLineInfo &info) {
  ASSERT_TRUE(info.function_name.empty());
  ASSERT_TRUE(info.source_file.empty());
  ASSERT_EQ(info.source_line, 0);
  return true;
}

static bool RunTests() {
  string testdata_dir = string(getenv("srcdir") ? getenv("srcdir") : ".") +
                        "/src/processor/testdata";

  SourceLineResolver resolver;
  ASSERT_TRUE(resolver.LoadModule("module1", testdata_dir + "/module1.out"));
  ASSERT_TRUE(resolver.LoadModule("module2", testdata_dir + "/module2.out"));

  SourceLineResolver::SourceLineInfo info;
  resolver.LookupAddress(0x1000, "module1", &info);
  ASSERT_EQ(info.function_name, "Function1_1");
  ASSERT_EQ(info.source_file, "file1_1.cc");
  ASSERT_EQ(info.source_line, 44);

  info.Reset();
  ASSERT_TRUE(VerifyEmpty(info));

  resolver.LookupAddress(0x800, "module1", &info);
  ASSERT_TRUE(VerifyEmpty(info));

  resolver.LookupAddress(0x1280, "module1", &info);
  ASSERT_EQ(info.function_name, "Function1_3");
  ASSERT_TRUE(info.source_file.empty());
  ASSERT_EQ(info.source_line, 0);

  resolver.LookupAddress(0x1380, "module1", &info);
  ASSERT_EQ(info.function_name, "Function1_4");
  ASSERT_TRUE(info.source_file.empty());
  ASSERT_EQ(info.source_line, 0);

  resolver.LookupAddress(0x2180, "module2", &info);
  ASSERT_EQ(info.function_name, "Function2_2");
  ASSERT_EQ(info.source_file, "file2_2.cc");
  ASSERT_EQ(info.source_line, 21);

  ASSERT_FALSE(resolver.LoadModule("module3",
                                   testdata_dir + "/module3_bad.out"));
  ASSERT_FALSE(resolver.LoadModule("module4",
                                   testdata_dir + "/invalid-filename"));
  return true;
}

int main(int argc, char **argv) {
  if (!RunTests()) {
    return 1;
  }
  return 0;
}
