/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */


// NaCl inter-module communication primitives.

// TODO(shiki): Make this program a unit test.

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include "native_client/src/shared/imc/nacl_imc.h"

nacl::SocketAddress server_address = {
  "imc-server"
};

nacl::SocketAddress client_address = {
  "imc-client"
};

namespace {

nacl::Handle g_front;

void CleanUp() {
  nacl::Close(g_front);
}

// Writes the last error message to the standard error.
void PrintError(const char* message) {
  char buffer[256];

  if (nacl::GetLastErrorString(buffer, sizeof buffer) == 0) {
    fprintf(stderr, "%s: %s\n", message, buffer);
  }
}

}  // namespace

int main() {
  int result;

  g_front = nacl::BoundSocket(&client_address);
  if (g_front == nacl::kInvalidHandle) {
    PrintError("BoundSocket");
    exit(EXIT_FAILURE);
  }
  atexit(CleanUp);

  nacl::Handle pair[2];
  if (nacl::SocketPair(pair) != 0) {
    PrintError("SocketPair");
    exit(EXIT_FAILURE);
  }

  // Test more than one SocketPair() creation
  nacl::Handle pair2[2];
  result = nacl::SocketPair(pair2);
  assert(result == 0);
  if (result != 0) {
    PrintError("SocketPair");
    exit(EXIT_FAILURE);
  }
  nacl::Close(pair2[0]);
  nacl::Close(pair2[1]);

  nacl::MessageHeader header;
  nacl::IOVec vec[2];
  char buffer[64];

  // Test kDontWait for a bound socket
  vec[0].base = buffer;
  vec[0].length = sizeof buffer;
  header.iov = vec;
  header.iov_length = 1;
  header.handles = NULL;
  header.handle_count = 0;
  result = nacl::ReceiveDatagram(g_front, &header, nacl::kDontWait);
  assert(result == -1);
  PrintError("ReceiveDatagram");
  assert(nacl::WouldBlock());

#if NACL_WINDOWS
  strncpy_s(buffer, sizeof buffer, "Hello!", sizeof buffer);
#else
  strncpy(buffer, "Hello!", sizeof buffer);
#endif  // NACL_WINDOWS
  vec[0].base = buffer;
  vec[0].length = strlen(buffer);

  // Test kCancel ControlHeader::command
  nacl::Handle broken[2] = {
    pair[1],
    nacl::Handle(0xffff00ff)  // invalid handle
  };
  header.iov = vec;
  header.iov_length = 1;
  header.handles = broken;
  header.handle_count = sizeof broken / sizeof broken[0];
  result = nacl::SendDatagramTo(g_front, &header, 0, &server_address);
  assert(result == -1);

  // Send pair[1] to the server.
  header.iov = vec;
  header.iov_length = 1;
  header.handles = &pair[1];
  header.handle_count = 1;
  result = nacl::SendDatagramTo(g_front, &header, 0, &server_address);
  if (static_cast<size_t>(result) != vec[0].length) {
    PrintError("SendDatagram");
    exit(EXIT_FAILURE);
  }
  result = nacl::Close(pair[1]);
  if (result != 0) {
    PrintError("Close");
    exit(EXIT_FAILURE);
  }

  nacl::Handle server_handle = pair[0];
  nacl::Handle shared_memory;

  // Test an empty message
  memset(buffer, 0, sizeof buffer);
  vec[0].base = buffer;
  vec[0].length = sizeof buffer;
  header.iov = vec;
  header.iov_length = 1;
  header.handles = NULL;
  header.handle_count = 0;
  result = nacl::ReceiveDatagram(server_handle, &header, 0);
  assert(result == 0);

  // Test scatter/gather
  memset(buffer, 0, sizeof buffer);
  header.iov = vec;
  header.iov_length = sizeof vec / sizeof vec[0];
  header.handles = NULL;
  header.handle_count = 0;
  for (size_t i = 0; i < sizeof vec / sizeof vec[0]; ++i) {
    vec[i].base = buffer + i;
    vec[i].length = 1;
  }
  result = nacl::ReceiveDatagram(server_handle, &header, 0);
  assert(result == sizeof vec / sizeof vec[0]);
  assert(header.flags & nacl::kMessageTruncated);
  assert(header.flags & nacl::kHandlesTruncated);
  printf("%s\n", buffer);

  // Test Send()
  result = nacl::Send(server_handle, buffer, sizeof vec / sizeof vec[0], 0);
  assert(result == sizeof vec / sizeof vec[0]);

  // Test shared memory
  header.iov = vec;
  header.iov_length = sizeof vec / sizeof vec[0];
  header.handles = &shared_memory;
  header.handle_count = 1;
  if (0 <= nacl::ReceiveDatagram(server_handle, &header, 0) &&
      header.handle_count == 1) {
    void* shared_region = nacl::Map(0, 1024 * 1024,
                                    nacl::kProtRead | nacl::kProtWrite,
                                    nacl::kMapShared,
                                    shared_memory, 0);
    if (shared_region) {
      while (*static_cast<volatile int*>(shared_region) == 0x00) {
      }
      printf("client: shm ok.\n");
      result = nacl::Send(server_handle, buffer, sizeof buffer, 0);
      assert(result == sizeof buffer);
      memset(shared_region, 0x00, 1024 * 1024);
      nacl::Unmap(shared_region, 1024 * 1024);
    }
    nacl::Close(shared_memory);
  } else {
    PrintError("ReceiveDatagram");
  }

  // Test close detection.
  nacl::Close(server_handle);
  return 0;
}
