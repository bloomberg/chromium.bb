/*
 * Copyright 2008, Google Inc.
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


// NaCl-NPAPI Interface

#ifndef NATIVE_CLIENT_SRC_SHARED_NPRUNTIME_NPRPC_H_
#define NATIVE_CLIENT_SRC_SHARED_NPRUNTIME_NPRPC_H_

#include "native_client/src/shared/npruntime/nacl_npapi.h"
#include "native_client/src/shared/npruntime/npcapability.h"

namespace nacl {

// The maximum number of NPVariant parameters passed by NPN_Invoke or
// NPN_InvokeDefault.
const size_t kParamMax = 256;

// The maximum size of the NPVariant structure in bytes among various platforms.
const size_t kNPVariantSizeMax = 16;

class RpcArgBuffer {
  NaClSrpcArg* arg_;
  char* limit_;
  char* base_;
  static const size_t kAlignSize = 4;

  // Rounds a size up to the nearest aligned address.
  static size_t RoundSize(size_t size) {
    return (size + kAlignSize - 1) & ~(kAlignSize - 1);
  }
  // Moves ptr size bytes forward in the RPC stack frame.
  char* Step(char* ptr, size_t size) {
    if (ptr + size == limit_) {
      // The last item on the stack does not have to be aligned at the tail.
      return limit_;
    }
    size = RoundSize(size);
    if (ptr + size <= limit_) {
      return ptr + size;
    }
    return NULL;
  }

 public:
  explicit RpcArgBuffer(NaClSrpcArg* arg) {
    if (NULL != arg &&
        NACL_SRPC_ARG_TYPE_CHAR_ARRAY == arg->tag &&
        NULL != arg->u.caval.carr &&
        0 < arg->u.caval.count) {
      base_ = arg->u.caval.carr;
      limit_ = arg->u.caval.carr + arg->u.caval.count;
    } else {
      base_ = NULL;
      limit_ = NULL;
    }
  }

  RpcArgBuffer(char* buf, uint32_t length) {
    if (NULL != buf && 0 < length) {
      base_ = buf;
      limit_ = buf + length;
    } else {
      base_ = NULL;
      limit_ = NULL;
    }
  }

  // Returns a pointer a region of the requested size if possible, NULL
  // otherwise.
  void* Request(size_t size) {
    if (base_ + size <= limit_) {
      return base_;
    } else {
      return NULL;
    }
  }

  // Moves the base pointer after a set of bytes has been consumed.
  void* Consume(size_t size) {
    base_ = Step(base_, size);
    return base_;
  }
};

// A helper class to assist in getting NPAPI types from SRPC arguments.
// Passing variable sized items requires two SRPC arguments.
// The first holds the fixed size portion, the second the optional portions.
class RpcArg {
  NPP npp_;
  RpcArgBuffer fixed_;
  RpcArgBuffer optional_;

 public:
  RpcArg(NPP npp, NaClSrpcArg* fixed_arg)
      : npp_(npp), fixed_(fixed_arg), optional_(NULL) { }

  RpcArg(NPP npp, NaClSrpcArg* fixed_arg, NaClSrpcArg* optional_arg)
      : npp_(npp), fixed_(fixed_arg), optional_(optional_arg) { }

  RpcArg(NPP npp, char* fixed_buf, uint32_t fixed_length)
      : npp_(npp), fixed_(fixed_buf, fixed_length), optional_(NULL) { }

  RpcArg(NPP npp,
         char* fixed_buf,
         uint32_t fixed_length,
         char* optional_buf,
         uint32_t optional_length)
      : npp_(npp),
        fixed_(fixed_buf, fixed_length),
        optional_(optional_buf, optional_length) { }

  // Reads an NPVariant from the argument vector.
  const NPVariant* GetVariant(bool param);
  // Puts an NPVariant into the argument vector.
  bool PutVariant(const NPVariant* variant);

  // Reads an array of NPVariant from the argument vector.
  const NPVariant* GetVariantArray(uint32_t count);
  // Puts an array of NPVariant into the argument vector.
  bool PutVariantArray(const NPVariant* variants, uint32_t count);

  // Reads an NPObject from the argument vector.
  NPObject* GetObject();
  // Puts an NPObject into the argument vector.
  bool PutObject(NPObject* object);

  // Reads an NPCapability from the argument vector.
  NPCapability* GetCapability();
  // Puts an NPCapability into the argument vector.
  bool PutCapability(const NPCapability* capability);

  // Reads an NPRect structure from the argument vector.
  NPRect* GetRect();
  // Puts an NPRect into the argument vector.
  bool PutRect(const NPRect* rect);
};

}  // namespace nacl

#endif  // NATIVE_CLIENT_SRC_SHARED_NPRUNTIME_NPRPC_H_
