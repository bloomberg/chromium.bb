/*
 * Copyright (c) 2013 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "native_client/src/trusted/service_runtime/load_file.h"

#include "native_client/src/shared/gio/gio.h"
#include "native_client/src/trusted/service_runtime/nacl_valgrind_hooks.h"
#include "native_client/src/trusted/service_runtime/sel_ldr.h"


NaClErrorCode NaClAppLoadFileFromFilename(struct NaClApp *nap,
                                          const char *filename) {
  struct GioMemoryFileSnapshot gio_file;
  struct Gio *gio;
  NaClErrorCode err;

  NaClFileNameForValgrind(filename);

  if (!GioMemoryFileSnapshotCtor(&gio_file, filename)) {
    return LOAD_OPEN_ERROR;
  }
  gio = &gio_file.base.base;
  err = NaClAppLoadFile(gio, nap);
  if (err != LOAD_OK) {
    return err;
  }
  if (gio->vtbl->Close(gio) != 0) {
    err = LOAD_INTERNAL;
  }
  gio->vtbl->Dtor(gio);
  return err;
}
