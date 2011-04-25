/*
 * Copyright 2009 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

/* Define simple debugging utilities that are turned on/off by the
 * value of the define flag DEBUGGING.
 *
 * To turn on debugging of instruction decoding, change value of
 * DEBUGGING to 1.
 *
 */

#ifndef NATIVE_CLIENT_SRC_SHARED_UTILS_DEBUGGING_H__
#define NATIVE_CLIENT_SRC_SHARED_UTILS_DEBUGGING_H__

#if DEBUGGING
/* Defines to execute statement(s) s if in debug mode. */
#define DEBUG(s) s
#else
/* Defines to not include statement(s) s if not in debugging mode. */
#define DEBUG(s) do { if (0) { s; } } while (0)
#endif

/* Turn off debugging if not otherwise specified in the specific code file. */
#ifndef DEBUGGING
#define DEBUGGING 0
#endif

#endif  /* NATIVE_CLIENT_SRC_SHARED_UTILS_DEBUGGING_H__ */
