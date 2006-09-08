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

#ifndef GOOGLE_CRASH_REPORT_PROCESSOR_H__
#define GOOGLE_CRASH_REPORT_PROCESSOR_H__

#include <string>

namespace google_airbag {

using std::string;

struct CrashReport;
class SymbolSupplier;

class CrashReportProcessor {
 public:
  // Initializes this CrashReportProcessor.  supplier should be an
  // implementation of the SymbolSupplier abstract base class.
  CrashReportProcessor(SymbolSupplier *supplier);
  ~CrashReportProcessor();

  // Fills in the stack and other derived data for the CrashReport,
  // using the supplied minidump file.  Returns true on success.
  bool ProcessReport(CrashReport *report, const string &minidump_file);

 private:
  SymbolSupplier *supplier_;
};

}  // namespace google_airbag

#endif  // GOOGLE_CRASH_REPORT_PROCESSOR_H__
