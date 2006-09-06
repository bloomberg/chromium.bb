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

// stackwalker_x86.h: x86-specific stackwalker.
//
// Provides stack frames given x86 register context and a memory region
// corresponding to an x86 stack.
//
// Author: Mark Mentovai


#ifndef PROCESSOR_STACKWALKER_X86_H__
#define PROCESSOR_STACKWALKER_X86_H__


#include "google/airbag_types.h"
#include "processor/stackwalker.h"
#include "processor/minidump_format.h"


namespace google_airbag {


class MinidumpContext;
class MinidumpModuleList;


class StackwalkerX86 : public Stackwalker {
  public:
    // context is a MinidumpContext object that gives access to x86-specific
    // register state corresponding to the innermost called frame to be
    // included in the stack.  memory and modules are passed directly through
    // to the base Stackwalker constructor.
    StackwalkerX86(MinidumpContext*    context,
                   MemoryRegion*       memory,
                   MinidumpModuleList* modules);

  private:
    // Implementation of Stackwalker, using x86 context (%ebp, %eip) and
    // stack conventions (saved %ebp at [%ebp], saved %eip at 4[%ebp]).
    virtual bool GetContextFrame(StackFrame* frame);
    virtual bool GetCallerFrame(StackFrame* frame);

    // Stores the CPU context corresponding to the innermost stack frame to
    // be returned by GetContextFrame.
    const MDRawContextX86* context_;

    // Stores the frame pointer returned in the last stack frame returned by
    // GetContextFrame or GetCallerFrame.
    u_int32_t              last_frame_pointer_;
};


} // namespace google_airbag


#endif // PROCESSOR_STACKWALKER_X86_H__
