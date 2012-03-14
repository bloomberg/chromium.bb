/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "native_client/src/trusted/service_runtime/sel_main_chrome.h"


int main() {
  /*
   * This currently tests linking of sel_main_chrome.c *only*.  We do
   * not expect this program to do anything meaningful when run.
   */
  struct NaClChromeMainArgs *args = NaClChromeMainArgsCreate();
  NaClChromeMainStart(args);
  return 1;
}
