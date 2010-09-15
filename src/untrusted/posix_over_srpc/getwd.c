/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include <string.h>

#define PATH_MAX 4096

char* getcwd(char* buf, size_t size);

char* getwd(char* buf) {
  char safe_buf[PATH_MAX];
  if (NULL == getcwd(safe_buf, PATH_MAX)) {
    return NULL;
  } else {
    strcpy(buf, safe_buf);
    return buf;
  }
}
