/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

// Second generation sel_universal implemented in C++ and with optional
// multimedia support via SDL

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
#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/shared/platform/scoped_ptr_refcount.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"
#include "native_client/src/trusted/desc/nacl_desc_wrapper.h"
#include "native_client/src/trusted/desc/nrd_all_modules.h"
#include "native_client/src/trusted/nonnacl_util/sel_ldr_launcher.h"
#include "native_client/src/trusted/reverse_service/reverse_service.h"
#include "native_client/src/trusted/sel_universal/replay_handler.h"
#include "native_client/src/trusted/sel_universal/rpc_universal.h"

#if defined(NACL_SEL_UNIVERSAL_INCLUDE_SDL)
// NOTE: we need to include this so that it can "hijack" main
#include <SDL/SDL.h>
#include "native_client/src/trusted/sel_universal/multimedia_handler.h"
#endif

#include "native_client/src/trusted/service_runtime/nacl_error_code.h"

using std::ifstream;
using std::map;
using std::string;
using std::vector;
using nacl::DescWrapper;

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
#if NACL_SEL_UNIVERSAL_INCLUDE_SDL
    "  --event_record <file>\n"
    "  --event_replay <file>\n"
#endif
    "  --debug\n"
    "  --abort_on_error\n"
    "  --silence_nexe\n"
    "  --command_prefix <prefix>\n"
    "  --command_file <file>\n"
    "  --rpc_load\n"
    "  --rpc_services\n";

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
static string command_prefix = "";
static bool rpc_load = false;
static bool rpc_services = false;

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

  // Extract sel_universal arguments and transfer the rest to sel_ldr_argv
  nacl::string app_name;
  for (int i = 1; i < argc; i++) {
    const string flag(argv[i]);
    // Check if the argument has the form -f nexe
    if (flag == "--help") {
      printf("%s", kUsage);
      exit(0);
#if  NACL_SEL_UNIVERSAL_INCLUDE_SDL
    } else if (flag == "--event_record") {
      if (argc <= i + 1) {
        NaClLog(LOG_FATAL, "not enough args for --event_record option\n");
      }
      RecordPPAPIEvents(argv[i + 1]);
    } else if (flag == "--event_replay") {
      if (argc <= i + 1) {
        NaClLog(LOG_FATAL, "not enough args for --event_replay option\n");
      }
      ReplayPPAPIEvents(argv[i + 1]);
#endif
    } else if (flag == "--debug") {
      NaClLogSetVerbosity(1);
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
      command_prefix = argv[i];
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
    } else if (flag == "--rpc_load") {
      rpc_load = true;
    } else if (flag == "--rpc_services") {
      rpc_services = true;
    } else if (flag == "--") {
      // Done processing sel_ldr args. If no '-f nexe' was given earlier,
      // the first argument after '--' is the nexe.
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

  if (app_name == "") {
    NaClLog(LOG_FATAL, "missing app\n");
  }

  return app_name;
}


int raii_main(int argc, char* argv[]) {
  // Get the arguments to sed_ldr and the nexe module
  vector<nacl::string> sel_ldr_argv;
  vector<nacl::string> app_argv;
  nacl::string app_name =
    ProcessArguments(argc, argv, &sel_ldr_argv, &app_argv);

  // Add '-X 5' to sel_ldr arguments to create communication socket
  sel_ldr_argv.push_back("-X");
  sel_ldr_argv.push_back("5");
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
  // Start sel_ldr with the given application and arguments.
  nacl::SelLdrLauncher launcher;
  nacl::DescWrapperFactory factory;  // DescWrapper "namespace"

  if (command_prefix != "") {
    launcher.SetCommandPrefix(command_prefix);
  }

  if (!launcher.StartFromCommandLine(rpc_load ? NACL_NO_FILE_PATH : app_name,
                                     5, sel_ldr_argv, app_argv)) {
    NaClLog(LOG_FATAL, "sel_universal: Failed to launch sel_ldr\n");
  }

  DescWrapper *host_file = NULL;

  if (rpc_load) {
    host_file = factory.OpenHostFile(app_name.c_str(),
                                     O_RDONLY, 0);
    if (NULL == host_file) {
      NaClLog(LOG_ERROR, "Could not open %s\n", app_name.c_str());
      exit(1);
    }
  }

  if (!launcher.SetupCommandAndLoad(&command_channel, host_file)) {
    NaClLog(LOG_ERROR, "sel_universal: set up command and load failed\n");
    exit(1);
  }

  delete host_file;

  nacl::scoped_ptr_refcount<nacl::ReverseService> rev_svc;

  if (rpc_services) {
    NaClLog(1, "Launching reverse RPC services\n");

    NaClSrpcResultCodes rpc_result;
    NaClDesc* h;
    rpc_result = NaClSrpcInvokeBySignature(&command_channel,
                                           "reverse_setup::h",
                                           &h);
    if (NACL_SRPC_RESULT_OK != rpc_result) {
      NaClLog(LOG_ERROR, "sel_universal: reverse setup failed\n");
      exit(1);
    }
    nacl::scoped_ptr<nacl::DescWrapper> conn_cap(launcher.WrapCleanup(h));
    if (conn_cap == NULL) {
      NaClLog(LOG_ERROR, "sel_universal: reverse desc wrap failed\n");
      exit(1);
    }

    rev_svc.reset(new nacl::ReverseService(conn_cap.get(), NULL));
    if (rev_svc == NULL) {
      NaClLog(LOG_ERROR, "sel_universal: reverse service ctor failed\n");
      exit(1);
    }
    conn_cap.release();
    if (!rev_svc->Start()) {
      NaClLog(LOG_ERROR, "sel_universal: reverse service start failed\n");
      exit(1);
    }
  }

  if (!launcher.StartModuleAndSetupAppChannel(&command_channel, &channel)) {
    NaClLog(LOG_ERROR,
            "sel_universal: start module and set up app channel failed\n");
    exit(1);
  }

  NaClCommandLoop loop(channel.client,
                       &channel,
                       launcher.socket_addr()->desc());

  //
  // Pepper sample commands
  // initialize_pepper pepper
  //   sdl_initialize OR (replay*; replay_activate)
  // install_upcalls service
  // show_variables
  // show_descriptors
  // rpc PPP_InitializeModule i(0) l(0) h(pepper) s("${service}") * i(0) i(0)

  loop.AddHandler("replay_activate", HandlerReplayActivate);
  loop.AddHandler("replay", HandlerReplay);
  loop.AddHandler("replay_unused", HandlerUnusedReplays);

  // possible platform specific stuff
  loop.AddHandler("shmem", HandlerShmem);
  loop.AddHandler("readonly_file", HandlerReadonlyFile);
  loop.AddHandler("sleep", HandlerSleep);
  loop.AddHandler("map_shmem", HandlerMap);
  loop.AddHandler("save_to_file", HandlerSaveToFile);
  loop.AddHandler("load_from_file", HandlerLoadFromFile);
  loop.AddHandler("file_size", HandlerFileSize);
  loop.AddHandler("sync_socket_create", HandlerSyncSocketCreate);
  loop.AddHandler("sync_socket_write", HandlerSyncSocketWrite);
#if  NACL_SEL_UNIVERSAL_INCLUDE_SDL
  loop.AddHandler("sdl_initialize", HandlerSDLInitialize);
  loop.AddHandler("sdl_event_loop", HandlerSDLEventLoop);
#endif

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

  if (rev_svc != NULL) {
    rev_svc->WaitForServiceThreadsToExit();
  }

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
