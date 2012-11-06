/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */


#include <sys/stat.h>
#include <sys/types.h>

#include <fcntl.h>
#include <string.h>
#include <unistd.h>

size_t strnlen(const char* string, size_t max);

size_t strnlen(const char* string, size_t max) {
  const char* end = (const char*)memchr(string, '\0', max);
  return end ? (size_t)(end - string) : max;
}
