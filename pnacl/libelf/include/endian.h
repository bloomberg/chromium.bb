/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Darwin does not provide endian.h */
#ifndef __APPLE__
#include_next <endian.h>
#endif

