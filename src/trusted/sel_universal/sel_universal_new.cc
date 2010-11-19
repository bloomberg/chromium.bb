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
#include "native_client/src/trusted/nonnacl_util/sel_ldr_launcher.h"

using std::vector;
using std::string;

#ifdef NACL_SEL_UNIVERSAL_INCLUDE_SDL
// TODO(robertm): move this into its own header at some point
extern void InitializeMultimediaHandler(NaClSrpcChannel* channel,
                                       int width,
                                       int height,
                                       const char* title);
#endif

static const char* kUsage =
  "Usage:\n"
  "\n"
  "sel_universal [-f nacl_file] [sel_ldr_arg*] [-- [nacl_file] module_arg*]\n"
  "\n"
  "Exactly one nacl_file argument is required.\n"
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
  int i;

  if (argc == 1) {
    printf("%s", kUsage);
    exit(0);
  }

  // Extract '-f nacl_file' from args and transfer the rest to sel_ldr_argv
  nacl::string app_name;
  for (i = 1; i < argc; i++) {
    // Check if the argument has the form -f nacl_file
    if (string(argv[i]) == "-f" && i + 1 < argc) {
      app_name = argv[i + 1];
      ++i;
    } else if (string(argv[i]) == "--help") {
      printf("%s", kUsage);
      exit(0);
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
    fprintf(stderr,
            "sel_universal: '-f nacl_file' or '-- nacl_file' required\n");
    exit(1);
  }

  return app_name;
}


int main(int  argc, char* argv[]) {
  // Get the arguments to sed_ldr and the nexe module
  vector<nacl::string> sel_ldr_argv;
  vector<nacl::string> app_argv;
  nacl::string app_name =
    ProcessArguments(argc, argv, &sel_ldr_argv, &app_argv);

  // Add '-X 5' to sel_ldr arguments to create communication socket
  sel_ldr_argv.push_back("-X");
  sel_ldr_argv.push_back("5");

  // Descriptor transfer requires the following
  NaClNrdAllModulesInit();

  // Start sel_ldr with the given application and arguments.
  nacl::SelLdrLauncher launcher;
  if (!launcher.StartFromCommandLine(app_name, 5, sel_ldr_argv, app_argv)) {
    NaClLog(LOG_FATAL, "sel_universal: Failed to launch sel_ldr\n");
  }

  // Open the communication channels to the service runtime.
  if (!launcher.OpenSrpcChannels(&command_channel, &channel)) {
    fprintf(stderr, "sel_universal: Open channel failed\n");
    exit(1);
  }

#ifdef NACL_SEL_UNIVERSAL_INCLUDE_SDL
  int width = 640;
  int height = 480;
  InitializeMultimediaHandler(&channel, width, height, "NaCl Module");
#endif

  NaClSrpcCommandLoop(channel.client,
                      &channel,
                      Interpreter,
                      launcher.socket_address());


  // Close the connections to sel_ldr.
  NaClSrpcDtor(&command_channel);
  NaClSrpcDtor(&channel);

  NaClNrdAllModulesFini();
  return 0;
}
