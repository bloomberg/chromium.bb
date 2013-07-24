/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

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
#include "native_client/src/trusted/validator/nacl_file_info.h"

namespace {

// ReverseInterfaceWrapper wraps a C++ interface and provides
// C-callable wrapper functions for use by the underlying C
// implementation.

struct ReverseInterfaceWrapper {
  NaClReverseInterface base NACL_IS_REFCOUNT_SUBCLASS;
  nacl::ReverseInterface* iface;
};

void RevLog(NaClReverseInterface* self, char const* message) {
  ReverseInterfaceWrapper* wrapper =
      reinterpret_cast<ReverseInterfaceWrapper*>(self);
  if (NULL == wrapper->iface) {
    NaClLog(1, "Log, no reverse_interface. Message: %s\n", message);
  } else {
    wrapper->iface->Log(message);
  }
}

void StartupInitializationComplete(NaClReverseInterface* self) {
  ReverseInterfaceWrapper* wrapper =
      reinterpret_cast<ReverseInterfaceWrapper*>(self);
  if (NULL == wrapper->iface) {
    NaClLog(1, "StartupInitializationComplete, no reverse_interface.\n");
  } else {
    wrapper->iface->StartupInitializationComplete();
  }
}

size_t EnumerateManifestKeys(NaClReverseInterface* self,
                             char* buffer,
                             size_t buffer_bytes) {
  ReverseInterfaceWrapper* wrapper =
      reinterpret_cast<ReverseInterfaceWrapper*>(self);
  if (NULL == wrapper->iface) {
    NaClLog(1, "EnumerateManifestKeys, no reverse_interface.\n");
    return 0;
  }

  std::set<nacl::string> manifest_keys;
  if (!wrapper->iface->EnumerateManifestKeys(&manifest_keys)) {
    NaClLog(LOG_WARNING, "EnumerateManifestKeys failed\n");
    return 0;
  }

  size_t size = 0;
  for (std::set<nacl::string>::iterator it = manifest_keys.begin();
       it != manifest_keys.end();
       ++it) {
    if (size >= buffer_bytes) {
      size += it->size() + 1;
      continue;
    }

    size_t to_write = buffer_bytes - size;
    if (it->size() + 1 < to_write) {
      to_write = it->size() + 1;
    } else {
      NaClLog(3,
              "EnumerateManifestKeys: truncating entry %s\n", it->c_str());
    }
    strncpy(buffer + size, it->c_str(), to_write);
    NaClLog(3, "EnumerateManifestKeys: %.*s\n", (int) to_write, buffer + size);
    size += to_write;
  }
  return size;
}

int OpenManifestEntry(NaClReverseInterface* self,
                      char const* url_key,
                      struct NaClFileInfo* info) {
  ReverseInterfaceWrapper* wrapper =
      reinterpret_cast<ReverseInterfaceWrapper*>(self);
  if (NULL == wrapper->iface) {
    NaClLog(1, "OpenManifestEntry, no reverse_interface.\n");
    return 0;
  }
  return wrapper->iface->OpenManifestEntry(nacl::string(url_key), info);
}

int CloseManifestEntry(NaClReverseInterface* self,
                       int32_t desc) {
  ReverseInterfaceWrapper* wrapper =
      reinterpret_cast<ReverseInterfaceWrapper*>(self);
  if (NULL == wrapper->iface) {
    NaClLog(1, "CloseManifestEntry, no reverse_interface.\n");
    return 0;
  }
  return wrapper->iface->CloseManifestEntry(desc);
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

void ReportExitStatus(NaClReverseInterface* self,
                      int exit_status) {
  ReverseInterfaceWrapper* wrapper =
      reinterpret_cast<ReverseInterfaceWrapper*>(self);
  if (NULL == wrapper->iface) {
    NaClLog(1, "ReportExitStatus, no reverse_interface.\n");
  } else {
    wrapper->iface->ReportExitStatus(exit_status);
  }
}

void DoPostMessage(NaClReverseInterface* self,
                   char const* message,
                   size_t message_bytes) {
  ReverseInterfaceWrapper* wrapper =
      reinterpret_cast<ReverseInterfaceWrapper*>(self);
  if (NULL == wrapper->iface) {
    NaClLog(1, "DoPostMessage, no reverse_interface.\n");
  } else {
    wrapper->iface->DoPostMessage(nacl::string(message, message_bytes));
  }
}

int CreateProcess(NaClReverseInterface* self,
                  NaClDesc** out_sock_addr,
                  NaClDesc** out_app_addr) {
  ReverseInterfaceWrapper* wrapper =
      reinterpret_cast<ReverseInterfaceWrapper*>(self);
  if (NULL == wrapper->iface) {
    NaClLog(1, "CreateProcess, no reverse_interface.\n");
    return -NACL_ABI_EAGAIN;
  }

  int status;
  nacl::DescWrapper* sock_addr;
  nacl::DescWrapper* app_addr;
  if (0 == (status = wrapper->iface->CreateProcess(&sock_addr, &app_addr))) {
    *out_sock_addr = sock_addr->desc();
    *out_app_addr = app_addr->desc();
  }
  return status;
}

class CreateProcessFunctorBinder : public nacl::CreateProcessFunctorInterface {
 public:
  CreateProcessFunctorBinder(void (*functor)(void* functor_state,
                                             NaClDesc* out_sock_addr,
                                             NaClDesc* out_app_addr,
                                             int32_t out_pid_or_errno),
                             void* functor_state)
      : functor_(functor), state_(functor_state) {}

  virtual void Results(nacl::DescWrapper* out_sock_addr,
                       nacl::DescWrapper* out_app_addr,
                       int32_t out_pid_or_errno) {
    functor_(state_, out_sock_addr->desc(), out_app_addr->desc(),
             out_pid_or_errno);
  }
 private:
  void (*functor_)(void*, NaClDesc*, NaClDesc*, int32_t);
  void* state_;
};

void CreateProcessFunctorResult(NaClReverseInterface* self,
                                void (*functor)(void* functor_state,
                                                NaClDesc* out_sock_addr,
                                                NaClDesc* out_app_addr,
                                                int32_t out_pid_or_errno),
                                void *functor_state) {
  ReverseInterfaceWrapper* wrapper =
      reinterpret_cast<ReverseInterfaceWrapper*>(self);

  CreateProcessFunctorBinder callback(functor, functor_state);
  wrapper->iface->CreateProcessFunctorResult(&callback);
}

void FinalizeProcess(NaClReverseInterface* self,
                     int32_t pid) {
  ReverseInterfaceWrapper* wrapper =
      reinterpret_cast<ReverseInterfaceWrapper*>(self);

  wrapper->iface->FinalizeProcess(pid);
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
  RevLog,
  StartupInitializationComplete,
  EnumerateManifestKeys,
  OpenManifestEntry,
  CloseManifestEntry,
  ReportCrash,
  ReportExitStatus,
  DoPostMessage,
  CreateProcess,
  CreateProcessFunctorResult,
  FinalizeProcess,
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

// TODO(ncbray) remove
bool ReverseInterface::OpenManifestEntry(nacl::string url_key,
                                         int32_t* out_desc) {
  UNREFERENCED_PARAMETER(url_key);
  *out_desc = -1;
  return false;
}

// TODO(ncbray) convert to a pure virtual.
bool ReverseInterface::OpenManifestEntry(nacl::string url_key,
                                         struct NaClFileInfo* info) {
  memset(info, 0, sizeof(*info));
  return OpenManifestEntry(url_key, &info->desc);
}

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
