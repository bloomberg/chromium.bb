/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <assert.h>
#include <string.h>
#include <alloca.h>

int global_var;
volatile void *global_ptr;

void test_two_line_function(int arg) {
  global_var = arg - 1;
  global_var = arg;
}

void test_stepi_after_break() {
  /* Something meaningful to step through.  */
  global_var = 0;
}

void set_global_var(int arg) {
  global_var = arg;
}

void leaf_call(int arg) {
  global_var = arg;
}

void nested_calls(int arg) {
  global_var = 1;
  leaf_call(arg + 1);
}

int test_print_symbol() {
  int local_var = 3;
  global_var = 2 + local_var * 0; /* Use local variable to prevent warning.  */
  set_global_var(1);
  nested_calls(1);
  return global_var;
}

/* A function with non-trivial prolog. */
void test_step_from_function_start(int arg) {
  int local_var = arg - 1;
  global_var = local_var;
  /*
   * Force using frame pointer for this function by calling alloca.
   * This allows to test skipping %esp modifying instructions when they
   * are located in the middle of the function.
   */
  global_ptr = alloca(arg);
}

int main(int argc, char **argv) {
  assert(argc >= 2);

  if (strcmp(argv[1], "break_inside_function") == 0) {
    test_two_line_function(1);
    return 0;
  }
  if (strcmp(argv[1], "stepi_after_break") == 0) {
    test_stepi_after_break();
    return 0;
  }
  if (strcmp(argv[1], "print_symbol") == 0) {
    return test_print_symbol();
  }
  if (strcmp(argv[1], "stack_trace") == 0) {
    nested_calls(1);
    return 0;
  }
  if (strcmp(argv[1], "step_from_func_start") == 0) {
    global_var = 0;
    test_step_from_function_start(2);
    return 0;
  }
  return 1;
}
