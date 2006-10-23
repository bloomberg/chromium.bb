// Copyright (c) 2006, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "google/minidump_processor.h"
#include "processor/minidump.h"
#include "processor/scoped_ptr.h"
#include "processor/stackwalker_x86.h"

namespace google_airbag {

MinidumpProcessor::MinidumpProcessor(SymbolSupplier *supplier)
    : supplier_(supplier) {
}

MinidumpProcessor::~MinidumpProcessor() {
}

CallStack* MinidumpProcessor::Process(const string &minidump_file) {
  Minidump dump(minidump_file);
  if (!dump.Read()) {
    return NULL;
  }

  MinidumpException *exception = dump.GetException();
  if (!exception) {
    return NULL;
  }

  MinidumpThreadList *threads = dump.GetThreadList();
  if (!threads) {
    return NULL;
  }

  // TODO(bryner): get all the threads
  MinidumpThread *thread = threads->GetThreadByID(exception->GetThreadID());
  if (!thread) {
    return NULL;
  }

  MinidumpMemoryRegion *thread_memory = thread->GetMemory();
  if (!thread_memory) {
    return NULL;
  }

  scoped_ptr<Stackwalker> walker(
    Stackwalker::StackwalkerForCPU(exception->GetContext(), thread_memory,
                                   dump.GetModuleList(), supplier_));
  if (!walker.get()) {
    return NULL;
  }

  return walker->Walk();
}

}  // namespace google_airbag
