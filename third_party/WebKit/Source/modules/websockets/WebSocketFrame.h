/*
 * Copyright (C) 2012 Google Inc.  All rights reserved.
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

#ifndef WebSocketFrame_h
#define WebSocketFrame_h

#include "platform/wtf/Allocator.h"

namespace blink {

struct WebSocketFrame {
  STACK_ALLOCATED();
  // RFC6455 opcodes.
  enum OpCode {
    kOpCodeContinuation = 0x0,
    kOpCodeText = 0x1,
    kOpCodeBinary = 0x2,
    kOpCodeClose = 0x8,
    kOpCodePing = 0x9,
    kOpCodePong = 0xA,
    kOpCodeInvalid = 0x10
  };

  // Flags for the constructor.
  // This is not the bitmasks for frame composition / decomposition.
  enum {
    kEmptyFlags = 0,
    kFinal = 1,
    kReserved1 = 2,
    kCompress = 2,
    kReserved2 = 4,
    kReserved3 = 8,
    kMasked = 16,
  };
  typedef unsigned Flags;
  // The Flags parameter shall be a combination of above flags.
  WebSocketFrame(OpCode,
                 const char* payload,
                 size_t payload_length,
                 Flags = kEmptyFlags);

  OpCode op_code;
  bool final;
  bool compress;
  bool reserved2;
  bool reserved3;
  bool masked;
  const char* payload;
  size_t payload_length;
};

}  // namespace blink

#endif  // WebSocketFrame_h
