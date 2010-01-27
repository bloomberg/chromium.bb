/*
 * Copyright 2009 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

/* Define some common debugging tools, based on the value of DEBUGGING. */

#ifndef NATIVE_CLIENT_PRIVATE_TOOLS_NCV_ARM_DEBUGGING_H__
#define NATIVE_CLIENT_PRIVATE_TOOLS_NCV_ARM_DEBUGGING_H__

#ifndef DEBUGGING
#define DEBUGGING 0
#endif

#if DEBUGGING
#define DEBUG(x) x
#else
#define DEBUG(x)
#endif

#endif  // NATIVE_CLIENT_PRIVATE_TOOLS_NCV_ARM_DEBUGGING_H__
