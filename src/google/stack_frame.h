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
