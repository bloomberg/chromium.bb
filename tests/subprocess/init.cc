// Copyright (c) 2013 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The "init" process that has an interface to the reverse service.

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include <errno.h>
#include <pthread.h>
#include <string.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "native_client/src/include/nacl_scoped_ptr.h"
#include "native_client/src/public/imc_syscalls.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"
#include "native_client/src/untrusted/irt/irt.h"

#include "native_client/tests/subprocess/scoped_lock.h"
#include "native_client/tests/subprocess/scoped_non_ptr.h"
#include "native_client/tests/subprocess/process_lib.h"

// This is not a Pepper module, so we do not have to leave the main
// thread available/waiting as an event/interrupt handler.  However,
// SRPC-based NaCl modules under sel_universal act in much the same
// way: there should be a thread which accepts (at least) a connection
// from sel_universal and handle RPC requests from it.  It is up to
// the application whether it wants to accept more than one
// connection.

NaClProcessLib::NameServiceFactory *g_name_service_factory = NULL;

void TestGlobalSetup() {
  if (!NaClProcessLib::NameServiceFactory::Init()) {
    fprintf(stderr, "NaClProcessLib::NameServiceFactory::Init failed\n");
    abort();
  }
  g_name_service_factory =
      NaClProcessLib::NameServiceFactory::NameServiceFactorySingleton();
}

// Each connection can be stateful and that state is tracked by this
// object.
class SrpcHandlerState {
 public:
  explicit SrpcHandlerState(int desc)
      : desc_(desc) {}
  void Run();
 private:
  static void StartTestHandler(struct NaClSrpcRpc *rpc,
                               struct NaClSrpcArg *args[],
                               struct NaClSrpcArg *rets[],
                               struct NaClSrpcClosure *done);
  static void TestStatusHandler(struct NaClSrpcRpc *rpc,
                                struct NaClSrpcArg *args[],
                                struct NaClSrpcArg *rets[],
                                struct NaClSrpcClosure *done);

  int desc_;

  static struct NaClSrpcHandlerDesc const table[];
};

// Pretend we are like Pepper and we cannot block the main thread --
// instead, we spawn a thread on which we do file operatons.  We could
// return an identifier for the test/thread, so that the caller can
// poll for completion; instead, we keep it simple and require that
// there is at most one test in progress at any time.
class TestState {
 public:
  TestState();
  ~TestState();
  void MarkDone(int result);
  int WaitUntilDone();
  void Run();
 private:
  pthread_mutex_t mu_;
  pthread_cond_t cv_;

  bool done_;
  int result_;

  int sr_cap_;
  int app_cap_;
};

TestState::TestState()
    : done_(false)
    , result_(0)
    , sr_cap_(-1)
    , app_cap_(-1) {
  if (0 != pthread_mutex_init(&mu_,
                              static_cast<pthread_mutexattr_t*>(NULL))) {
    fprintf(stderr, "TestState mutex init\n");
    abort();
  }
  if (0 != pthread_cond_init(&cv_, static_cast<pthread_condattr_t*>(NULL))) {
    fprintf(stderr, "TestState cond init\n");
    abort();
  }
}

TestState::~TestState() {
  pthread_mutex_destroy(&mu_);
  pthread_cond_destroy(&cv_);
  if (sr_cap_ != -1) {
    close(sr_cap_);
  }
  if (app_cap_ != -1) {
    close(app_cap_);
  }
}

void TestState::MarkDone(int result) {
  MutexLocker hold(&mu_);
  done_ = true;
  result_ = result;
  pthread_cond_broadcast(&cv_);
}

int TestState::WaitUntilDone() {
  MutexLocker hold(&mu_);
  while (!done_) {
    pthread_cond_wait(&cv_, &mu_);
  }
  return result_;
}

namespace {
void close_desc(int desc) { (void) close(desc); }
}

void TestState::Run() {
  nacl::scoped_ptr<NaClProcessLib::NameServiceClient> name_service(
      g_name_service_factory->NameService());
  ScopedNonPtr<int, close_desc> ks_cap(name_service->Resolve("KernelService"));
  if (-1 == ks_cap) {
    fprintf(stderr, "No Kernel Service!?!\n");
    MarkDone(1);
    return;
  }
  NaClProcessLib::KernelServiceClient ks;
  ks.InitializeFromConnectionCapability(ks_cap);
  if (!ks.CreateProcess(&sr_cap_, &app_cap_)) {
    MarkDone(2);
    return;
  }

  nacl::scoped_ptr<NaClProcessLib::ServiceRuntimeClient> sr(
      ks.ServiceRuntimeClientFactory(sr_cap_));

  ScopedNonPtr<int, close_desc> manifest_desc(
      name_service->Resolve("ManifestNameService"));
  if (-1 == manifest_desc) {
    fprintf(stderr, "No manifest name service?!?\n");
    MarkDone(3);
    return;
  }

  NaClProcessLib::NameServiceClient manifest_service;
  if (!manifest_service.InitializeFromConnectionCapability(manifest_desc)) {
    fprintf(stderr, "Could not connect to manifest name service\n");
    MarkDone(4);
    return;
  }

  ScopedNonPtr<int, close_desc> subproc_desc(
      manifest_service.Resolve("subprogram"));
  if (-1 == subproc_desc) {
    fprintf(stderr,
            "Could not resolve 'subprogram' in manifest name service\n");
    MarkDone(5);
    return;
  }

  NaClErrorCode nec;
  if (LOAD_OK != (nec = sr->RunNaClModule(subproc_desc))) {
    fprintf(stderr,
            "Error while loading program: NaCl error code %d\n",
            nec);
    MarkDone(6);
    return;
  }

  NaClProcessLib::SrpcClientConnection app_conn;
  if (!app_conn.InitializeFromConnectionCapability(app_cap_)) {
    fprintf(stderr, "Could not connect to subprogram\n");
    MarkDone(7);
    return;
  }
  NaClSrpcResultCodes app_result =
      NaClSrpcInvokeBySignature(app_conn.chan(), "hello::");
  if (NACL_SRPC_RESULT_OK != app_result) {
    fprintf(stderr, "Hello RPC to subprogram failed\n");
    MarkDone(8);
    return;
  }

  close(sr_cap_); sr_cap_ = -1;
  close(app_cap_); app_cap_ = -1;
  MarkDone(0);  // success
}

pthread_mutex_t test_mu = PTHREAD_MUTEX_INITIALIZER;
TestState *cur_test = NULL;

void *test_func(void *thr_state) {
  TestState *state = reinterpret_cast<TestState*>(thr_state);

  state->Run();
  return NULL;
}

struct NaClSrpcHandlerDesc const SrpcHandlerState::table[] = {
  { "start_test::i", StartTestHandler, },
  { "test_status::ii", TestStatusHandler, },
  { static_cast<char const *>(NULL), static_cast<NaClSrpcMethod>(NULL), },
};

void SrpcHandlerState::StartTestHandler(struct NaClSrpcRpc *rpc,
                                        struct NaClSrpcArg *args[],
                                        struct NaClSrpcArg *rets[],
                                        struct NaClSrpcClosure *done) {
  NaClSrpcClosureRunner on_scope_exit(done);
  rpc->result = NACL_SRPC_RESULT_OK;

  {
    MutexLocker hold(&test_mu);
    if (NULL != cur_test) {
      rets[0]->u.ival = 0;  // fail; test already in progress
      return;
    }
    cur_test = new TestState();

    pthread_t tid;
    if (0 !=
        pthread_create(&tid, static_cast<pthread_attr_t*>(NULL),
                       test_func, cur_test)) {
      delete cur_test;
      cur_test = NULL;
      rets[0]->u.ival = 0;
      return;
    }
    (void) pthread_detach(tid);
  }
  rets[0]->u.ival = 1;  // successfully launched thread
}

void SrpcHandlerState::TestStatusHandler(struct NaClSrpcRpc *rpc,
                                         struct NaClSrpcArg *args[],
                                         struct NaClSrpcArg *rets[],
                                         struct NaClSrpcClosure *done) {
  NaClSrpcClosureRunner on_scope_exit(done);
  rpc->result = NACL_SRPC_RESULT_OK;

  int result;
  {
    MutexLocker hold(&test_mu);
    if (NULL == cur_test) {
      rets[0]->u.ival = 0;  // bool: fail; no test in progress
      return;
    }
    result = cur_test->WaitUntilDone();
    delete cur_test;
    cur_test = NULL;
  }

  rets[0]->u.ival = 1;  // bool: successfully launched thread
  rets[1]->u.ival = result;  // int: test result code
}

void SrpcHandlerState::Run() {
  if (!NaClSrpcServerLoop(desc_, table, NULL)) {
    fprintf(stderr, "NaClSrpcServerLoop failed\n");
    abort();
  }
  if (-1 == close(desc_)) {
    perror("close RPC connection");
    abort();
  }
}

void *RpcHandler(void *thread_state) {
  int desc = reinterpret_cast<int>(thread_state);
  SrpcHandlerState handler(desc);

  handler.Run();
  return (void *) NULL;
}

int main(int ac, char **av) {
  int d;

  NaClSrpcModuleInit();
  TestGlobalSetup();

  while ((d = imc_accept(3)) != -1) {
    // Spawn a thread process RPCs on d.
    pthread_t thr;
    if (0 != pthread_create(&thr, static_cast<pthread_attr_t *>(NULL),
                            RpcHandler, reinterpret_cast<void *>(d))) {
      perror("pthread_create for RpcHandler");
      abort();
    }
  }

  return 0;
}
