/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Newlib provides <sys/features.h>, but not <features.h>.
 * This file handles includes of <features.h>
 */

/* Apple does not provide features.h in any form */
#ifndef __APPLE__
#include <sys/features.h>
#endif
