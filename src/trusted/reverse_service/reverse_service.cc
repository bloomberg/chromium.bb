/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#define NACL_LOG_MODULE_NAME "reverse_service"

#include "native_client/src/trusted/reverse_service/reverse_service.h"

#include <string.h>

#include <limits>
#include <string>

#include "native_client/src/include/nacl_compiler_annotations.h"
#include "native_client/src/include/nacl_scoped_ptr.h"
#include "native_client/src/include/portability_io.h"
#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/shared/platform/nacl_host_desc.h"
#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/shared/platform/nacl_sync.h"
#include "native_client/src/shared/platform/nacl_sync_checked.h"
#include "native_client/src/shared/platform/nacl_threads.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"

#include "native_client/src/trusted/desc/nacl_desc_invalid.h"
#include "native_client/src/trusted/desc/nacl_desc_io.h"

#include "native_client/src/trusted/service_runtime/include/sys/fcntl.h"

namespace {

// ReverseInterfaceWrapper wraps a C++ interface and provides
// C-callable wrapper functions for use by the underlying C
// implementation.

struct ReverseInterfaceWrapper {
  NaClReverseInterface base NACL_IS_REFCOUNT_SUBCLASS;
  nacl::ReverseInterface* iface;
};

void StartupInitializationComplete(NaClReverseInterface* self) {
  ReverseInterfaceWrapper* wrapper =
      reinterpret_cast<ReverseInterfaceWrapper*>(self);
  if (NULL == wrapper->iface) {
    NaClLog(1, "StartupInitializationComplete, no reverse_interface.\n");
  } else {
    wrapper->iface->StartupInitializationComplete();
  }
}

void ReportCrash(NaClReverseInterface* self) {
  ReverseInterfaceWrapper* wrapper =
      reinterpret_cast<ReverseInterfaceWrapper*>(self);
  if (NULL == wrapper->iface) {
    NaClLog(1, "ReportCrash, no reverse_interface.\n");
  } else {
    wrapper->iface->ReportCrash();
  }
}

int64_t RequestQuotaForWrite(NaClReverseInterface* self,
                             char const* file_id,
                             int64_t offset,
                             int64_t length) {
  ReverseInterfaceWrapper* wrapper =
      reinterpret_cast<ReverseInterfaceWrapper*>(self);
  if (NULL == wrapper->iface) {
    NaClLog(1, "RequestQuotaForWrite, no reverse_interface.\n");
    return 0;
  }
  return wrapper->iface->RequestQuotaForWrite(
      nacl::string(file_id), offset, length);
}

void ReverseInterfaceWrapperDtor(NaClRefCount* vself) {
  ReverseInterfaceWrapper* self =
      reinterpret_cast<ReverseInterfaceWrapper*>(vself);

  self->iface->Unref();
  self->iface = NULL;

  NACL_VTBL(NaClRefCount, self) = &kNaClRefCountVtbl;
  (*NACL_VTBL(NaClRefCount, self)->Dtor)(vself);
}

static NaClReverseInterfaceVtbl const kReverseInterfaceWrapperVtbl = {
  {
    ReverseInterfaceWrapperDtor,
  },
  StartupInitializationComplete,
  ReportCrash,
  RequestQuotaForWrite,
};

int ReverseInterfaceWrapperCtor(ReverseInterfaceWrapper* self,
                                nacl::ReverseInterface* itf) {
  NaClLog(4,
          "ReverseInterfaceWrapperCtor: self 0x%" NACL_PRIxPTR "\n",
          reinterpret_cast<uintptr_t>(self));
  if (!NaClReverseInterfaceCtor_protected(
        reinterpret_cast<NaClReverseInterface*>(&self->base))) {
    NaClLog(4, "ReverseInterfaceWrapperCtor:"
            " NaClReverseInterfaceCtor_protected failed\n");
    return 0;
  }
  self->iface = itf;

  NACL_VTBL(NaClRefCount, self) =
     reinterpret_cast<NaClRefCountVtbl const*>(&kReverseInterfaceWrapperVtbl);

  NaClLog(4, "VTBL\n");
  NaClLog(4, "Leaving ReverseInterfaceWrapperCtor\n");
  return 1;
}

}  // namespace

namespace nacl {

ReverseService::ReverseService(DescWrapper* conn_cap,
                               ReverseInterface* rif)
  : service_(NULL),
    reverse_interface_(rif) {
  NaClLog(4, "ReverseService::ReverseService ctor invoked\n");

  ReverseInterfaceWrapper* wrapper =
      reinterpret_cast<ReverseInterfaceWrapper*>(malloc(sizeof *wrapper));
  if (NULL == wrapper) {
    NaClLog(LOG_FATAL, "ReverseService::ReverseService: malloc failed\n");
  }
  if (!ReverseInterfaceWrapperCtor(wrapper, rif)) {
    NaClLog(LOG_FATAL, "ReverseService::ReverseService: "
            "ReverseInterfaceWrapperCtor failed\n");
  }

  service_ = reinterpret_cast<NaClReverseService*>(malloc(sizeof *service_));
  if (NULL == service_) {
    NaClLog(LOG_FATAL, "ReverseService::ReverseService: malloc failed\n");
  }
  if (!NaClReverseServiceCtor(service_,
                              reinterpret_cast<NaClReverseInterface*>(wrapper),
                              conn_cap->desc())) {
    NaClLog(LOG_FATAL, "ReverseService::ReverseService: "
            "NaClReverseServiceCtor failed\n");
  }
}

ReverseService::~ReverseService() {
  NaClRefCountUnref(reinterpret_cast<struct NaClRefCount*>(service_));
  service_ = NULL;
}

bool ReverseService::Start(bool crash_report) {
  return NACL_VTBL(NaClReverseService, service_)->Start(
      service_, crash_report);
}

void ReverseService::WaitForServiceThreadsToExit() {
  NACL_VTBL(NaClReverseService, service_)->WaitForServiceThreadsToExit(
      service_);
}

void ReverseService::IncrThreadCount() {
  NACL_VTBL(NaClReverseService, service_)->ThreadCountIncr(service_);
}

void ReverseService::DecrThreadCount() {
  NACL_VTBL(NaClReverseService, service_)->ThreadCountDecr(service_);
}

}  // namespace nacl
