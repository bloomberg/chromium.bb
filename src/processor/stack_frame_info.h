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

// stack_frame_info.h: Holds debugging information about a stack frame.
//
// This structure is specific to Windows debugging information obtained
// from pdb files using the DIA API.
//
// Author: Mark Mentovai


#ifndef PROCESSOR_STACK_FRAME_INFO_H__
#define PROCESSOR_STACK_FRAME_INFO_H__

#include <string>

#include "google/airbag_types.h"

namespace google_airbag {

struct StackFrameInfo {
 public:
  StackFrameInfo() : valid(false),
                     prolog_size(0),
                     epilog_size(0),
                     parameter_size(0),
                     saved_register_size(0),
                     local_size(0),
                     max_stack_size(0),
                     program_string() {}

  StackFrameInfo(u_int32_t set_prolog_size,
                 u_int32_t set_epilog_size,
                 u_int32_t set_parameter_size,
                 u_int32_t set_saved_register_size,
                 u_int32_t set_local_size,
                 u_int32_t set_max_stack_size,
                 const std::string set_program_string)
      : valid(true),
        prolog_size(set_prolog_size),
        epilog_size(set_epilog_size),
        parameter_size(set_parameter_size),
        saved_register_size(set_saved_register_size),
        local_size(set_local_size),
        max_stack_size(set_max_stack_size),
        program_string(set_program_string) {}

  // True when the contents of the structure are valid.
  bool valid;

  // These values come from IDiaFrameData.
  u_int32_t prolog_size;
  u_int32_t epilog_size;
  u_int32_t parameter_size;
  u_int32_t saved_register_size;
  u_int32_t local_size;
  u_int32_t max_stack_size;
  std::string program_string;
};

}  // namespace google_airbag


#endif  // PROCESSOR_STACK_FRAME_INFO_H__
