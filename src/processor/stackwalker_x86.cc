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
                               SymbolSupplier*     supplier,
                               void*               supplier_data)
    : Stackwalker(memory, modules, supplier, supplier_data),
      last_frame_pointer_(0) {
  if (memory_->GetBase() + memory_->GetSize() - 1 > 0xffffffff) {
    // The x86 is a 32-bit CPU, the limits of the supplied stack are invalid.
    // Mark memory_ = NULL, which will cause stackwalking to fail.
    memory_ = NULL;
  }

  // TODO(mmentovai): verify that |context| is x86 when Minidump supports
  // other CPU types.
  context_ = context->context();
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
