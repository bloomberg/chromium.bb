/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_SRC_TRUSTED_REVERSE_SERVICE_REVERSE_SERVICE_H_
#define NATIVE_CLIENT_SRC_TRUSTED_REVERSE_SERVICE_REVERSE_SERVICE_H_

#include "native_client/src/include/nacl_compiler_annotations.h"
#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/nacl_scoped_ptr.h"
#include "native_client/src/trusted/reverse_service/reverse_socket.h"
#include "native_client/src/shared/platform/refcount_base.h"
#include "native_client/src/shared/platform/nacl_sync.h"

namespace nacl {

class DescWrapper;

class ReverseService : public RefCountBase {
 public:
  explicit ReverseService(nacl::DescWrapper* conn_cap);

  // covariant impl of Ref()
  ReverseService* Ref() {
    return reinterpret_cast<ReverseService*>(RefCountBase::Ref());
  }

  bool Start();

  void WaitForServiceThreadsToExit();

  void IncrThreadCount();
  void DecrThreadCount();

 protected:
  ~ReverseService();

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(ReverseService);

  // We cannot just have the object here, since MSVC generates warning
  // C4355 that 'this' is used in base member initializer list.  Meh.
  // It's a fair warning: 'this' isn't fully constructed yet.
  scoped_ptr<ReverseSocket> service_socket_;

  static NaClSrpcHandlerDesc const handlers[];

  NaClMutex mu_;
  NaClCondVar cv_;
  uint32_t thread_count_;
};

}  // namespace nacl

#endif
