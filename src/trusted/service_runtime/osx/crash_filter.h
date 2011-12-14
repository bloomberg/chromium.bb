/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_SRC_TRUSTED_SERVICE_RUNTIME_OSX_CRASH_FILTER_H_
#define NATIVE_CLIENT_SRC_TRUSTED_SERVICE_RUNTIME_OSX_CRASH_FILTER_H_ 1


#include <mach/port.h>

/*
 * This function is intended for use by Chromium's embedding of
 * Breakpad crash reporting.  Given the Mach port for a thread in this
 * process that has crashed (and is suspended), this function returns
 * whether the thread crashed inside NaCl untrusted code.  This is
 * used for deciding whether to report the crash.
 */
int NaClMachThreadIsInUntrusted(mach_port_t thread_port);


#endif
