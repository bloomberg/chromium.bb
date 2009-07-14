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


/*
 * Used to provide stack traceback support in NaCl modules in the debugger.
 * When the application is compiled with -finstrument functions, stack tracing
 * is turned on.  This module maintains a stack of function entry addresses.
 * The state of the stack trace can be printed at any point by calling
 * __nacl_dump_stack_trace.
 */

#include <stdio.h>

/*
 * Because of the use of static variables, this module is NOT THREAD SAFE.
 * TODO(sehr): use thread local variables for this, etc., if possible.
 */
#define STACK_MAX_DEPTH 1024
static void* stack[STACK_MAX_DEPTH];

static int top_loc = 0;

static void push(void* function_address) {
  if (top_loc < STACK_MAX_DEPTH) {
    stack[top_loc] = function_address;
  }
  /*
   * Although we have a static limit on the number of function addresses that
   * may be pushed, we keep track of the depth even beyond that.  Deeply
   * recursive chains will look confused, and may make top_loc wrap around
   * (to a negative number) in unusual cases, but we can improve this later.
   */
  ++top_loc;
}

static void pop() {
  /*
   * It's possible the calls and returns won't match.  Don't decrement past the
   * bottom of the stack.
   */
  if (top_loc > 0) {
    --top_loc;
  }
}

void __cyg_profile_func_enter(void* this_fn, void* call_site) {
  push(this_fn);
}

void __cyg_profile_func_exit(void* this_fn, void* call_site) {
  pop();
}

void __nacl_dump_stack_trace() {
  int i;
  int top_to_display;
  if (top_loc > STACK_MAX_DEPTH) {
    printf("Overflow: %d calls were not recorded.\n",
           top_loc - STACK_MAX_DEPTH);
    top_to_display = STACK_MAX_DEPTH;
  } else {
    top_to_display = top_loc;
  }
  for (i = top_to_display - 1; i >= 0; i--) {
    printf("%d: %p\n", top_loc - 1 - i, stack[i]);
  }
}
