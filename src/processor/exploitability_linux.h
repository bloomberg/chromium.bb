// Copyright (c) 2013 Google Inc.
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

// exploitability_linux.h: Linux specific exploitability engine.
//
// Provides a guess at the exploitability of the crash for the Linux
// platform given a minidump and process_state.
//
// Author: Matthew Riley

#ifndef GOOGLE_BREAKPAD_PROCESSOR_EXPLOITABILITY_LINUX_H_
#define GOOGLE_BREAKPAD_PROCESSOR_EXPLOITABILITY_LINUX_H_

#include "common/scoped_ptr.h"
#include "google_breakpad/common/breakpad_types.h"
#include "google_breakpad/processor/exploitability.h"

namespace google_breakpad {

class ExploitabilityLinux : public Exploitability {
 public:
  ExploitabilityLinux(Minidump *dump,
                      ProcessState *process_state);

  virtual ExploitabilityRating CheckPlatformExploitability();

 private:
  // Takes the address of the instruction pointer and returns
  // whether the instruction pointer lies in a valid instruction region.
  bool InstructionPointerInCode(uint64_t instruction_ptr);

  // Checks the exception that triggered the creation of the
  // minidump and reports whether the exception suggests no exploitability.
  bool BenignCrashTrigger(const MDRawExceptionStream *raw_exception_stream);

  // Checks if the stack pointer points to a memory mapping that is not
  // labelled as the stack.
  bool StackPointerOffStack(uint64_t stack_ptr);

  // Checks if the stack or heap are marked executable according
  // to the memory mappings.
  bool ExecutableStackOrHeap();
};

}  // namespace google_breakpad

#endif  // GOOGLE_BREAKPAD_PROCESSOR_EXPLOITABILITY_LINUX_H_
