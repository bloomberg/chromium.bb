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

#include "google/crash_report.h"
#include "google/crash_report_processor.h"
#include "processor/minidump.h"
#include "processor/stackwalker_x86.h"

namespace google_airbag {

CrashReportProcessor::CrashReportProcessor(SymbolSupplier *supplier)
    : supplier_(supplier) {
}

CrashReportProcessor::~CrashReportProcessor() {
}

bool CrashReportProcessor::ProcessReport(CrashReport *report,
                                         const string &minidump_file) {
  Minidump dump(minidump_file);
  if (!dump.Read()) {
    return false;
  }

  MinidumpException *exception = dump.GetException();
  if (!exception) {
    return false;
  }

  MinidumpThreadList *threads = dump.GetThreadList();
  if (!threads) {
    return false;
  }

  // TODO(bryner): get all the threads
  MinidumpThread *thread = threads->GetThreadByID(exception->GetThreadID());
  if (!thread) {
    return false;
  }

  MinidumpMemoryRegion *thread_memory = thread->GetMemory();
  if (!thread_memory) {
    return false;
  }

  // TODO(bryner): figure out which StackWalker we want
  StackwalkerX86 walker(exception->GetContext(), thread_memory,
                        dump.GetModuleList(), supplier_, report);
  walker.Walk(&report->stack_frames);
  return true;
}

}  // namespace google_airbag
