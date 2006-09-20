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

// minidump_stackwalk.cc: Print the stack of the exception thread from a
// minidump.
//
// Author: Mark Mentovai

#include <stdlib.h>
#include <stdio.h>

#include <string>

#include "processor/minidump.h"
#include "processor/stackwalker_x86.h"


using std::string;
using namespace google_airbag;


int main(int argc, char** argv) {
  if (argc != 2) {
    fprintf(stderr, "usage: %s <file>\n", argv[0]);
    exit(1);
  }

  Minidump minidump(argv[1]);
  if (!minidump.Read()) {
    fprintf(stderr, "minidump.Read() failed\n");
    exit(1);
  }

  MinidumpException* exception = minidump.GetException();
  if (!exception) {
    fprintf(stderr, "minidump.GetException() failed\n");
    exit(1);
  }

  MinidumpThreadList* thread_list = minidump.GetThreadList();
  if (!thread_list) {
    fprintf(stderr, "minidump.GetThreadList() failed\n");
    exit(1);
  }

  MinidumpThread* exception_thread =
   thread_list->GetThreadByID(exception->GetThreadID());
  if (!exception_thread) {
    fprintf(stderr, "thread_list->GetThreadByID() failed\n");
    exit(1);
  }

  MemoryRegion* stack_memory = exception_thread->GetMemory();
  if (!stack_memory) {
    fprintf(stderr, "exception_thread->GetStackMemory() failed\n");
    exit(1);
  }

  MinidumpContext* context = exception->GetContext();
  if (!context) {
    fprintf(stderr, "exception->GetContext() failed\n");
    exit(1);
  }

  MinidumpModuleList* modules = minidump.GetModuleList();
  if (!modules) {
    fprintf(stderr, "minidump.GetModuleList() failed\n");
    exit(1);
  }

  StackwalkerX86 stackwalker = StackwalkerX86(context, stack_memory,
                                              modules, NULL);

  StackFrames stack;
  stackwalker.Walk(&stack);

  unsigned int index;
  for (index = 0 ; index < stack.size() ; index++) {
    StackFrame frame = stack.at(index);
    printf("[%2d]  ebp = 0x%08llx  eip = 0x%08llx  \"%s\" + 0x%08llx\n",
           index,
           frame.frame_pointer,
           frame.instruction,
           frame.module_base ? frame.module_name.c_str() : "0x0",
           frame.instruction - frame.module_base);
  }

  return 0;
}
