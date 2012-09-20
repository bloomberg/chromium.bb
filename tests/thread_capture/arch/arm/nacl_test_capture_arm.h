/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_TESTS_THREAD_CAPTURE_ARCH_ARM_NACL_TEST_CAPTURE_ARM_H_
#define NATIVE_CLIENT_TESTS_THREAD_CAPTURE_ARCH_ARM_NACL_TEST_CAPTURE_ARM_H_ 1

/*
 * This is a text section label, but not code that actually follows
 * the C calling convention.  We only want this for getting the
 * address.  Do not call!
 */
void NaClTestCapture(void);

#endif
