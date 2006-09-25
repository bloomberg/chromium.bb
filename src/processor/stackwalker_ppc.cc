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

// stackwalker_ppc.cc: ppc-specific stackwalker.
//
// See stackwalker_ppc.h for documentation.
//
// Author: Mark Mentovai


#include "processor/stackwalker_ppc.h"
#include "processor/minidump.h"

namespace google_airbag {


StackwalkerPPC::StackwalkerPPC(const MDRawContextPPC *context,
                               MemoryRegion *memory,
                               MinidumpModuleList *modules,
                               SymbolSupplier *supplier)
    : Stackwalker(memory, modules, supplier),
      context_(context) {
  if (memory_->GetBase() + memory_->GetSize() - 1 > 0xffffffff) {
    // This implementation only covers 32-bit ppc CPUs.  The limits of the
    // supplied stack are invalid.  Mark memory_ = NULL, which will cause
    // stackwalking to fail.
    memory_ = NULL;
  }
}


bool StackwalkerPPC::GetContextFrame(StackFrame *frame) {
  if (!context_ || !memory_ || !frame)
    return false;

  // The stack frame and instruction pointers are stored directly in
  // registers, so pull them straight out of the CPU context structure.
  frame->frame_pointer = context_->gpr[1];
  frame->instruction = context_->srr0;

  return true;
}


bool StackwalkerPPC::GetCallerFrame(StackFrame *frame,
                                    const StackFrames *walked_frames) {
  if (!memory_ || !frame || !walked_frames)
    return false;

  // The stack frame and instruction pointers for previous frames are saved
  // on the stack.  The typical ppc calling convention is for the called
  // procedure to store its return address in the calling procedure's stack
  // frame at 8(%r1), and to allocate its own stack frame by decrementing %r1
  // (the stack pointer) and saving the old value of %r1 at 0(%r1).  Because
  // the ppc has no hardware stack, there is no distinction between the
  // stack pointer and frame pointer, and what is typically thought of as
  // the frame pointer on an x86 is usually referred to as the stack pointer
  // on a ppc.

  u_int32_t last_stack_pointer = walked_frames->back().frame_pointer;

  // Don't pass frame.frame_pointer or frame.instruction directly
  // ReadMemory, because their types are too wide (64-bit), and we
  // specifically want to read 32-bit quantities for both.
  u_int32_t stack_pointer;
  if (!memory_->GetMemoryAtAddress(last_stack_pointer, &stack_pointer))
    return false;

  // A caller frame must reside higher in memory than its callee frames.
  // Anything else is an error, or an indication that we've reached the
  // end of the stack.
  if (stack_pointer <= last_stack_pointer)
    return false;

  u_int32_t instruction;
  if (!memory_->GetMemoryAtAddress(stack_pointer + 8, &instruction))
    return false;

  // Mac OS X/Darwin gives 1 as the return address from the bottom-most
  // frame in a stack (a thread's entry point).  I haven't found any
  // documentation on this, but 0 or 1 would be bogus return addresses,
  // so check for them here and return false (end of stack) when they're
  // hit to avoid having a phantom frame.
  if (instruction <= 1)
    return false;

  frame->frame_pointer = stack_pointer;
  frame->instruction = instruction;

  return true;
}


} // namespace google_airbag
