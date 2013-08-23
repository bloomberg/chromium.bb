/*
 * Copyright (c) 2013 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_SRC_TRUSTED_SERVICE_RUNTIME_NACL_RUNTIME_HOST_INTERFACE_H_
#define NATIVE_CLIENT_SRC_TRUSTED_SERVICE_RUNTIME_NACL_RUNTIME_HOST_INTERFACE_H_

#include "native_client/src/include/nacl_base.h"
#include "native_client/src/include/nacl_macros.h"

#include "native_client/src/trusted/desc/nacl_desc_base.h"
#include "native_client/src/trusted/nacl_base/nacl_refcount.h"

EXTERN_C_BEGIN

struct NaClRuntimeHostInterface {
  struct NaClRefCount base NACL_IS_REFCOUNT_SUBCLASS;
};

struct NaClRuntimeHostInterfaceVtbl {
  struct NaClRefCountVtbl       vbase;

  int                           (*Log)(
      struct NaClRuntimeHostInterface *self,
      char const                      *message);

  int                           (*StartupInitializationComplete)(
      struct NaClRuntimeHostInterface *self);

  int                           (*ReportExitStatus)(
      struct NaClRuntimeHostInterface *self,
      int                             exit_status);

  ssize_t                       (*PostMessage)(
      struct NaClRuntimeHostInterface *self,
      char const                      *message,
      size_t                          message_bytes);

  int                           (*CreateProcess)(
      struct NaClRuntimeHostInterface *self,
      struct NaClDesc                 **out_sock_addr,
      struct NaClDesc                 **out_app_addr);
};

/*
 * The protected Ctor is intended for use by subclasses of
 * NaClRuntimeHostInterface.
 */
int NaClRuntimeHostInterfaceCtor_protected(
    struct NaClRuntimeHostInterface *self);

void NaClRuntimeHostInterfaceDtor(struct NaClRefCount *vself);

int NaClRuntimeHostInterfaceLogNotImplemented(
    struct NaClRuntimeHostInterface *self,
    char const                      *message);

int NaClRuntimeHostInterfaceStartupInitializationCompleteNotImplemented(
    struct NaClRuntimeHostInterface *self);

int NaClRuntimeHostInterfaceReportExitStatusNotImplemented(
    struct NaClRuntimeHostInterface *self,
    int                             exit_status);

ssize_t NaClRuntimeHostInterfacePostMessageNotImplemented(
    struct NaClRuntimeHostInterface *self,
    char const                      *message,
    size_t                          message_bytes);

int NaClRuntimeHostInterfaceCreateProcessNotImplemented(
    struct NaClRuntimeHostInterface *self,
    struct NaClDesc                 **out_sock_addr,
    struct NaClDesc                 **out_app_addr);

extern struct NaClRuntimeHostInterfaceVtbl const kNaClRuntimeHostInterfaceVtbl;

EXTERN_C_END

#endif /* NATIVE_CLIENT_SRC_TRUSTED_SERVICE_RUNTIME_NACL_RUNTIME_HOST_INTERFACE_H_ */
