/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

/*
 * NaCl run time.
 */
#include <fcntl.h>
#include <stdlib.h>

#include "native_client/src/shared/platform/nacl_sync_checked.h"

#include "native_client/src/trusted/desc/nacl_desc_io.h"

#include "native_client/src/trusted/service_runtime/sel_ldr.h"
#include "native_client/src/trusted/service_runtime/arch/arm/sel_rt.h"

/*
 * Allocate ldt for app, without creating the main thread.
 */
NaClErrorCode NaClAppPrepareToLaunch(struct NaClApp     *nap,
                                     int                in_desc,
                                     int                out_desc,
                                     int                err_desc) {
  int                 i;
  struct NaClHostDesc *nhdp;

  int                 descs[3];

  NaClErrorCode       retval = LOAD_INTERNAL;

  descs[0] = in_desc;
  descs[1] = out_desc;
  descs[2] = err_desc;

  NaClXMutexLock(&nap->mu);

  /*
   * We dup the stdin, stdout, and stderr descriptors and wrap them in
   * NaClHostDesc objects.  Those in turn are wrapped by the
   * NaClDescIoDesc subclass of NaClDesc, and then put into the
   * open-file table.  NaCl app I/O operations will use these shared
   * descriptors, and if they close one of these descriptors it will
   * only be a duplicated descriptor.  NB: some fcntl/ioctl flags
   * apply to the descriptor (e.g., O_CLOEXEC) and some apply to the
   * underlying open file entry (e.g., O_NONBLOCK), so changes by the
   * NaCl app could affect the service runtime.
   */
  for (i = 0; i < 3; ++i) {
    nhdp = malloc(sizeof *nhdp);
    if (NULL == nhdp) {
      NaClLog(LOG_FATAL,
              "NaClAppPrepareToLaunch: no memory for abstract descriptor %d\n",
              i);
    }
    NaClHostDescPosixDup(nhdp, descs[i], (0 == i) ? O_RDONLY : O_WRONLY);
    NaClSetDesc(nap, i, (struct NaClDesc *) NaClDescIoDescMake(nhdp));
  }
  retval = LOAD_OK;

  NaClXMutexUnlock(&nap->mu);
  return retval;
}
