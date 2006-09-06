// Copyright (C) 2006 Google Inc.
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

// minidump_stackwalk.cc: Print the stack of the exception thread from a
// minidump.
//
// Author: Mark Mentovai

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#ifndef _WIN32
#include <unistd.h>
#define O_BINARY 0
#else // !_WIN32
#include <io.h>
#define open _open
#endif // !_WIN32

#include <memory>

#include "processor/minidump.h"
#include "processor/stackwalker_x86.h"


using std::auto_ptr;
using namespace google_airbag;


int main(int argc, char** argv) {
  if (argc != 2) {
    fprintf(stderr, "usage: %s <file>\n", argv[0]);
    exit(1);
  }

  int fd = open(argv[1], O_RDONLY | O_BINARY);
  if (fd == -1) {
    fprintf(stderr, "open failed\n");
    exit(1);
  }

  Minidump minidump(fd);
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

  StackwalkerX86 stackwalker = StackwalkerX86(context, stack_memory, modules);

  auto_ptr<StackFrames> stack(stackwalker.Walk());
  if (!stack.get()) {
     fprintf(stderr, "stackwalker->Walk() failed\n");
     exit(1);
  }

  unsigned int index;
  for (index = 0 ; index < stack->size() ; index++) {
    StackFrame frame = stack->at(index);
    printf("[%2d]  ebp = 0x%08llx  eip = 0x%08llx  \"%s\" + 0x%08llx\n",
           index,
           frame.frame_pointer,
           frame.instruction,
           frame.module_base ? frame.module_name.c_str() : "0x0",
           frame.instruction - frame.module_base);
  }

  return 0;
}
