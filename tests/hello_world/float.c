/*
 * Copyright 2009, Google Inc.
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

/*
 * Test basic floating point operations
 */

#include <stdio.h>
#include <stdlib.h>

#if 1
#define PRINT_FLOAT(mesg, prec, val)\
 printf(mesg ": %." #prec  "f\n", (double) (val))
#else
#define PRINT_FLOAT(mesg, prec, val)\
  printf(mesg ": %d\n", (int) (val))
#endif

#define PRINT_INT(mesg, val)\
  printf(mesg ": %d\n", (int) (val))


/* TODO(robertm): maybe use macro */
int main_double(int argc, char* argv[]) {
  int i;
  double last = 0.0;
  double x;
  printf("double\n");

  for (i = 1; i < argc; ++i) {
    printf("val str: %s\n", argv[i]);

    x = strtod(argv[i], 0);
    PRINT_INT("val int", x);
    PRINT_FLOAT("val flt",9, x);
    PRINT_FLOAT("last", 9, last);
    PRINT_FLOAT("+", 9, last + x);
    PRINT_FLOAT("-", 9, last - x);
    PRINT_FLOAT("*", 9, last * x);
    PRINT_FLOAT("/", 9, last / x);
    printf("\n");
    last = x;
  }
  return 0;
}


/* TODO(robertm): maybe use macro */
int main_float(int argc, char* argv[]) {
  int i;
  float last = 0.0;
  float x;
  printf("float\n");

  for (i = 1; i < argc; ++i) {
    printf("val str: %s\n", argv[i]);

    x = strtof(argv[i], 0);
    PRINT_INT("val int", x);
    PRINT_FLOAT("val flt",9, x);
    PRINT_FLOAT("last", 3, last);
    PRINT_FLOAT("+", 3, last + x);
    PRINT_FLOAT("-", 3, last - x);
    PRINT_FLOAT("*", 3, last * x);
    PRINT_FLOAT("/", 3, last / x);
    printf("\n");
    last = x;
  }
  return 0;
}


int main(int argc, char* argv[]) {
  main_float(argc, argv);
  main_double(argc, argv);
  return 0;
}
