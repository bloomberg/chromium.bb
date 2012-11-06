/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */


/*
 * Stub routines for arm raise
 */

/* TODO(robertm): this is neither the final place not the final implementation
                  for this function */

/* @IGNORE_LINES_FOR_CODE_HYGIENE[1] */
extern void exit(int);

int raise(int sig) {
  exit(-1);
  return 0;
}
