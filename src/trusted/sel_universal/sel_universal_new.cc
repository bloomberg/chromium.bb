/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

// Second generation sel_universal implemented in C++ and with
// (limited) multimedia support

#ifdef NACL_SEL_UNIVERSAL_INCLUDE_SDL
  // NOTE: we need to include this so that it can "hijack" main
#include <SDL.h>
#endif

#include <stdio.h>
#include <string>
#include <vector>

#include "native_client/src/include/nacl_string.h"
#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/portability.h"
#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"
#include "native_client/src/trusted/desc/nacl_desc_wrapper.h"
#include "native_client/src/trusted/nonnacl_util/sel_ldr_launcher.h"
#include "native_client/src/trusted/sel_universal/rpc_universal.h"

using std::vector;
using std::string;
using nacl::DescWrapper;

static const char* kUsage =
  "Usage:\n"
  "\n"
  "sel_universal [-f nacl_file] [sel_ldr_arg*] [-- [nacl_file] module_arg*]\n"
  "\n"
  "Exactly one nacl_file argument is required.\n"
  "After startup the user is prompted for interactive commands.\n"
  "For sample commands have a look at: tests/srpc/srpc_basic_test.stdin\n";

// NOTE: this used to be stack allocated inside main which cause
// problems on ARM (probably a tool chain bug).
// NaClSrpcChannel is pretty big (> 256kB)
struct NaClSrpcChannel command_channel;
struct NaClSrpcChannel channel;

// When given argc and argv this function (a) extracts the nacl_file argument,
// (b) populates sel_ldr_argv with sel_ldr arguments, and (c) populates
// app_argv with nexe module args. Also see kUsage above for details.
// It will call exit with codes 0 (help message) and 1 (incorrect args).
static nacl::string ProcessArguments(int argc,
                                     char* argv[],
                                     vector<nacl::string>* const sel_ldr_argv,
                                     vector<nacl::string>* const app_argv) {
  if (argc == 1) {
    printf("%s", kUsage);
    exit(0);
  }

  // Extract '-f nacl_file' from args and transfer the rest to sel_ldr_argv
  nacl::string app_name;
  for (int i = 1; i < argc; i++) {
    // Check if the argument has the form -f nacl_file
    if (string(argv[i]) == "-f" && i + 1 < argc) {
      app_name = argv[i + 1];
      ++i;
    } else if (string(argv[i]) == "--help") {
      printf("%s", kUsage);
      exit(0);
    } else if (string(argv[i]) == "--debug") {
      NaClLogSetVerbosity(1);
    } else if (string(argv[i]) == "--") {
      // Done processing sel_ldr args. If no '-f nacl_file' was given earlier,
      // the first argument after '--' is the nacl_file.
      i++;
      if (app_name == "" && i < argc) {
        app_name = argv[i++];
      }
      // The remaining arguments are passed to the executable.
      for (; i < argc; i++) {
        app_argv->push_back(argv[i]);
      }
    } else {
      sel_ldr_argv->push_back(argv[i]);
    }
  }

  if (app_name == "") {
    NaClLog(LOG_FATAL, "missing app\n");
    exit(1);
  }

  return app_name;
}


int main(int argc, char* argv[]) {
  // Descriptor transfer requires the following
  NaClSrpcModuleInit();
  NaClNrdAllModulesInit();

  // Get the arguments to sed_ldr and the nexe module
  vector<nacl::string> sel_ldr_argv;
  vector<nacl::string> app_argv;
  nacl::string app_name =
    ProcessArguments(argc, argv, &sel_ldr_argv, &app_argv);

  // Add '-X 5' to sel_ldr arguments to create communication socket
  sel_ldr_argv.push_back("-X");
  sel_ldr_argv.push_back("5");


  // Start sel_ldr with the given application and arguments.
  nacl::SelLdrLauncher launcher;
  if (!launcher.StartFromCommandLine(app_name, 5, sel_ldr_argv, app_argv)) {
    NaClLog(LOG_FATAL, "sel_universal: Failed to launch sel_ldr\n");
  }

  // Open the communication channels to the service runtime.
  if (!launcher.OpenSrpcChannels(&command_channel, &channel)) {
    NaClLog(LOG_ERROR, "sel_universal: Open channel failed\n");
    exit(1);
  }

  NaClCommandLoop loop(channel.client,
                       &channel,
                       launcher.socket_address());

#if ENABLE_PEPPER_EMULATION
  // Pepper sample commands
  // initialize_pepper pepper_desc
  // add_pepper_rpcs
  // install_upcalls service_string
  // show_variables
  // show_descriptors
  // rpc PPP_InitializeModule i(0) l(0) h(pepper_desc) s("${service_string}") * i(0) i(0)

  extern bool HandlerPepperInit(NaClCommandLoop* ncl,
                                const vector<string>& args);
  loop.AddHandler("initialize_pepper", HandlerPepperInit);

  extern bool HandlerAddPepperRpcs(NaClCommandLoop* ncl,
                                   const vector<string>& args);
  loop.AddHandler("add_pepper_rpcs", HandlerAddPepperRpcs);
#endif

#if NACL_LINUX
  extern bool HandlerSysv(NaClCommandLoop* ncl, const vector<string>& args);
  loop.AddHandler("sysv", HandlerSysv);

  extern bool HandlerSleep(NaClCommandLoop* ncl, const vector<string>& args);
  loop.AddHandler("sleep", HandlerSleep);
#endif  /* NACL_LINUX */

  NaClLog(1, "starting loop\n");
  loop.StartInteractiveLoop();

  // Close the connections to sel_ldr.
  NaClSrpcDtor(&command_channel);
  NaClSrpcDtor(&channel);

  NaClSrpcModuleFini();
  NaClNrdAllModulesFini();
  return 0;
}
