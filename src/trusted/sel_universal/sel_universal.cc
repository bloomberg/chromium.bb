/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

// Second generation sel_universal implemented in C++

#include <stdio.h>

#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "native_client/src/include/nacl_string.h"
#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/nacl_scoped_ptr.h"
#include "native_client/src/include/portability.h"
#include "native_client/src/include/portability_io.h"
#include "native_client/src/public/secure_service.h"
#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/shared/platform/scoped_ptr_refcount.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"
#include "native_client/src/trusted/desc/nacl_desc_wrapper.h"
#include "native_client/src/trusted/desc/nrd_all_modules.h"
#include "native_client/src/trusted/nonnacl_util/sel_ldr_launcher.h"
#include "native_client/src/trusted/sel_universal/pnacl_emu_handler.h"
#include "native_client/src/trusted/sel_universal/replay_handler.h"
#include "native_client/src/trusted/sel_universal/rpc_universal.h"
#include "native_client/src/trusted/service_runtime/nacl_error_code.h"

#include "native_client/src/trusted/nonnacl_util/launcher_factory.h"
#include "native_client/src/trusted/nonnacl_util/sel_ldr_launcher_zygote.h"

using std::ifstream;
using std::map;
using std::string;
using std::vector;
using nacl::DescWrapper;

// TODO(robertm): add explanation for flags
static const char* kUsage =
    "Usage:\n"
    "\n"
    "sel_universal <sel_universal_arg> <sel_ldr_arg>* [-- <nexe> <nexe_arg>*]\n"
    "\n"
    "Exactly one nexe argument is required.\n"
    "After startup the user is prompted for interactive commands.\n"
    "For sample commands have a look at: tests/srpc/srpc_basic_test.stdin\n"
    "\n"
    "sel_universal arguments are:\n"
    "\n"
    "  --help\n"
    "  --event_record <file>\n"
    "  --event_replay <file>\n"
    "  --debug\n"
    "  --abort_on_error\n"
    "  --silence_nexe\n"
    "  --command_prefix <prefix>\n"
    "  --command_file <file>\n"
    "  --var <tag> <value>\n"
    "  --url_alias <url> <filename>\n"
    "  --no_app_channel\n"
    "\n"
    "The following sel_ldr arguments might be useful:\n"
    "  -v                    increase verbosity\n"
    "  -E NACL_SRPC_DEBUG=1  even more verbosity for srpc debugging\n";


// NOTE: this used to be stack allocated inside main which cause
// problems on ARM (probably a tool chain bug).
// NaClSrpcChannel is pretty big (> 256kB)
static NaClSrpcChannel command_channel;
static NaClSrpcChannel channel;

// variables set via command line
static map<string, string> initial_vars;
static vector<string> initial_commands;
static bool abort_on_error = false;
static bool silence_nexe = false;
static vector<string> command_prefix;
static bool app_channel = true;

// When given argc and argv this function (a) extracts the nexe argument,
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

  int debug_level = 0;
  // Extract sel_universal arguments and transfer the rest to sel_ldr_argv
  nacl::string app_name;
  for (int i = 1; i < argc; i++) {
    const string flag(argv[i]);
    if (flag == "--help") {
      printf("%s", kUsage);
      exit(0);
    } else if (flag == "--debug") {
      ++debug_level;
    } else if (flag == "--abort_on_error") {
      abort_on_error = true;
    } else if (flag == "--silence_nexe") {
      silence_nexe = true;
    } else if (flag == "--command_prefix") {
      if (argc <= i + 1) {
        NaClLog(LOG_FATAL,
                "not enough args for --command_prefix option\n");
      }
      ++i;
      command_prefix.push_back(argv[i]);
    } else if (flag == "--command_file") {
      if (argc <= i + 1) {
        NaClLog(LOG_FATAL, "not enough args for --command_file option\n");
      }
      NaClLog(LOG_INFO, "reading commands from %s\n", argv[i + 1]);
      ifstream f;
      f.open(argv[i + 1]);
      ++i;
      while (!f.eof()) {
        string s;
        getline(f, s);
        initial_commands.push_back(s);
      }
      f.close();
      NaClLog(LOG_INFO, "total commands now: %d\n",
              static_cast<int>(initial_commands.size()));
    } else if (flag == "--var") {
      if (argc <= i + 2) {
        NaClLog(LOG_FATAL, "not enough args for --var option\n");
      }

      const string tag = string(argv[i + 1]);
      const string val = string(argv[i + 2]);
      i += 2;
      initial_vars[tag] = val;
    } else if (flag == "--no_app_channel") {
      app_channel = false;
    } else if (flag == "--") {
      // Done processing sel_ldr args.  The first argument after '--' is the
      // nexe.
      i++;
      if (app_name == "" && i < argc) {
        app_name = argv[i++];
      }
      // The remaining arguments are passed to the executable.
      for (; i < argc; i++) {
        app_argv->push_back(argv[i]);
      }
    } else {
      // NOTE: most sel_ldr args start with a single hyphen so there is not
      // much confusion with sel_universal args. But this remains a hack.
      sel_ldr_argv->push_back(argv[i]);
    }
  }

  if (debug_level > 0) {
    NaClLogSetVerbosity(debug_level);
  }

  if (app_name == "") {
    NaClLog(LOG_FATAL, "missing app\n");
  }

  return app_name;
}


static bool HandlerHardShutdown(NaClCommandLoop* ncl,
                               const vector<string>& args) {
  UNREFERENCED_PARAMETER(ncl);
  UNREFERENCED_PARAMETER(args);
  NaClSrpcInvokeBySignature(&command_channel,
                            NACL_SECURE_SERVICE_HARD_SHUTDOWN);
  return true;
}


static bool HandlerForceShutdown(NaClCommandLoop* ncl,
                                const vector<string>& args) {
  UNREFERENCED_PARAMETER(ncl);
  UNREFERENCED_PARAMETER(args);
  NaClSrpcDtor(&command_channel);
  return true;
}


int raii_main(int argc, char* argv[]) {
  // Get the arguments to sed_ldr and the nexe module
  vector<nacl::string> sel_ldr_argv;
  vector<nacl::string> app_argv;
  nacl::string app_name =
    ProcessArguments(argc, argv, &sel_ldr_argv, &app_argv);

  if (silence_nexe) {
    // redirect stdout/stderr in the nexe to /dev/null
    std::stringstream ss_stdout;
    std::stringstream ss_stderr;

    int fd = open(PORTABLE_DEV_NULL, O_RDWR);
    sel_ldr_argv.push_back("-w");
    ss_stdout << "1:" << fd;
    sel_ldr_argv.push_back(ss_stdout.str());
    sel_ldr_argv.push_back("-w");
    ss_stderr << "2:" << fd;
    sel_ldr_argv.push_back(ss_stderr.str());
  }
  nacl::Zygote zygote;
  if (!zygote.Init()) {
    NaClLog(LOG_FATAL, "sel_universal: cannot spawn zygote process\n");
  }
  NaClLog(4, "sel_universal: zygote.Init() okay\n");
  nacl::SelLdrLauncherStandaloneFactory launcher_factory(&zygote);
  NaClLog(4, "sel_universal: launcher_factory ctor() okay\n");

  // Start sel_ldr with the given application and arguments.
  nacl::scoped_ptr<nacl::SelLdrLauncherStandalone> launcher(
      launcher_factory.MakeSelLdrLauncherStandalone());
  NaClLog(4, "sel_universal: MakeSelLdrLauncherStandalone() okay\n");
  nacl::DescWrapperFactory factory;  // DescWrapper "namespace"

  if (!launcher->StartViaCommandLine(command_prefix, sel_ldr_argv, app_argv)) {
    NaClLog(LOG_FATAL, "sel_universal: Failed to launch sel_ldr\n");
  }

  if (!launcher->SetupCommand(&command_channel)) {
    NaClLog(LOG_ERROR, "sel_universal: set up command failed\n");
    exit(1);
  }

  DescWrapper *host_file = factory.OpenHostFile(app_name.c_str(), O_RDONLY, 0);
  if (NULL == host_file) {
    NaClLog(LOG_ERROR, "Could not open %s\n", app_name.c_str());
    exit(1);
  }

  if (!launcher->LoadModule(&command_channel, host_file)) {
    NaClLog(LOG_ERROR, "sel_universal: load module failed\n");
    exit(1);
  }

  delete host_file;

  if (!launcher->StartModule(&command_channel)) {
    NaClLog(LOG_ERROR,
            "sel_universal: start module failed\n");
    exit(1);
  }

  if (app_channel) {
    if (!launcher->SetupAppChannel(&channel)) {
      NaClLog(LOG_ERROR,
              "sel_universal: set up app channel failed\n");
      exit(1);
    }
  }

  NaClCommandLoop loop(channel.client,
                       &channel,
                       launcher->socket_addr()->desc());

  // Sample built-in commands accepted by sel_universal:
  //
  // install_upcalls service
  // show_variables
  // show_descriptors
  // rpc <name> <in-arg>* "*" <out-arg>*
  //
  // TODO(robertm): add more documentation
  //
  // More custom command handlers are installed below:

  loop.AddHandler("replay_activate", HandlerReplayActivate);
  loop.AddHandler("replay", HandlerReplay);
  loop.AddHandler("replay_unused", HandlerUnusedReplays);

  // possible platform specific stuff
  loop.AddHandler("readonly_file", HandlerReadonlyFile);
  loop.AddHandler("readwrite_file", HandlerReadwriteFile);
  loop.AddHandler("sleep", HandlerSleep);
  loop.AddHandler("save_to_file", HandlerSaveToFile);
  loop.AddHandler("load_from_file", HandlerLoadFromFile);
  loop.AddHandler("file_size", HandlerFileSize);
  loop.AddHandler("sync_socket_create", HandlerSyncSocketCreate);
  loop.AddHandler("sync_socket_write", HandlerSyncSocketWrite);
  // new names
  loop.AddHandler("stream_file", HandlerPnaclFileStream);

  loop.AddHandler("hard_shutdown", HandlerHardShutdown);
  loop.AddHandler("force_shutdown", HandlerForceShutdown);

  NaClLog(1, "populating initial vars\n");
  for (map<string, string>::iterator it = initial_vars.begin();
       it != initial_vars.end();
       ++it) {
    loop.SetVariable(it->first, it->second);
  }

  const bool success = initial_commands.size() > 0 ?
                       loop.ProcessCommands(initial_commands) :
                       loop.StartInteractiveLoop(abort_on_error);

  // Close the connections to sel_ldr.
  NaClSrpcDtor(&command_channel);
  NaClSrpcDtor(&channel);

  return success ? 0 : -1;
}

int main(int argc, char* argv[]) {
  // Descriptor transfer requires the following
  NaClSrpcModuleInit();
  NaClNrdAllModulesInit();

  int exit_status = raii_main(argc, argv);

  NaClSrpcModuleFini();
  NaClNrdAllModulesFini();

  return exit_status;
}
