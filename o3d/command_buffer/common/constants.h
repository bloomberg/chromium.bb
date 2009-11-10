/*
 * Copyright 2009, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#ifndef O3D_COMMAND_BUFFER_COMMON_CROSS_CONSTANTS_H_
#define O3D_COMMAND_BUFFER_COMMON_CROSS_CONSTANTS_H_

#include "base/basictypes.h"

namespace command_buffer {

typedef int32 CommandBufferOffset;
const CommandBufferOffset kInvalidCommandBufferOffset = -1;

// Status of the command buffer service. It does not process commands
// (meaning: get will not change) unless in kParsing state.
namespace parser_status {
  enum ParserStatus {
    kNotConnected,  // The service is not connected - initial state.
    kNoBuffer,      // The service is connected but no buffer was set.
    kParsing,       // The service is connected, and parsing commands from the
                    // buffer.
    kParseError,    // Parsing stopped because a parse error was found.
  };
}

namespace parse_error {
  enum ParseError {
    kParseNoError,
    kParseInvalidSize,
    kParseOutOfBounds,
    kParseUnknownCommand,
    kParseInvalidArguments,
  };
}

// Invalid shared memory Id, returned by RegisterSharedMemory in case of
// failure.
const int32 kInvalidSharedMemoryId = -1;

}  // namespace command_buffer

#endif  // O3D_COMMAND_BUFFER_COMMON_CROSS_CONSTANTS_H_
