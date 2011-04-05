/*
 * Copyright 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include <unistd.h>
#include <string.h>

int main() {
  const char *string = "In blob library\n";
  write(1, string, strlen(string));
  return 0;
}
