/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include <unistd.h>

ssize_t send(int socket, const void *buffer, size_t length, int flags) {
  return write(socket, buffer, length);
}
