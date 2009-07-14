/*
 * Copyright 2008, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * NRD xfer effector for trusted code use.
 */

#ifndef NATIVE_CLIENT_SRC_TRUSTED_DESC_NRD_XFER_EFFECTOR_H_
#define NATIVE_CLIENT_SRC_TRUSTED_DESC_NRD_XFER_EFFECTOR_H_

#include "native_client/src/include/nacl_base.h"

#include "native_client/src/trusted/desc/nacl_desc_base.h"
#include "native_client/src/trusted/desc/nacl_desc_effector.h"

EXTERN_C_BEGIN

struct NaClNrdXferEffector {
  struct NaClDescEffector base;
  struct NaClDesc         *src_desc;  /* should be a NaClDescImcBoundDesc */
  struct NaClDesc         *out_desc;  /* one returned desc at a time */
};

/* bool: returns non-zero for success, ups refcount on src_desc */
int NaClNrdXferEffectorCtor(struct NaClNrdXferEffector  *self,
                            struct NaClDesc             *src_desc);

/*
 * yields returned NaClDesc, if any, passing ownership.
 */
struct NaClDesc *NaClNrdXferEffectorTakeDesc(struct NaClNrdXferEffector *self);



EXTERN_C_END

#endif /* NATIVE_CLIENT_SRC_TRUSTED_DESC_NRD_XFER_EFFECTOR_H_ */
