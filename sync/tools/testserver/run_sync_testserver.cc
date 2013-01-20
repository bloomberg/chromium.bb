// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/file_path.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/process_util.h"
#include "base/string_number_conversions.h"
#include "base/test/test_timeouts.h"
#include "net/test/python_utils.h"
#include "sync/test/local_sync_test_server.h"

static void PrintUsage() {
  printf("run_sync_testserver [--port=<port>] [--xmpp-port=<xmpp_port>]\n");
}

// Launches the chromiumsync_test.py or xmppserver_test.py scripts, which test
// the sync HTTP and XMPP sever functionality respectively.
static bool RunSyncTest(const FilePath::StringType& sync_test_script_name) {
  scoped_ptr<syncer::LocalSyncTestServer> test_server(
      new syncer::LocalSyncTestServer());
 if (!test_server->SetPythonPath()) {
    LOG(ERROR) << "Error trying to set python path. Exiting.";
    return false;
  }

  FilePath sync_test_script_path;
  if (!test_server->GetTestScriptPath(sync_test_script_name,
                                      &sync_test_script_path)) {
    LOG(ERROR) << "Error trying to get path for test script "
               << sync_test_script_name;
    return false;
  }

  CommandLine python_command(CommandLine::NO_PROGRAM);
  if (!GetPythonCommand(&python_command)) {
    LOG(ERROR) << "Could not get python runtime command.";
    return false;
  }

  python_command.AppendArgPath(sync_test_script_path);
  if (!base::LaunchProcess(python_command, base::LaunchOptions(), NULL)) {
    LOG(ERROR) << "Failed to launch test script " << sync_test_script_name;
    return false;
  }
  return true;
}

// Gets a port value from the switch with name |switch_name| and writes it to
// |port|. Returns true if a port was provided and false otherwise.
static bool GetPortFromSwitch(const std::string& switch_name, uint16* port) {
  DCHECK(port != NULL) << "|port| is NULL";
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  int port_int = 0;
  if (command_line->HasSwitch(switch_name)) {
    std::string port_str = command_line->GetSwitchValueASCII(switch_name);
    if (!base::StringToInt(port_str, &port_int)) {
      return false;
    }
  }
  *port = static_cast<uint16>(port_int);
  return true;
}

int main(int argc, const char* argv[]) {
  base::AtExitManager at_exit_manager;
  MessageLoopForIO message_loop;

  // Process command line
  CommandLine::Init(argc, argv);
  CommandLine* command_line = CommandLine::ForCurrentProcess();

  if (!logging::InitLogging(
          FILE_PATH_LITERAL("sync_testserver.log"),
          logging::LOG_TO_BOTH_FILE_AND_SYSTEM_DEBUG_LOG,
          logging::LOCK_LOG_FILE,
          logging::APPEND_TO_OLD_LOG_FILE,
          logging::DISABLE_DCHECK_FOR_NON_OFFICIAL_RELEASE_BUILDS)) {
    printf("Error: could not initialize logging. Exiting.\n");
    return -1;
  }

  TestTimeouts::Initialize();

  if (command_line->HasSwitch("help")) {
    PrintUsage();
    return 0;
  }

  if (command_line->HasSwitch("sync-test")) {
    return RunSyncTest(FILE_PATH_LITERAL("chromiumsync_test.py")) ? 0 : -1;
  }

  if (command_line->HasSwitch("xmpp-test")) {
    return RunSyncTest(FILE_PATH_LITERAL("xmppserver_test.py")) ? 0 : -1;
  }

  uint16 port = 0;
  GetPortFromSwitch("port", &port);

  uint16 xmpp_port = 0;
  GetPortFromSwitch("xmpp-port", &xmpp_port);

  scoped_ptr<syncer::LocalSyncTestServer> test_server(
      new syncer::LocalSyncTestServer(port, xmpp_port));
  if (!test_server->Start()) {
    printf("Error: failed to start python sync test server. Exiting.\n");
    return -1;
  }

  printf("Python sync test server running at %s (type ctrl+c to exit)\n",
         test_server->host_port_pair().ToString().c_str());

  message_loop.Run();
  return 0;
}
