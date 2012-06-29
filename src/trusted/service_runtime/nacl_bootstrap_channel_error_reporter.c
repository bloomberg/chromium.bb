/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <stdio.h>

#include "native_client/src/trusted/service_runtime/nacl_bootstrap_channel_error_reporter.h"

#include "native_client/src/include/portability.h"
#include "native_client/src/include/portability_io.h"
#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/shared/platform/nacl_sync.h"
#include "native_client/src/shared/platform/nacl_sync_checked.h"
#include "native_client/src/trusted/desc/nrd_xfer.h"
#include "native_client/src/trusted/service_runtime/sel_ldr.h"


static struct NaClMutex g_nacl_bootstrap_mu;

void NaClBootstrapChannelErrorReporterInit(void) {
  NaClXMutexCtor(&g_nacl_bootstrap_mu);
}

/*
 * Logging hooks to get recent output up to and including that of the
 * LOG_FATAL that triggered this callback to the plugin and on-stack
 * for breakpad.
 *
 * NB: the total amount sent should not be greater than what can be
 * buffered in kernel buffers (typically 4K), since the plugin does
 * not necessarily have a thread blocked reading the data (we may add
 * one later).  If the write blocks, then the abort-detection
 * machinery in the plugin cannot kick in, since that relies on
 * detecting communication channels (e.g., for reverse service)
 * closing as a side-effect of the service runtime exiting.
 *
 * It is safe to send blindly an amount less than what can be buffered
 * by the kernel, since even if we are running with a version of the
 * plugin that ignores the data, we won't block.
 */
void NaClBootstrapChannelErrorReporter(void *state,
                                       char *buf,
                                       size_t buf_bytes) {
  static int handling_error = 0;
  int horrible_recursive_error;
  struct NaClApp *nap = (struct NaClApp *) state;
  struct NaClDesc *channel = NULL;
  ssize_t rv;
  struct NaClImcTypedMsgHdr hdr;
  struct NaClImcMsgIoVec iov[1];

  /*
   * NB: if any of the code below generates a NaClLog(LOG_FATAL, ...),
   * we would be called recursively, possibly ad infinitum/nauseum.
   * Try to prevent this by using a static boolean state variable to
   * detect it and return quickly.  We cannot take the nap->mu lock,
   * since LOG_FATALs can occur while holding that lock and thus
   * result in a deadlock.  Luckily, the bootstrap_channel member is
   * read-only before we go multi-threaded.
   */
  NaClXMutexLock(&g_nacl_bootstrap_mu);
  horrible_recursive_error = handling_error;  /* copy while holding lock */
  handling_error = 1;
  NaClXMutexUnlock(&g_nacl_bootstrap_mu);

  if (horrible_recursive_error) {
    return;
  }
  if (NULL != nap->bootstrap_channel) {
    channel = nap->bootstrap_channel;

    iov[0].base = buf;
    iov[0].length = buf_bytes;
    hdr.iov = iov;
    hdr.iov_length = NACL_ARRAY_SIZE(iov);
    hdr.ndescv = NULL;
    hdr.ndesc_length = 0;
    hdr.flags = 0;

    rv = (*NACL_VTBL(NaClDesc, channel)->SendMsg)(channel, &hdr, 0);
    if (rv < 0 || (size_t) rv != buf_bytes) {
      fprintf(stderr,
              ("NaClBootstrapChannelErrorReporter: SendMsg returned %"
               NACL_PRIdS", expected %"NACL_PRIuS".\n"),
              rv, buf_bytes);
    }
  }
  /* done / give up */
}
