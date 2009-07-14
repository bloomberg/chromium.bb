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

#include "native_client/src/shared/imc/nacl_imc.h"
#include "native_client/src/shared/imc/nacl_imc_c.h"

//
// "C" bindings
//

int NaClGetLastErrorString(char* buffer, size_t length) {
  return nacl::GetLastErrorString(buffer, length);
}

NaClHandle NaClBoundSocket(const NaClSocketAddress* address) {
  return nacl::BoundSocket(
      reinterpret_cast<const nacl::SocketAddress*>(address));
}

int NaClSocketPair(NaClHandle pair[2]) {
  return nacl::SocketPair(pair);
}

int NaClClose(NaClHandle handle) {
  return nacl::Close(handle);
}

int NaClWouldBlock() {
  return nacl::WouldBlock();
}

int NaClSendDatagram(NaClHandle socket, const NaClMessageHeader* message,
                    int flags) {
  return nacl::SendDatagram(
      socket,
      reinterpret_cast<const nacl::MessageHeader*>(message),
      flags);
}

int NaClSendDatagramTo(NaClHandle socket, const NaClMessageHeader* message,
                       int flags, const NaClSocketAddress* name) {
  return nacl::SendDatagramTo(
      socket,
      reinterpret_cast<const nacl::MessageHeader*>(message),
      flags,
      reinterpret_cast<const nacl::SocketAddress*>(name));
}

int NaClSend(NaClHandle socket, const void* buffer, size_t length,
             int flags) {
  return nacl::Send(socket, buffer, length, flags);
}

int NaClSendTo(NaClHandle socket, const void* buffer, size_t length,
               int flags, const NaClSocketAddress* name) {
  return nacl::SendTo(socket, buffer, length, flags,
                      reinterpret_cast<const nacl::SocketAddress*>(name));
}

int NaClReceiveDatagram(NaClHandle socket, NaClMessageHeader* message,
                       int flags) {
  return nacl::ReceiveDatagram(socket,
                               reinterpret_cast<nacl::MessageHeader*>(message),
                               flags);
}

int NaClReceive(NaClHandle socket, void* buffer, size_t length, int flags) {
  return nacl::Receive(socket, buffer, length, flags);
}

NaClHandle NaClCreateMemoryObject(size_t length) {
  return nacl::CreateMemoryObject(length);
}

void* NaClMap(void* start, size_t length, int prot, int flags,
              NaClHandle memory, off_t offset) {
  return nacl::Map(start, length, prot, flags, memory, offset);
}

int NaClUnmap(void* start, size_t length) {
  return nacl::Unmap(start, length);
}
