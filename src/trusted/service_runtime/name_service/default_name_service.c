/*
 * Copyright 2011 The Native Client Authors.  All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include "native_client/src/trusted/service_runtime/name_service/default_name_service.h"

#include "native_client/src/trusted/desc/nacl_desc_rng.h"

int NaClDefaultNameServiceInit(struct NaClNameService *ns) {
  /*
   * Create an CSPRNG and enter it into the name server.
   */
  struct NaClDescRng *rng = NULL;

  rng = (struct NaClDescRng *) malloc(sizeof *rng);
  if (NULL == rng) {
    goto malloc_failed;
  }
  if (!NaClDescRngCtor(rng)) {
    goto rng_ctor_failed;
  }

  /*
   * It may appear desirable to insert a factory for rng, so there can
   * be per-thread secure rng access.  However, note that the only way
   * we "transfer" a RNG is to create a new (but indistinguishable)
   * RNG at the recipient, so each lookup results in a new generator
   * anyway.
   */
  (*NACL_VTBL(NaClNameService, ns)->
   CreateDescEntry)(ns, "SecureRandom", (struct NaClDesc *) rng);
  NaClDescUnref((struct NaClDesc *) rng);

  /* start the service! */
  NaClNameServiceLaunch(ns);

  return 1;

 rng_ctor_failed:
  free(rng);
 malloc_failed:
  return 0;
}
