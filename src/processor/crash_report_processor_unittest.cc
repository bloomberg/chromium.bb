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

// Unit test for CrashReporter.  Uses a pre-generated minidump and
// corresponding symbol file, and checks the contents of the CrashReport.

#include <string>
#include "google/crash_report.h"
#include "google/crash_report_processor.h"
#include "google/symbol_supplier.h"
#include "processor/minidump.h"

using std::string;
using google_airbag::CrashReportProcessor;
using google_airbag::CrashReport;

#define ASSERT_TRUE(cond) \
  if (!(cond)) {                                                        \
    fprintf(stderr, "FAILED: %s at %s:%d\n", #cond, __FILE__, __LINE__); \
    return false; \
  }

#define ASSERT_EQ(e1, e2) ASSERT_TRUE((e1) == (e2))

namespace google_airbag {

class TestSymbolSupplier : public SymbolSupplier {
 public:
  virtual string GetSymbolFile(MinidumpModule *module,
                               const CrashReport *report);
};

string TestSymbolSupplier::GetSymbolFile(MinidumpModule *module,
                                         const CrashReport *report) {
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
  CrashReportProcessor processor(&supplier);

  CrashReport report;
  string minidump_file = string(getenv("srcdir") ? getenv("srcdir") : ".") +
                         "/src/processor/testdata/minidump2.dmp";

  ASSERT_TRUE(processor.ProcessReport(&report, minidump_file));
  ASSERT_EQ(report.stack_frames.size(), 4);

  ASSERT_EQ(report.stack_frames[0].module_base, 0x400000);
  ASSERT_EQ(report.stack_frames[0].module_name, "c:\\test_app.exe");
  ASSERT_EQ(report.stack_frames[0].function_name, "CrashFunction");
  ASSERT_EQ(report.stack_frames[0].source_file_name, "c:\\test_app.cc");
  ASSERT_EQ(report.stack_frames[0].source_line, 36);

  ASSERT_EQ(report.stack_frames[1].module_base, 0x400000);
  ASSERT_EQ(report.stack_frames[1].module_name, "c:\\test_app.exe");
  ASSERT_EQ(report.stack_frames[1].function_name, "main");
  ASSERT_EQ(report.stack_frames[1].source_file_name, "c:\\test_app.cc");
  ASSERT_EQ(report.stack_frames[1].source_line, 42);

  // This comes from the CRT
  ASSERT_EQ(report.stack_frames[2].module_base, 0x400000);
  ASSERT_EQ(report.stack_frames[2].module_name, "c:\\test_app.exe");
  ASSERT_EQ(report.stack_frames[2].function_name, "__tmainCRTStartup");
  ASSERT_EQ(report.stack_frames[2].source_file_name,
            "f:\\rtm\\vctools\\crt_bld\\self_x86\\crt\\src\\crt0.c");
  ASSERT_EQ(report.stack_frames[2].source_line, 318);

  // No debug info available for kernel32.dll
  ASSERT_EQ(report.stack_frames[3].module_base, 0x7c800000);
  ASSERT_EQ(report.stack_frames[3].module_name,
            "C:\\WINDOWS\\system32\\kernel32.dll");
  ASSERT_TRUE(report.stack_frames[3].function_name.empty());
  ASSERT_TRUE(report.stack_frames[3].source_file_name.empty());
  ASSERT_EQ(report.stack_frames[3].source_line, 0);

  return true;
}

int main(int argc, char *argv[]) {
  if (!RunTests()) {
    return 1;
  }

  return 0;
}
