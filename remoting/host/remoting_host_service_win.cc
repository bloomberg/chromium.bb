// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file implements the Windows service controlling Me2Me host processes
// running within user sessions.

#include "remoting/host/remoting_host_service_win.h"

#include <windows.h>
#include <stdio.h>

#include "base/at_exit.h"
#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/file_path.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/string16.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"

#include "remoting/base/scoped_sc_handle_win.h"
#include "remoting/host/remoting_host_service_resource.h"

using base::StringPrintf;

namespace {

// Service name.
const char kServiceName[] = "chromoting";
// TODO(alexeypa): investigate and migrate this over to Chrome's i18n framework.
const char kMuiStringFormat[] = "@%ls,-%d";
const char kServiceDependencies[] = "";

const DWORD kServiceStopTimeoutMs = 30 * 1000;

// Command line actions and switches:
// "run" sumply runs the service as usual.
const char kRunActionName[] = "run";

// "install" requests the service to be installed.
const char kInstallActionName[] = "install";

// "remove" uninstalls the service.
const char kRemoveActionName[] = "remove";

// "--console" runs the service interactively for debugging purposes.
const char kConsoleSwitchName[] = "console";

// "--help" or "--?" prints the usage message.
const char kHelpSwitchName[] = "help";
const char kQuestionSwitchName[] = "?";

const char kUsageMessage[] =
  "\n"
  "Usage: %s [action] [options]\n"
  "\n"
  "Actions:\n"
  "  run         - Run the service. If no action specified 'run' is assumed.\n"
  "  install     - Install the service.\n"
  "  remove      - Uninstall the service.\n"
  "\n"
  "Options:\n"
  "  --console   - Run the service interactively for debugging purposes.\n"
  "  --help, --? - Print this message.\n";

// Exit codes:
const int kSuccessExitCode = 0;
const int kUsageExitCode = 1;
const int kErrorExitCode = 2;

void usage(const char* program_name) {
  fprintf(stderr, kUsageMessage, program_name);
}

}  // namespace

namespace remoting {

HostService::HostService() :
  run_routine_(&HostService::RunAsService) {
}

HostService::~HostService() {
}

bool HostService::InitWithCommandLine(const CommandLine* command_line) {
  CommandLine::StringVector args = command_line->GetArgs();

  // Choose the action to perform.
  if (!args.empty()) {
    if (args.size() > 1) {
      LOG(ERROR) << "Invalid command line: more than one action requested.";
      return false;
    }
    if (args[0] == ASCIIToUTF16(kInstallActionName)) {
      run_routine_ = &HostService::Install;
    } else if (args[0] == ASCIIToUTF16(kRemoveActionName)) {
      run_routine_ = &HostService::Remove;
    } else if (args[0] != ASCIIToUTF16(kRunActionName)) {
      LOG(ERROR) << "Invalid command line: invalid action specified: "
                 << args[0];
      return false;
    }
  }

  // Run interactively if needed.
  if (run_routine_ == &HostService::RunAsService &&
      command_line->HasSwitch(kConsoleSwitchName)) {
    run_routine_ = &HostService::RunInConsole;
  }

  return true;
}

int HostService::Install() {
  ScopedScHandle scmanager(
      OpenSCManagerW(NULL, NULL,
                     SC_MANAGER_CONNECT | SC_MANAGER_CREATE_SERVICE));
  if (!scmanager.IsValid()) {
    LOG_GETLASTERROR(ERROR)
        << "Failed to connect to the service control manager";
    return kErrorExitCode;
  }

  FilePath exe;
  if (!PathService::Get(base::FILE_EXE, &exe)) {
    LOG(ERROR) << "Unable to retrieve the service binary path.";
    return kErrorExitCode;
  }

  string16 name = StringPrintf(ASCIIToUTF16(kMuiStringFormat).c_str(),
                               exe.value().c_str(),
                               IDS_DISPLAY_SERVICE_NAME);
  ScopedScHandle service(
      CreateServiceW(scmanager,
                     ASCIIToUTF16(kServiceName).c_str(),
                     name.c_str(),
                     SERVICE_QUERY_STATUS | SERVICE_CHANGE_CONFIG,
                     SERVICE_WIN32_OWN_PROCESS,
                     SERVICE_DEMAND_START, SERVICE_ERROR_NORMAL,
                     exe.value().c_str(),
                     NULL,
                     NULL,
                     ASCIIToUTF16(kServiceDependencies).c_str(),
                     NULL,
                     NULL));

  if (service.IsValid()) {
    // Set the service description if the service is freshly installed.
    string16 description = StringPrintf(
                               ASCIIToUTF16(kMuiStringFormat).c_str(),
                               exe.value().c_str(),
                               IDS_SERVICE_DESCRIPTION);

    SERVICE_DESCRIPTIONW info;
    info.lpDescription = const_cast<LPWSTR>(description.c_str());
    if (!ChangeServiceConfig2W(service, SERVICE_CONFIG_DESCRIPTION, &info)) {
      LOG_GETLASTERROR(ERROR) << "Failed to set the service description";
      return kErrorExitCode;
    }

    printf("The service has been installed successfully.\n");
    return kSuccessExitCode;
  } else {
    if (GetLastError() == ERROR_SERVICE_EXISTS) {
      printf("The service is installed already.\n");
      return kSuccessExitCode;
    } else {
      LOG_GETLASTERROR(ERROR)
          << "Failed to create the service";
      return kErrorExitCode;
    }
  }
}

VOID CALLBACK HostService::OnServiceStopped(PVOID context) {
  SERVICE_NOTIFY* notify = reinterpret_cast<SERVICE_NOTIFY*>(context);
  DCHECK(notify != NULL);
  DCHECK(notify->dwNotificationStatus == ERROR_SUCCESS);
  DCHECK(notify->dwNotificationTriggered == SERVICE_NOTIFY_STOPPED);
  DCHECK(notify->ServiceStatus.dwCurrentState == SERVICE_STOPPED);
}

int HostService::Remove() {
  ScopedScHandle scmanager(OpenSCManagerW(NULL, NULL, SC_MANAGER_CONNECT));
  if (!scmanager.IsValid()) {
    LOG_GETLASTERROR(ERROR)
        << "Failed to connect to the service control manager";
    return kErrorExitCode;
  }

  ScopedScHandle service(
      OpenServiceW(scmanager, ASCIIToUTF16(kServiceName).c_str(),
                   DELETE | SERVICE_STOP | SERVICE_QUERY_STATUS));
  if (!service.IsValid()) {
    if (GetLastError() == ERROR_SERVICE_DOES_NOT_EXIST) {
      printf("The service is not installed.\n");
      return kSuccessExitCode;
    } else {
      LOG_GETLASTERROR(ERROR) << "Failed to open the service";
      return kErrorExitCode;
    }
  }

  // Register for the service status notifications. The notification is
  // going to be delivered even if the service is stopped already.
  SERVICE_NOTIFY notify;
  ZeroMemory(&notify, sizeof(notify));
  notify.dwVersion = SERVICE_NOTIFY_STATUS_CHANGE;
  notify.pfnNotifyCallback = &HostService::OnServiceStopped;
  notify.pContext = this;

  // The notification callback will be unregistered once the service handle
  // is closed.
  if (ERROR_SUCCESS != NotifyServiceStatusChange(
                           service, SERVICE_NOTIFY_STOPPED, &notify)) {
    LOG_GETLASTERROR(ERROR)
        << "Failed to register for the service status notifications";
    return kErrorExitCode;
  }

  // Ask SCM to stop the service and wait.
  SERVICE_STATUS status;
  if (ControlService(service, SERVICE_CONTROL_STOP, &status)) {
    printf("Stopping...\n");
  }

  DWORD wait_result = SleepEx(kServiceStopTimeoutMs, TRUE);
  if (wait_result != WAIT_IO_COMPLETION) {
    LOG(ERROR) << "Failed to stop the service.";
    return kErrorExitCode;
  }

  // Try to delete the service now.
  if (!DeleteService(service)) {
    LOG_GETLASTERROR(ERROR) << "Failed to delete the service";
    return kErrorExitCode;
  }

  printf("The service has been removed successfully.\n");
  return kSuccessExitCode;
}

int HostService::Run() {
  return (this->*run_routine_)();
}

int HostService::RunAsService() {
  NOTIMPLEMENTED();
  return 0;
}

int HostService::RunInConsole() {
  NOTIMPLEMENTED();
  return 0;
}

} // namespace remoting

int main(int argc, char** argv) {
  CommandLine::Init(argc, argv);

  // This object instance is required by Chrome code (for example,
  // FilePath, LazyInstance, MessageLoop).
  base::AtExitManager exit_manager;

  const CommandLine* command_line = CommandLine::ForCurrentProcess();

  if (command_line->HasSwitch(kHelpSwitchName) ||
      command_line->HasSwitch(kQuestionSwitchName)) {
    usage(argv[0]);
    return kSuccessExitCode;
  }

  remoting::HostService service;
  if (!service.InitWithCommandLine(command_line)) {
    usage(argv[0]);
    return kUsageExitCode;
  }

  return service.Run();
}
