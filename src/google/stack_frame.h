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

#ifndef GOOGLE_STACK_FRAME_H__
#define GOOGLE_STACK_FRAME_H__

#include <string>
#include <vector>
#include "google/airbag_types.h"

namespace google_airbag {

using std::string;
using std::vector;

struct StackFrame {
  // Initialize sensible defaults, or this will be instantiated with
  // primitive members in an undetermined state.
  StackFrame()
      : instruction()
      , frame_pointer()
      , module_base()
      , module_name()
      , function_base()
      , function_name()
      , source_file_name()
      , source_line() {}

  // The program counter location relative to the module base
  u_int64_t instruction;

  // The frame pointer to this stack frame
  u_int64_t frame_pointer;

  // The base address of the module
  u_int64_t module_base;

  // The module in which the pc resides
  string module_name;

  // The start address of the function, may be omitted if debug symbols
  // are not available.
  u_int64_t function_base;

  // The function name, may be omitted if debug symbols are not available
  string function_name;

  // The source file name, may be omitted if debug symbols are not available
  string source_file_name;

  // The (1-based) source line number,
  // may be omitted if debug symbols are not available
  int source_line;

  // TODO(bryner): saved registers
};

typedef vector<StackFrame> StackFrames;

}  // namespace google_airbag

#endif  // GOOGLE_STACK_FRAME_H__
