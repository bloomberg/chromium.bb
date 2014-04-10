/*
 * Copyright (c) 2013 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "native_client/tests/subprocess/process_lib.h"

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>

// TODO(bsy) get rid of debug printfs

// The header files below access private, not-guaranteed-to-be-stable
// ABIs/APIs.
#include "native_client/src/public/imc_syscalls.h"
#include "native_client/src/public/name_service.h"
#include "native_client/src/public/secure_service.h"
#include "native_client/src/trusted/service_runtime/include/bits/nacl_syscalls.h"
#include "native_client/src/trusted/service_runtime/include/sys/nacl_kernel_service.h"
// get NACL_SYSCALL_BLOCK_SIZE, NACL_SYSCALL_START_ADDR
#include "native_client/src/trusted/service_runtime/nacl_config.h"

namespace NaClProcessLib {

SrpcClientConnection::SrpcClientConnection() : initialized_(false) {}

SrpcClientConnection::~SrpcClientConnection() {
  if (initialized_) {
    close(desc_);
    NaClSrpcDtor(&chan_);
  }
}

bool SrpcClientConnection::InitializeFromConnectedDesc(int desc) {
  if (!NaClSrpcClientCtor(&chan_, desc)) {
    fprintf(stderr,
            "SrpcClientConnection::InitializeFromConnectedDesc failed\n");
    return false;
  }
  desc_ = desc;
  initialized_ = true;
  return true;
}

bool SrpcClientConnection::InitializeFromConnectionCapability(int cap) {
  int conn = imc_connect(cap);
  if (-1 == conn) {
    fprintf(stderr,
            "SrpcClientConnection:InitializeFromConnectionCapability"
            " imc_connect failed, error %d\n", errno);
    return false;
  }
  return InitializeFromConnectedDesc(conn);
}

int NameServiceClient::Resolve(std::string name) {
  NaClSrpcResultCodes result;
  int status;
  int desc = -1;
  if (!initialized()) {
    return -1;
  }
  result = NaClSrpcInvokeBySignature(chan(), NACL_NAME_SERVICE_LOOKUP,
                                     name.c_str(), O_RDONLY,
                                     &status, &desc);
  if (NACL_SRPC_RESULT_OK != result) {
    fprintf(stderr, "Service lookup RPC failed (%d): %s\n", result,
            NaClSrpcErrorString(result));
    return -1;
  }
  if (NACL_NAME_SERVICE_SUCCESS != status) {
    fprintf(stderr, "Resolve failed for \"%s\"; error %d\n", name.c_str(),
            status);
    return -1;
  }

  return desc;
}

int NameServiceFactory::name_service_cap;
NameServiceFactory *NameServiceFactory::singleton = NULL;

NameServiceFactory *NameServiceFactory::NameServiceFactorySingleton() {
  return singleton;
}

NameServiceFactory::NameServiceFactory() {}

typedef int (*nameservice_tramp_t)(int *desc);

bool NameServiceFactory::Init() {
  if (NULL != singleton) {
    return false;
  }
  nameservice_tramp_t nameservice_tramp =
      reinterpret_cast<nameservice_tramp_t>(
          (NACL_SYSCALL_START_ADDR +
           NACL_SYSCALL_BLOCK_SIZE * NACL_sys_nameservice));

  name_service_cap = -1;
  if (-1 == (*nameservice_tramp)(&name_service_cap)) {
    return false;
  }
  singleton = new NameServiceFactory();
  return (NULL != singleton);
}

NameServiceClient *NameServiceFactory::NameService() {
  NameServiceClient *client = new NameServiceClient();
  if (!client->InitializeFromConnectionCapability(name_service_cap)) {
    delete client;
    return NULL;
  }
  return client;
}

bool KernelServiceClient::CreateProcess(int *child_sockaddr,
                                        int *app_sockaddr) {
  if (!initialized()) {
    return 0;
  }
  NaClSrpcResultCodes result;
  int status;
  result = NaClSrpcInvokeBySignature(chan(), NACL_KERNEL_SERVICE_CREATE_PROCESS,
                                     &status, child_sockaddr, app_sockaddr);
  if (NACL_SRPC_RESULT_OK != result) {
    fprintf(stderr, "create process failed (%d): %s\n", result,
            NaClSrpcErrorString(result));
    return 0;
  }
  return 1;
}

ServiceRuntimeClient* KernelServiceClient::ServiceRuntimeClientFactory(
    int child_sockaddr) {
  ServiceRuntimeClient* src = new ServiceRuntimeClient;
  if (!src->InitializeFromConnectionCapability(child_sockaddr)) {
    delete src;
    return NULL;
  }
  return src;
}

NaClErrorCode ServiceRuntimeClient::RunNaClModule(int module_descriptor) {
  NaClSrpcResultCodes result;
  int status;
  if (!initialized()) {
    return LOAD_INTERNAL;
  }
  printf("Entered RunNaClModule\n");
  printf("chan: %p\n", (void*) chan());
  result = NaClSrpcInvokeBySignature(chan(), NACL_SECURE_SERVICE_LOAD_MODULE,
                                     module_descriptor);
  printf("load_module result %d\n", result);
  if (NACL_SRPC_RESULT_OK != result) {
    fprintf(stderr, "load_module RPC failed (%d): %s\n",
            result, NaClSrpcErrorString(result));
    return LOAD_INTERNAL;
  }
  result = NaClSrpcInvokeBySignature(chan(), "start_module::i", &status);
  printf("start_module result %d\n", result);
  if (NACL_SRPC_RESULT_OK != result) {
    fprintf(stderr, "start_module RPC failed (%d): %s\n",
            result, NaClSrpcErrorString(result));
    return LOAD_INTERNAL;
  }
  printf("start_module status: %d\n", status);
  return static_cast<NaClErrorCode>(status);
}

}  // namespace NaClProcessLib
