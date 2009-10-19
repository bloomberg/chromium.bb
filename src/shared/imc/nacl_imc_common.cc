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


// NaCl inter-module communication primitives.

// TODO(robertm): stdio.h is included for NULL only - find a better way
#include <stdio.h>
#include "native_client/src/shared/imc/nacl_imc.h"

namespace nacl {

int Send(Handle socket, const void* buffer, size_t length, int flags) {
  MessageHeader header;
  IOVec vec;

  vec.base = const_cast<void*>(buffer);
  vec.length = length;
  header.iov = &vec;
  header.iov_length = 1;
  header.handles = NULL;
  header.handle_count = 0;
  header.flags = 0;
  return SendDatagram(socket, &header, flags);
}

int SendTo(Handle socket, const void* buffer, size_t length, int flags,
           const SocketAddress* name) {
  MessageHeader header;
  IOVec vec;

  vec.base = const_cast<void*>(buffer);
  vec.length = length;
  header.iov = &vec;
  header.iov_length = 1;
  header.handles = NULL;
  header.handle_count = 0;
  header.flags = 0;
  return SendDatagramTo(socket, &header, flags, name);
}

int Receive(Handle socket, void* buffer, size_t length, int flags) {
  MessageHeader header;
  IOVec vec;

  vec.base = buffer;
  vec.length = length;
  header.iov = &vec;
  header.iov_length = 1;
  header.handles = NULL;
  header.handle_count = 0;
  header.flags = 0;
  return ReceiveDatagram(socket, &header, flags);
}

}  // namespace nacl
