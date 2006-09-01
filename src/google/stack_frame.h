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

#ifndef _STACK_FRAME_H__
#define _STACK_FRAME_H__

#include <vector>
#include "airbag_types.h"

namespace google_airbag {

using std::string;

struct StackFrame {
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

  // The source line number, may be omitted if debug symbols are not available
  int source_line;

  // TODO(bryner): saved registers
};

typedef std::vector<StackFrame> StackFrames;

}  // namespace google_airbag

#endif  // _STACK_FRAME_H__
