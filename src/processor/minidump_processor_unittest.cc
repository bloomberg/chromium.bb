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

// Unit test for MinidumpProcessor.  Uses a pre-generated minidump and
// corresponding symbol file, and checks the stack frames for correctness.

#include <string>
#include "google/minidump_processor.h"
#include "google/symbol_supplier.h"
#include "processor/minidump.h"

using std::string;
using google_airbag::MinidumpProcessor;
using google_airbag::StackFrames;

#define ASSERT_TRUE(cond) \
  if (!(cond)) {                                                        \
    fprintf(stderr, "FAILED: %s at %s:%d\n", #cond, __FILE__, __LINE__); \
    return false; \
  }

#define ASSERT_EQ(e1, e2) ASSERT_TRUE((e1) == (e2))

namespace google_airbag {

class TestSymbolSupplier : public SymbolSupplier {
 public:
  TestSymbolSupplier() : has_supplier_data_(false) {}
  virtual ~TestSymbolSupplier() {}

  virtual string GetSymbolFile(MinidumpModule *module, void *supplier_data);

  // This member is used to test the data argument to GetSymbolFile.
  // If the argument is correct, it's set to true.
  bool has_supplier_data_;
};

string TestSymbolSupplier::GetSymbolFile(MinidumpModule *module,
                                         void *supplier_data) {
  if (supplier_data == &has_supplier_data_) {
    has_supplier_data_ = true;
  }

  if (*(module->GetName()) == "c:\\test_app.exe") {
    return string(getenv("srcdir") ? getenv("srcdir") : ".") +
      "/src/processor/testdata/minidump2.sym";
  }

  return "";
}

}  // namespace google_airbag

using google_airbag::TestSymbolSupplier;

static bool RunTests() {

  TestSymbolSupplier supplier;
  MinidumpProcessor processor(&supplier);

  StackFrames stack_frames;
  string minidump_file = string(getenv("srcdir") ? getenv("srcdir") : ".") +
                         "/src/processor/testdata/minidump2.dmp";

  ASSERT_TRUE(processor.Process(minidump_file,
                                &supplier.has_supplier_data_, &stack_frames));
  ASSERT_TRUE(supplier.has_supplier_data_);
  ASSERT_EQ(stack_frames.size(), 4);

  ASSERT_EQ(stack_frames[0].module_base, 0x400000);
  ASSERT_EQ(stack_frames[0].module_name, "c:\\test_app.exe");
  ASSERT_EQ(stack_frames[0].function_name, "CrashFunction");
  ASSERT_EQ(stack_frames[0].source_file_name, "c:\\test_app.cc");
  ASSERT_EQ(stack_frames[0].source_line, 36);

  ASSERT_EQ(stack_frames[1].module_base, 0x400000);
  ASSERT_EQ(stack_frames[1].module_name, "c:\\test_app.exe");
  ASSERT_EQ(stack_frames[1].function_name, "main");
  ASSERT_EQ(stack_frames[1].source_file_name, "c:\\test_app.cc");
  ASSERT_EQ(stack_frames[1].source_line, 42);

  // This comes from the CRT
  ASSERT_EQ(stack_frames[2].module_base, 0x400000);
  ASSERT_EQ(stack_frames[2].module_name, "c:\\test_app.exe");
  ASSERT_EQ(stack_frames[2].function_name, "__tmainCRTStartup");
  ASSERT_EQ(stack_frames[2].source_file_name,
            "f:\\rtm\\vctools\\crt_bld\\self_x86\\crt\\src\\crt0.c");
  ASSERT_EQ(stack_frames[2].source_line, 318);

  // No debug info available for kernel32.dll
  ASSERT_EQ(stack_frames[3].module_base, 0x7c800000);
  ASSERT_EQ(stack_frames[3].module_name,
            "C:\\WINDOWS\\system32\\kernel32.dll");
  ASSERT_TRUE(stack_frames[3].function_name.empty());
  ASSERT_TRUE(stack_frames[3].source_file_name.empty());
  ASSERT_EQ(stack_frames[3].source_line, 0);

  return true;
}

int main(int argc, char *argv[]) {
  if (!RunTests()) {
    return 1;
  }

  return 0;
}
