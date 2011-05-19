/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "native_client/src/trusted/reverse_service/reverse_socket.h"

#include "native_client/src/include/nacl_scoped_ptr.h"
#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/trusted/desc/nacl_desc_wrapper.h"
#include "native_client/src/trusted/nacl_base/nacl_refcount.h"
#include "native_client/src/trusted/desc/nacl_desc_base.h"
#include "native_client/src/trusted/simple_service/nacl_simple_rservice.h"

namespace nacl {

ReverseSocket::~ReverseSocket() {
  NaClLog(4, "~ReverseSocket, about to delete conn_cap_ 0x%"NACL_PRIxPTR"\n",
          (uintptr_t) conn_cap_);
  delete conn_cap_;
  NaClRefCountUnref(reinterpret_cast<NaClRefCount*>(rev_service_));
  rev_service_ = NULL;
}

bool ReverseSocket::StartService(void* server_instance_data) {
  NaClLog(4, "Entered ReverseSocket::StartService\n");
  nacl::scoped_ptr_malloc<NaClSimpleRevService> rev_service_tmp(
      reinterpret_cast<NaClSimpleRevService*>(
          malloc(sizeof(NaClSimpleRevService))));
  if (NULL == rev_service_tmp.get()) {
    NaClLog(4, "FAILURE: Leaving ReverseSocket::StartService\n");
    return false;
  }
  nacl::scoped_ptr_nacl_refcount<NaClSimpleRevService> ref(
      &rev_service_tmp,
      NaClSimpleRevServiceCtor(rev_service_tmp.get(),
                               NaClDescRef(conn_cap_->desc()),
                               handlers_,
                               thread_factory_fn_,
                               thread_factory_data_));
  if (!ref.constructed()) {
    NaClLog(4, "FAILURE: Leaving ReverseSocket::StartService\n");
    NaClDescUnref(conn_cap_->desc());
    return false;
  }

  rev_service_ = ref.release();

  NaClLog(4, "ReverseSocket::StartService: invoking ConnectAndSpawnHandler\n");
  if (0 != (*NACL_VTBL(NaClSimpleRevService, rev_service_)->
            ConnectAndSpawnHandler)(rev_service_,
                                    server_instance_data)) {
    NaClLog(4, "FAILURE: Leaving ReverseSocket::StartService\n");
    return false;
  }
  // Handler thread has its own reference to rev_service_.get(), so
  // our Dtor unreferencing it doesn't matter.
  NaClLog(4, "Leaving ReverseSocket::StartService\n");
  return true;
}

}  // namespace nacl
