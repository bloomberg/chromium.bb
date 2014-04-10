// -*- c++ -*-
// Copyright (c) 2013 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NATIVE_CLIENT_TESTS_SUBPROCESS_PROCESS_LIB_H_
#define NATIVE_CLIENT_TESTS_SUBPROCESS_PROCESS_LIB_H_

#include "native_client/src/shared/srpc/nacl_srpc.h"

#include <string>
#include <vector>

// NaClErrorCode
#include "native_client/src/trusted/service_runtime/nacl_error_code.h"

namespace NaClProcessLib {

class SrpcClientConnection {
 public:
  SrpcClientConnection();
  virtual ~SrpcClientConnection();

  // takes ownership of desc
  bool InitializeFromConnectedDesc(int desc);

  // uses cap to derive desc, but caller retains ownership
  bool InitializeFromConnectionCapability(int cap);

  bool initialized() const { return initialized_; }
  NaClSrpcChannel *chan() { return &chan_; }

 private:
  bool initialized_;
  int desc_;
  NaClSrpcChannel chan_;  // POD
};

// Basic name service related utilities.
class NameServiceClient : public SrpcClientConnection {
 public:
  NameServiceClient() {}

  int Resolve(std::string name);
};

class NameServiceFactory {
 public:
  // Init must be called before any other operation.  May fail, which
  // is typically a fatal error.
  static bool Init();

  static NameServiceFactory *NameServiceFactorySingleton();
  NameServiceClient *NameService();
 private:
  NameServiceFactory();

  static int name_service_cap;
  static NameServiceFactory *singleton;
};

class ServiceRuntimeClient : public SrpcClientConnection {
 public:
  ServiceRuntimeClient() {}

  // Returns an NaClErrorCode -- LOAD_OK (0) is okay, non-zero is an error.
  NaClErrorCode RunNaClModule(int module_descriptor);
  // module_descriptor must be a valid descriptor.
  // irt_descriptor may be -1 if no IRT is desired.
};

class KernelServiceClient : public SrpcClientConnection {
 public:
  KernelServiceClient() {}

  // Return true if successful.  When successful, |*child_sockaddr|
  // contains the socket address to the service runtime for the newly
  // created subprocess, and |*app_sockaddr| contains the application
  // (untrusted) socket address.
  bool CreateProcess(int *child_sockaddr, int *app_sockaddr);

  ServiceRuntimeClient* ServiceRuntimeClientFactory(int child_sockaddr);
};

}  // namespace NaClProcessLib

#endif
