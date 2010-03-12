/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

/*
 * Second generation sel_universal implemented in C++ and with
 * (limited) multimedia support
 */

// NOTE: we need to include this so that it can "hijack" main
#include <SDL.h>

#include <vector>

#include "native_client/src/include/nacl_string.h"
#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/portability.h"
#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"
#include "native_client/src/trusted/nonnacl_util/sel_ldr_launcher.h"

using std::vector;


// TODO(robertm): move this into its own header at some point
extern void IntializeMultimediaHandler(NaClSrpcChannel* channel,
                                       int width,
                                       int height,
                                       const char* title);
static const char* kUsage =
  "Usage:\n"
  "\n"
  "sel_universal_new [option*] -f nexe_file [-- sel_ldr_arg*] [-- nexe_arg*]\n"
  "\n"
  "where option may be\n"
  "-f nacl_module        nacl module to execute\n"
  "-v                    increase verbosity (can be repeated)\n"
  "\n"
  "After startup the user is prompted for interactive commands.\n"
  "For sample commands have a look at: tests/srpc/srpc_basic_test.stdin\n";


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
struct NaClSrpcChannel command_channel;
struct NaClSrpcChannel untrusted_command_channel;
struct NaClSrpcChannel channel;

int main(int  argc, char *argv[]) {
  nacl::string app_name;
  int pass_debug = 0;
  int width = 640;
  int height = 480;
  /* Descriptor transfer requires the following. */
  NaClNrdAllModulesInit();

  /* command line parsing */
  while (1) {
    const int opt = getopt(argc, argv, "df:v");
    if (opt == -1) break;

    switch (opt) {
      case 'd':
        pass_debug = 1;
        break;
      case 'f':
        app_name = optarg;
        break;
      case 'v':
        NaClLogIncrVerbosity();
        break;
      default:
        fputs(kUsage, stderr);
        return -1;
    }
  }

  if (app_name == "") {
    NaClLog(LOG_FATAL, "-f nacl_file must be specified\n");
    return 1;
  }

  vector<nacl::string> sel_ldr_argv;
  vector<nacl::string> app_argv;

  int i;
  /* remaining sel_universal arguments come first. */
  for (i = 0; i < argc; ++i) {
    if (nacl::string("--") == argv[i]) break;
  }

  sel_ldr_argv.push_back("-P");
  sel_ldr_argv.push_back("5");
  sel_ldr_argv.push_back("-X");
  sel_ldr_argv.push_back("5");
  if (pass_debug) {
    sel_ldr_argv.push_back("-d");
  }
  /* sel_ldr arguments come next. */
  for (++i; i < argc; ++i) {
    if (nacl::string("--") == argv[i]) break;

    sel_ldr_argv.push_back(argv[i]);
  }

  /* app args are last. */
  for (++i; i < argc; ++i) {
    app_argv.push_back(argv[i]);
  }

  /*
   * Start sel_ldr with the given application and arguments.
   */
  nacl::SelLdrLauncher launcher;
  if (!launcher.Start(app_name, 5, sel_ldr_argv, app_argv)) {
    NaClLog(LOG_FATAL, "Launch failed\n");
  }

  /*
   * Open the communication channels to the service runtime.
   */
  if (!launcher.OpenSrpcChannels(&command_channel,
                                 &untrusted_command_channel,
                                 &channel)) {
    NaClLog(LOG_FATAL, "Open channel failed\n");
  }

  IntializeMultimediaHandler(&channel, width, height, "NaCl Module");

  NaClSrpcCommandLoop(channel.client,
                      &channel,
                      Interpreter,
                      launcher.GetSelLdrSocketAddress());

  /*
   * Close the connections to sel_ldr.
   */
  NaClSrpcDtor(&command_channel);
  NaClSrpcDtor(&untrusted_command_channel);
  NaClSrpcDtor(&channel);

  NaClNrdAllModulesFini();
  return 0;
}
