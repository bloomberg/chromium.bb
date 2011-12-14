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

/*
 * A NaClDesc subclass that passes Write data to the browser via
 * postmessage.  Objects of this class should be instantiated only if
 * the NaClApp object's reverse channel is available.
 *
 * This is a DEBUG interface to make it easier to determine the state
 * of a NaCl application.  The interface is enabled only when a debug
 * environment variable is set.
 */
struct NaClDescPostMessage {
  struct NaClDesc base NACL_IS_REFCOUNT_SUBCLASS;

  /*
   * There is not much state associated with sending a write buffer to
   * the plugin via RPC -- just behavior changes in the virtual Write
   * function; all state is in the NaClApp object.  it is assumed that
   * the lifetime of the pointed-to NaClApp object is at least that of
   * this NaClDescPostMessage object.
   */
  struct NaClApp  *nap;
  ssize_t         error;
};

int NaClDescPostMessageCtor(struct NaClDescPostMessage  *self,
                            struct NaClApp              *nap);



EXTERN_C_END

#endif
