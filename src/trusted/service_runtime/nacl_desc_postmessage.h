/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * Subclass of NaClDesc which passes write output data to the
 * JavaScript console using the reverse channel.
 */

#ifndef NATIVE_CLIENT_SRC_TRUSTED_SERVICE_RUNTIME_NACL_DESC_JSCONSOLE_H_
#define NATIVE_CLIENT_SRC_TRUSTED_SERVICE_RUNTIME_NACL_DESC_JSCONSOLE_H_

#include "native_client/src/include/nacl_base.h"
#include "native_client/src/include/portability.h"

#include "native_client/src/trusted/desc/nacl_desc_base.h"

EXTERN_C_BEGIN

struct NaClDescEffector;
struct NaClDescXferState;
struct NaClMessageHeader;
struct NaClRuntimeHostInterface;

/*
 * A NaClDesc subclass that passes Write data to the runtime host
 * via postmessage.
 *
 * This is a DEBUG interface to make it easier to determine the state
 * of a NaCl application.  The interface is enabled only when a debug
 * environment variable is set.
 */
struct NaClDescPostMessage {
  struct NaClDesc base NACL_IS_REFCOUNT_SUBCLASS;

  struct NaClRuntimeHostInterface *runtime_host;
};

int NaClDescPostMessageCtor(struct NaClDescPostMessage      *self,
                            struct NaClRuntimeHostInterface *runtime_host);

EXTERN_C_END

#endif
