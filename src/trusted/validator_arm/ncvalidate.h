/*
 * Copyright 2009 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include "native_client/src/include/portability.h"

EXTERN_C_BEGIN

void NCValidateInit();

void NCValidateSegment(uint8_t *mbase, uint32_t vbase, size_t size);

int NCValidateFinish();

EXTERN_C_END

