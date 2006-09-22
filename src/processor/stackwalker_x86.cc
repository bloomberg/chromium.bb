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

// stackwalker_x86.cc: x86-specific stackwalker.
//
// See stackwalker_x86.h for documentation.
//
// Author: Mark Mentovai


#include "processor/stackwalker_x86.h"
#include "processor/minidump.h"


namespace google_airbag {


StackwalkerX86::StackwalkerX86(MinidumpContext*    context,
                               MemoryRegion*       memory,
                               MinidumpModuleList* modules,
                               SymbolSupplier*     supplier)
    : Stackwalker(memory, modules, supplier),
      last_frame_pointer_(0) {
  if (memory_->GetBase() + memory_->GetSize() - 1 > 0xffffffff) {
    // The x86 is a 32-bit CPU, the limits of the supplied stack are invalid.
    // Mark memory_ = NULL, which will cause stackwalking to fail.
    memory_ = NULL;
  }

  // If |context| is not an x86 context, context_ will be set to NULL,
  // which will cause GetContextFrame to fail when called by Walk.
  // For StackwalkerX86, |context| should only ever be an x86 context.
  context_ = context->GetContextX86();
}


bool StackwalkerX86::GetContextFrame(StackFrame* frame) {
  if (!context_ || !memory_ || !frame)
    return false;

  // The frame and instruction pointers are stored directly in registers,
  // so pull them straight out of the CPU context structure.
  frame->frame_pointer = last_frame_pointer_ = context_->ebp;
  frame->instruction = context_->eip;

  return true;
}


bool StackwalkerX86::GetCallerFrame(StackFrame* frame) {
  if (!memory_ || !frame)
    return false;

  // The frame and instruction pointers for previous frames are saved on the
  // stack.  The typical x86 calling convention, when frame pointers are
  // present, is for the calling procedure to use CALL, which pushes the
  // return address onto the stack and sets the instruction pointer (%eip)
  // to the entry point of the called routine.  The called routine's then
  // PUSHes the calling routine's frame pointer (%ebp) onto the stack before
  // copying the stack pointer (%esp) to the frame pointer (%ebp).  Therefore,
  // the calling procedure's frame pointer is always available by
  // dereferencing the called procedure's frame pointer, and the return
  // address is always available at the memory location immediately above
  // the address pointed to by the called procedure's frame pointer.

  // If there is no frame pointer, determining the layout of the stack is
  // considerably more difficult, requiring debugging information.  This
  // stackwalker doesn't attempt to solve that problem (at this point).

  // Don't pass frame.frame_pointer or frame.instruction directly
  // ReadMemory, because their types are too wide (64-bit), and we
  // specifically want to read 32-bit quantities for both.
  u_int32_t frame_pointer;
  if (!memory_->GetMemoryAtAddress(last_frame_pointer_, &frame_pointer))
    return false;

  // A caller frame must reside higher in memory than its callee frames.
  // Anything else is an error, or an indication that we've reached the
  // end of the stack.
  if (frame_pointer <= last_frame_pointer_)
    return false;

  u_int32_t instruction;
  if (!memory_->GetMemoryAtAddress(last_frame_pointer_ + 4, &instruction))
    return false;

  frame->frame_pointer = last_frame_pointer_ = frame_pointer;
  frame->instruction = instruction;

  return true;
}


} // namespace google_airbag
