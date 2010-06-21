/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */


#include "native_client/src/include/portability.h"
#include "native_client/src/include/portability_io.h"
#include "native_client/src/include/portability_string.h"
#include "native_client/src/shared/npruntime/nacl_npapi.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

namespace nacl {

class NPInstance;

extern void OpenMacFile(NPStream* stream,
                        const char* filename,
                        NPInstance* module);

}  // namespace nacl
