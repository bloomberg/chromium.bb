/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

/*
 * NaCl testing shell
 */

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/portability.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "native_client/src/shared/srpc/nacl_srpc.h"
#include "native_client/src/trusted/nonnacl_util/sel_ldr_launcher_c.h"

static const char* kUsage =
  "Usage:\n"
  "\n"
  "sel_universal [option*] -f nacl_file [-- sel_ldr_arg*] [-- module_arg*]\n"
  "\n"
  "where option may be\n"
  "-f nacl_module        nacl module to execute\n"
  "-v                    increase verbosity (can be repeated)\n"
  "\n"
  "After startup the user is prompted for interactive commands.\n"
  "For sample commands have a look at: tests/srpc/srpc_basic_test.stdin\n"
  ;


static NaClSrpcError Interpreter(NaClSrpcService* service,
                                 NaClSrpcChannel* channel,
                                 uint32_t rpc_number,
                                 NaClSrpcArg** ins,
                                 NaClSrpcArg** outs) {
  UNREFERENCED_PARAMETER(service);

  return NaClSrpcInvokeV(channel, rpc_number, ins, outs);
}


/* NOTE: this used to be stack allocated inside main which cause
 * problems on ARM (probably a tool chain bug).
 * NaClSrpcChannel is pretty big (> 256kB)
 */
struct NaClSrpcChannel     command_channel;
struct NaClSrpcChannel     channel;

int main(int  argc, char *argv[]) {
  struct NaClSelLdrLauncher* launcher;
  int                        opt;
  static char*               application_name = NULL;
  int                        i;
  size_t                     n;
  int                        sel_ldr_argc;
  char**                     tmp_ldr_argv;
  const char**               sel_ldr_argv;
  int                        module_argc;
  const char**               module_argv;
  static const char*         kFixedArgs[] = { "-X", "5" };
  int                        pass_debug = 0;
  int                        pass_pq_disable = 0;

  /* Descriptor transfer requires the following. */
  NaClNrdAllModulesInit();

  /* command line parsing */
  while ((opt = getopt(argc, argv, "df:Qv")) != -1) {
    switch (opt) {
      case 'd':
        pass_debug = 1;
        break;
      case 'f':
        application_name = optarg;
        break;
      case 'v':
        NaClLogIncrVerbosity();
        break;
      case 'Q':
        /* TODO(cbiffle): pass this in the sel_ldr flags portion of the
           sel_universal commandline options. I.e., fix scons invocations
           etc. so that we don't need this special case here */
        pass_pq_disable = 1;
        break;
      default:
       fputs(kUsage, stderr);
        return -1;
    }
  }

  if (NULL == application_name) {
    fprintf(stderr, "-f nacl_file must be specified\n");
    return 1;
  }


  /*
   * Collect any extra arguments on to sel_ldr or the application.
   * sel_ldr_argv contains the options to be passed to sel_ldr.
   * module_argv contains the options to be passed to the application.
   */
  tmp_ldr_argv = NULL;
  sel_ldr_argc = 0;
  module_argv = NULL;
  module_argc = 0;
  for (i = 0; i < argc; ++i) {
    if (tmp_ldr_argv == NULL) {
      /* sel_universal arguments come first. */
      if (!strcmp("--", argv[i])) {
        tmp_ldr_argv = &argv[i + 1];
      }
    } else if (module_argv == NULL) {
      /* sel_ldr arguments come next. */
      if (!strcmp("--", argv[i])) {
        module_argv = (const char**) &argv[i + 1];
      } else {
        ++sel_ldr_argc;
      }
    } else {
      /* application arguments come last. */
      ++module_argc;
    }
  }

  /*
   * Prepend the fixed arguments to the command line.
   */
  sel_ldr_argv =
    (const char**) malloc((pass_debug + pass_pq_disable + sel_ldr_argc
                           + NACL_ARRAY_SIZE(kFixedArgs)) *
                          sizeof(*sel_ldr_argv));
  for (n = 0; n < NACL_ARRAY_SIZE(kFixedArgs); ++n) {
    sel_ldr_argv[n] = kFixedArgs[n];
  }
  for (i = 0; i < sel_ldr_argc; ++i) {
    sel_ldr_argv[i + NACL_ARRAY_SIZE(kFixedArgs)] = tmp_ldr_argv[i];
  }
  if (pass_debug) {
    sel_ldr_argv[sel_ldr_argc + NACL_ARRAY_SIZE(kFixedArgs)] = "-d";
  }
  if (pass_pq_disable) {
    sel_ldr_argv[sel_ldr_argc + NACL_ARRAY_SIZE(kFixedArgs) + pass_debug] =
        "-Q";
  }

  /*
   * Start sel_ldr with the given application and arguments.
   */
  launcher = NaClSelLdrStart(application_name,
                             5,
                             (pass_debug + pass_pq_disable + sel_ldr_argc
                              + NACL_ARRAY_SIZE(kFixedArgs)),
                             (const char**) sel_ldr_argv,
                             module_argc,
                             (const char**) module_argv);

  /*
   * Open the communication channels to the service runtime.
   */
  if (!NaClSelLdrOpenSrpcChannels(launcher, &command_channel, &channel)) {
    fprintf(stderr, "Open failed\n");
    return 1;
  }

  NaClSrpcCommandLoop(channel.client,
                        &channel,
                      Interpreter,
                      NaClSelLdrGetSockAddr(launcher));

  /*
   * Close the connections to sel_ldr.
   */
  NaClSrpcDtor(&command_channel);
  NaClSrpcDtor(&channel);

  /*
   * And shut it down.
   */
  NaClSelLdrShutdown(launcher);

  NaClNrdAllModulesFini();
  return 0;
}
