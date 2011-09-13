/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_SRC_TRUSTED_REVERSE_SERVICE_REVERSE_SERVICE_H_
#define NATIVE_CLIENT_SRC_TRUSTED_REVERSE_SERVICE_REVERSE_SERVICE_H_

#include <set>

#include "native_client/src/include/nacl_compiler_annotations.h"
#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/nacl_scoped_ptr.h"
#include "native_client/src/include/nacl_string.h"
#include "native_client/src/trusted/reverse_service/manifest_rpc.h"
#include "native_client/src/trusted/reverse_service/reverse_control_rpc.h"
#include "native_client/src/trusted/reverse_service/reverse_socket.h"
#include "native_client/src/trusted/service_runtime/include/sys/nacl_name_service.h"
#include "native_client/src/shared/platform/refcount_base.h"
#include "native_client/src/shared/platform/nacl_sync.h"

namespace nacl {

class DescWrapper;

class ReverseInterface : public RefCountBase {
 public:
  virtual ~ReverseInterface() {}

  // debugging, messaging
  virtual void Log(nacl::string message) = 0;

  // Startup handshake
  virtual void StartupInitializationComplete() = 0;

  // Name service use.
  //
  // Some of these functions require that the actual operation be done
  // in a different thread, so that the implementation of the
  // interface will have to block the requesting thread.  However, on
  // surf away, the thread switch may get cancelled, and the
  // implementation will have to reply with a failure indication.

  // The bool functions returns false if the service thread unblocked
  // because of surf-away, shutdown, or other issues.  The plugin,
  // when it tells sel_ldr to shut down, will also signal all threads
  // that are waiting for main thread callbacks to wake up and abandon
  // their vigil after the callbacks are all cancelled (by abandoning
  // the WeakRefAnchor or by bombing their CompletionCallbackFactory).
  // Since shutdown/surfaway is the only admissible error, we use bool
  // as the return type.
  virtual bool EnumerateManifestKeys(std::set<nacl::string>* keys) = 0;
  virtual bool OpenManifestEntry(nacl::string url_key, int32_t* out_desc) = 0;
  virtual bool CloseManifestEntry(int32_t desc) = 0;
  virtual void ReportCrash() {}  // = 0;

  // covariant impl of Ref()
  ReverseInterface* Ref() {  // down_cast
    return reinterpret_cast<ReverseInterface*>(RefCountBase::Ref());
  }
};

class ReverseService : public RefCountBase {
 public:
  ReverseService(nacl::DescWrapper* conn_cap, ReverseInterface* rif);

  // covariant impl of Ref()
  ReverseService* Ref() {  // down_cast
    return reinterpret_cast<ReverseService*>(RefCountBase::Ref());
  }

  bool Start();

  void WaitForServiceThreadsToExit();

  void IncrThreadCount();
  void DecrThreadCount();

  ReverseInterface* reverse_interface() const { return reverse_interface_; }

 protected:
  ~ReverseService();

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(ReverseService);

  // We cannot just have the object here, since MSVC generates warning
  // C4355 that 'this' is used in base member initializer list.  Meh.
  // It's a fair warning: 'this' isn't fully constructed yet.
  scoped_ptr<ReverseSocket> service_socket_;
  ReverseInterface* reverse_interface_;

  static NaClSrpcHandlerDesc const handlers[];

  NaClMutex mu_;
  NaClCondVar cv_;
  uint32_t thread_count_;
};

}  // namespace nacl

#endif
