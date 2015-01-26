/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <fcntl.h>
#include <cstring>

#include "native_client/src/include/build_config.h"

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
// SRPC. Its value is controlled by a command line flag kNoSrpcLoadModule.
bool g_load_module_srpc = true;
const char kNoSrpcLoadModule[] = "--no_srpc_load_module";

// A global variable that specifies whether or not the test should set
// the irt_load_optional flag in NaClChromeMainArgs.
bool g_irt_load_optional = false;
const char kIrtLoadOptional[] = "--irt_load_optional";

// A global variable that specifies whether or not to test validation
// caching of the main nexe.
bool g_test_validation_cache = false;
const char kTestValidationCache[] = "--test_validation_cache";

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

// Process commandline options and return the index where non-option
// commandline arguments begin. Assumes all options are in the front.
int NaClHandleArguments(int argc, char **argv) {
  static const struct TestArguments {
    const char *flag_name;
    bool *flag_reference;
    bool value_to_set;
  } long_opts[] = {
    {kNoSrpcLoadModule, &g_load_module_srpc, false},
    {kIrtLoadOptional, &g_irt_load_optional, true},
    {kTestValidationCache, &g_test_validation_cache, true},
    {NULL, NULL, false}
  };
  int cur_arg = 1;
  while (cur_arg < argc) {
    int i = 0;
    bool matched = false;
    while (long_opts[i].flag_name != NULL) {
      if (strcmp(long_opts[i].flag_name, argv[cur_arg]) == 0) {
        *long_opts[i].flag_reference = long_opts[i].value_to_set;
        ++cur_arg;
        matched = true;
        break;
      }
      ++i;
    }
    if (!matched) {
      return cur_arg;
    }
  }
  return cur_arg;
}

int main(int argc, char **argv) {
  // Note that we deliberately do not call NaClAllModulesInit() here,
  // in order to mimic what we expect the Chromium side to do.
  NaClChromeMainInit();
  struct NaClChromeMainArgs *args = NaClChromeMainArgsCreate();
  struct NaClApp *nap = NaClAppCreate();
  struct ThreadArgs thread_args;

  NaClHandleBootstrapArgs(&argc, &argv);
  int last_option_index = NaClHandleArguments(argc, argv);
#if NACL_LINUX
  args->prereserved_sandbox_size = g_prereserved_sandbox_size;
#endif
  // There should be two more arguments after parsing the optional ones.
  if (last_option_index + 2 != argc) {
    NaClLog(
        LOG_FATAL,
        "NaClHandleArguments stopped at opt %d (expected %d == argc(%d)-2\n",
        last_option_index, argc - 2, argc);
  }
  const char *irt_filename = argv[last_option_index];
  const char *nexe_filename = argv[last_option_index + 1];

  NaClLog(LOG_INFO,
          "SelMainChromeTest configuration:\n"
          "g_load_module_srpc: %d\n"
          "g_irt_load_optional: %d\n"
          "g_test_validation_cache: %d\n",
           g_load_module_srpc, g_irt_load_optional, g_test_validation_cache);

  args->irt_fd = OpenFileReadOnly(irt_filename);
  args->irt_load_optional = g_irt_load_optional;
  CHECK(args->irt_fd >= 0);

  memset(&thread_args.file_info, 0, sizeof thread_args.file_info);
  thread_args.file_info.desc = OpenFileReadOnly(nexe_filename);
  CHECK(thread_args.file_info.desc >= 0);
  NaClFileNameForValgrind(nexe_filename);

  NaClHandle socketpair[2];
  CHECK(NaClSocketPair(socketpair) == 0);
  args->imc_bootstrap_handle = socketpair[0];
  thread_args.channel = socketpair[1];

  // Check that NaClDescMakeCustomDesc() works when called in this context.
  NaClAppSetDesc(nap, NACL_CHROME_DESC_BASE, MakeExampleDesc());

  // Set up mock validation cache.
  struct TestValidationHandle test_handle;
  struct NaClValidationCache test_cache;
  if (g_test_validation_cache) {
    test_handle.expected_token_lo = 0xabcdef123456789LL;
    test_handle.expected_token_hi = 0x101010101010101LL;
    test_handle.expected_file_handle = OpenFileHandleReadExec(nexe_filename);
    test_handle.expected_file_path = strdup(nexe_filename);
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
    info.desc = OpenFileHandleReadExec(nexe_filename);
#if NACL_WINDOWS
    info.desc = _open_osfhandle(info.desc, _O_RDONLY | _O_BINARY);
#endif
    if (g_test_validation_cache) {
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
