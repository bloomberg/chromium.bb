/*
 * Copyright (C) 2020 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "perfetto/base/build_config.h"
#include "test/gtest_and_gmock.h"

// This translation unit is built only on Linux and MacOS. See //gn/BUILD.gn.
#if PERFETTO_BUILDFLAG(PERFETTO_LOCAL_SYMBOLIZER)

#include "src/profiling/symbolizer/local_symbolizer.h"

namespace perfetto {
namespace profiling {
namespace {

TEST(LocalSymbolizerTest, ParseLineWindows) {
  std::string file_name;
  uint32_t lineno;
  ASSERT_TRUE(
      ParseLlvmSymbolizerLine("C:\\Foo\\Bar.cc:123:1", &file_name, &lineno));
  EXPECT_EQ(file_name, "C:\\Foo\\Bar.cc");
  EXPECT_EQ(lineno, 123u);
}

}  // namespace
}  // namespace profiling
}  // namespace perfetto

#endif
