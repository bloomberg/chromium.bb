/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef NATIVE_CLIENT_TESTS_EARTH_EARTH_H_
#define NATIVE_CLIENT_TESTS_EARTH_EARTH_H_

#include <string.h>

/* NaCl Earth demo */
#ifdef __cplusplus
extern "C" {
#endif

void DebugPrintf(const char *fmt, ...);
void Earth_Init(int argcount, const char *argname[], const char *argvalue[]);
void Earth_Draw(uint32_t* image_data);

#ifdef __cplusplus
}
#endif

#endif  /* NATIVE_CLIENT_TESTS_EARTH_EARTH_H_ */
