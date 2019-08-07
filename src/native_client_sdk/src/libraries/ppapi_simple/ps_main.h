/* Copyright 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

#ifndef PPAPI_SIMPLE_PS_MAIN_H_
#define PPAPI_SIMPLE_PS_MAIN_H_

#include "ppapi_simple/ps.h"
#include "ppapi_simple/ps_event.h"

EXTERN_C_BEGIN

typedef int (*PSMainFunc_t)(int argc, char *argv[]);

/**
 * PSUserMainGet
 *
 * Prototype for the user provided function which retrieves the user's main
 * function.
 * This is normally defined using the PPAPI_SIMPLE_REGISTER_MAIN macro.
 */
PSMainFunc_t PSUserMainGet();

/**
 * PPAPI_SIMPLE_REGISTER_MAIN
 *
 * Constructs a PSInstance object and configures it to use call the provided
 * 'main' function on its own thread once initialization is complete.
 *
 * The ps_entrypoint_*.o and ps_main.o objects will not be linked by default,
 * so we force them to be linked here.
 */
#define PPAPI_SIMPLE_REGISTER_MAIN(main_func) \
  PSMainFunc_t PSUserMainGet() { return main_func; }

EXTERN_C_END

#endif  /* PPAPI_SIMPLE_PS_MAIN_H_ */
