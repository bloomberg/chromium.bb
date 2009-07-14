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

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "native_client/src/shared/imc/nacl_imc_c.h"

static const NaClSocketAddress test_address = {
  "google-imc-test"
};

static NaClHandle g_front;

static void CleanUp(void) {
  NaClClose(g_front);
}

// Writes the last error message to the standard error.
void PrintError(const char* message) {
  char buffer[256];

  if (NaClGetLastErrorString(buffer, sizeof buffer) == 0) {
    fprintf(stderr, "%s: %s", message, buffer);
  }
}

int main(int argc, char* argv[]) {
  int result;
  NaClHandle pair[2];
  NaClMessageHeader header;
  NaClIOVec vec;
  char buffer[] = "Hello!";

  g_front = NaClBoundSocket(&test_address);
  if (g_front == NACL_INVALID_HANDLE) {
    PrintError("BoundSocket");
    exit(EXIT_FAILURE);
  }
  atexit(CleanUp);

  if (NaClSocketPair(pair) != 0) {
    PrintError("SocketPair");
    exit(EXIT_FAILURE);
  }

  vec.base = buffer;
  vec.length = sizeof buffer;

  // Test SendDatagram
  header.iov = &vec;
  header.iov_length = 1;
  header.handles = NULL;
  header.handle_count = 0;
  result = NaClSendDatagram(pair[0], &header, 0);
  assert(result == sizeof buffer);

  // Test ReceiveDatagram
  memset(buffer, 0, sizeof buffer);
  header.iov = &vec;
  header.iov_length = 1;
  header.handles = NULL;
  header.handle_count = 0;
  result = NaClReceiveDatagram(pair[1], &header, 0);
  assert(result == sizeof buffer);

  assert(strcmp(buffer, "Hello!") == 0);
  printf("%s\n", buffer);

  NaClClose(pair[0]);
  NaClClose(pair[1]);

  return 0;
}
