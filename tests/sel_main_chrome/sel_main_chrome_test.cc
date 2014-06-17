/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <fcntl.h>
#include <cstring>

#if NACL_WINDOWS
# include <io.h>
#endif

#include "native_client/src/public/chrome_main.h"
#include "native_client/src/public/nacl_app.h"
#include "native_client/src/public/nacl_file_info.h"
#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/shared/platform/nacl_threads.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"
#include "native_client/src/trusted/desc/nacl_desc_custom.h"
#include "native_client/src/trusted/desc/nacl_desc_file_info.h"
#include "native_client/src/trusted/nonnacl_util/sel_ldr_launcher.h"
#include "native_client/src/trusted/service_runtime/include/sys/fcntl.h"
#include "native_client/src/trusted/service_runtime/nacl_all_modules.h"
#include "native_client/src/trusted/service_runtime/nacl_config.h"
#include "native_client/src/trusted/service_runtime/nacl_valgrind_hooks.h"
#include "native_client/src/trusted/service_runtime/sel_addrspace.h"
#include "native_client/src/trusted/service_runtime/sel_ldr.h"
#include "native_client/src/trusted/validator/validation_cache.h"

// A global variable that specifies whether the module should be loaded via
// SRPC. Its value is controlled by a command line flag; see
// NaClHandleLoadModuleArg() for where it's set.
bool g_load_module_srpc;

int OpenFileReadOnly(const char *filename) {
#if NACL_WINDOWS
  return _open(filename, _O_RDONLY);
#else
  return open(filename, O_RDONLY);
#endif
}

int32_t OpenFileHandleReadExec(const char *filename) {
#if NACL_WINDOWS
  HANDLE h = CreateFileA(filename,
                         GENERIC_READ | GENERIC_EXECUTE,
                         FILE_SHARE_READ,
                         NULL,
                         OPEN_EXISTING,
                         FILE_ATTRIBUTE_NORMAL,
                         NULL);
  // On Windows, valid handles are 32 bit unsigned integers so this is safe.
  return reinterpret_cast<int32_t>(h);
#else
  return open(filename, O_RDONLY);
#endif
}

// This launcher class does not actually launch a process, but we
// reuse SelLdrLauncherBase in order to use its helper methods.
class DummyLauncher : public nacl::SelLdrLauncherBase {
 public:
  explicit DummyLauncher(NaClHandle channel) {
    channel_ = channel;
  }

  virtual bool Start(const char *url) {
    UNREFERENCED_PARAMETER(url);
    return true;
  }
};

// Fake validation cache methods for testing.
struct TestValidationHandle {
  uint64_t expected_token_lo;
  uint64_t expected_token_hi;
  int32_t expected_file_handle;
  char *expected_file_path;
};

struct TestValidationQuery {
  bool known_to_validate;
};

static void *TestCreateQuery(void *handle) {
  UNREFERENCED_PARAMETER(handle);
  return static_cast<void *>(new TestValidationQuery());
}

static void TestAddData(void *query, const unsigned char *data,
                        size_t length) {
  UNREFERENCED_PARAMETER(query);
  UNREFERENCED_PARAMETER(data);
  UNREFERENCED_PARAMETER(length);
}

static int TestQueryKnownToValidate(void *query) {
  TestValidationQuery *s = static_cast<TestValidationQuery *>(query);
  return s->known_to_validate;
}

static void TestSetKnownToValidate(void *query) {
  TestValidationQuery *s = static_cast<TestValidationQuery *>(query);
  s->known_to_validate = 1;
}

static void TestDestroyQuery(void *query) {
  delete static_cast<TestValidationQuery *>(query);
}

static int TestCachingIsInexpensive(const struct NaClValidationMetadata *m) {
  UNREFERENCED_PARAMETER(m);
  return 1;
}

static int TestResolveFileToken(void *handle, struct NaClFileToken *file_token,
                                int32_t *fd, char **file_path,
                                uint32_t *file_path_length) {
  TestValidationHandle *h = static_cast<TestValidationHandle *>(handle);
  CHECK(h->expected_token_lo == file_token->lo);
  CHECK(h->expected_token_hi == file_token->hi);
  *fd = h->expected_file_handle;
  *file_path = h->expected_file_path;
  *file_path_length = static_cast<uint32_t>(strlen(h->expected_file_path));
  return 1;
}

struct ThreadArgs {
  NaClHandle channel;
  NaClFileInfo file_info;
};

void WINAPI DummyRendererThread(void *thread_arg) {
  struct ThreadArgs *args = (struct ThreadArgs *) thread_arg;

  nacl::DescWrapperFactory desc_wrapper_factory;
  DummyLauncher launcher(args->channel);
  NaClSrpcChannel trusted_channel;
  NaClSrpcChannel untrusted_channel;
  if (g_load_module_srpc) {
    struct NaClDesc *desc = NaClDescIoFromFileInfo(args->file_info,
                                                   NACL_ABI_O_RDONLY);
    CHECK(desc != NULL);
    nacl::DescWrapper *nexe_desc =
        desc_wrapper_factory.MakeGenericCleanup(desc);
    CHECK(nexe_desc != NULL);
    CHECK(launcher.SetupCommandAndLoad(&trusted_channel, nexe_desc));
  } else {
    CHECK(launcher.SetupCommand(&trusted_channel));
  }
  CHECK(launcher.StartModuleAndSetupAppChannel(&trusted_channel,
                                               &untrusted_channel));
}

void ExampleDescDestroy(void *handle) {
  UNREFERENCED_PARAMETER(handle);
}

ssize_t ExampleDescSendMsg(void *handle,
                           const struct NaClImcTypedMsgHdr *msg,
                           int flags) {
  UNREFERENCED_PARAMETER(handle);
  UNREFERENCED_PARAMETER(msg);
  UNREFERENCED_PARAMETER(flags);

  NaClLog(LOG_FATAL, "ExampleDescSendMsg: Not implemented\n");
  return 0;
}

ssize_t ExampleDescRecvMsg(void *handle,
                           struct NaClImcTypedMsgHdr *msg,
                           int flags) {
  UNREFERENCED_PARAMETER(handle);
  UNREFERENCED_PARAMETER(msg);
  UNREFERENCED_PARAMETER(flags);

  NaClLog(LOG_FATAL, "ExampleDescRecvMsg: Not implemented\n");
  return 0;
}

struct NaClDesc *MakeExampleDesc() {
  struct NaClDescCustomFuncs funcs = NACL_DESC_CUSTOM_FUNCS_INITIALIZER;
  funcs.Destroy = ExampleDescDestroy;
  funcs.SendMsg = ExampleDescSendMsg;
  funcs.RecvMsg = ExampleDescRecvMsg;
  return NaClDescMakeCustomDesc(NULL, &funcs);
}

void NaClHandleLoadModuleArg(int *argc_p, char ***argv_p) {
  static const char kNoSrpcLoadModule[] = "--no_srpc_load_module";
  int argc = *argc_p;
  char **argv = *argv_p;
  char *argv0;

  g_load_module_srpc = true;
  CHECK(argc >= 1);

  argv0 = argv[0];
  if (strcmp(argv[1], kNoSrpcLoadModule) == 0) {
    g_load_module_srpc = false;
    --argc;
    ++argv;
  }
  *argc_p = argc;
  *argv_p = argv;
  (*argv_p)[0] = argv0;
}

int main(int argc, char **argv) {
  // Note that we deliberately do not call NaClAllModulesInit() here,
  // in order to mimic what we expect the Chromium side to do.
  NaClChromeMainInit();
  struct NaClChromeMainArgs *args = NaClChromeMainArgsCreate();
  struct NaClApp *nap = NaClAppCreate();
  struct ThreadArgs thread_args;

  NaClHandleBootstrapArgs(&argc, &argv);
  NaClHandleLoadModuleArg(&argc, &argv);
#if NACL_LINUX
  args->prereserved_sandbox_size = g_prereserved_sandbox_size;
#endif

  CHECK(argc == 3 || argc == 4);

  args->irt_fd = OpenFileReadOnly(argv[1]);
  CHECK(args->irt_fd >= 0);

  memset(&thread_args.file_info, 0, sizeof thread_args.file_info);
  thread_args.file_info.desc = OpenFileReadOnly(argv[2]);
  CHECK(thread_args.file_info.desc >= 0);
  NaClFileNameForValgrind(argv[2]);

  NaClHandle socketpair[2];
  CHECK(NaClSocketPair(socketpair) == 0);
  args->imc_bootstrap_handle = socketpair[0];
  thread_args.channel = socketpair[1];

  // Check that NaClDescMakeCustomDesc() works when called in this context.
  NaClAppSetDesc(nap, NACL_CHROME_DESC_BASE, MakeExampleDesc());

  // Set up mock validation cache.
  struct TestValidationHandle test_handle;
  struct NaClValidationCache test_cache;
  if (argc == 4) {
    CHECK(strcmp(argv[3], "-vcache") == 0);
    test_handle.expected_token_lo = 0xabcdef123456789LL;
    test_handle.expected_token_hi = 0x101010101010101LL;
    test_handle.expected_file_handle = OpenFileHandleReadExec(argv[2]);
    test_handle.expected_file_path = strdup(argv[2]);
    test_cache.handle = &test_handle;
    test_cache.CreateQuery = &TestCreateQuery;
    test_cache.AddData = &TestAddData;
    test_cache.QueryKnownToValidate = &TestQueryKnownToValidate;
    test_cache.SetKnownToValidate = &TestSetKnownToValidate;
    test_cache.DestroyQuery = &TestDestroyQuery;
    test_cache.CachingIsInexpensive = &TestCachingIsInexpensive;
    test_cache.ResolveFileToken = &TestResolveFileToken;
    args->validation_cache = &test_cache;
    thread_args.file_info.file_token.lo = test_handle.expected_token_lo;
    thread_args.file_info.file_token.hi = test_handle.expected_token_hi;
  }
  if (!g_load_module_srpc) {
    NaClFileInfo info;
    info.desc = OpenFileHandleReadExec(argv[2]);
#if NACL_WINDOWS
    info.desc = _open_osfhandle(info.desc, _O_RDONLY | _O_BINARY);
#endif
    if (argc == 4) {
      info.file_token.lo = test_handle.expected_token_lo;
      info.file_token.hi = test_handle.expected_token_hi;
    } else {
      info.file_token.lo = 0;
      info.file_token.hi = 0;
    }
    args->nexe_desc = NaClDescIoFromFileInfo(info, NACL_ABI_O_RDONLY);
  }

  NaClThread thread;
  CHECK(NaClThreadCtor(&thread, DummyRendererThread, &thread_args,
                       NACL_KERN_STACK_SIZE));

  NaClChromeMainStartApp(nap, args);
  NaClLog(LOG_FATAL, "NaClChromeMainStartApp() should never return\n");
  return 1;
}
