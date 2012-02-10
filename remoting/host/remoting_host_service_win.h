// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_REMOTING_HOST_SERVICE_WIN_H_
#define REMOTING_HOST_REMOTING_HOST_SERVICE_WIN_H_

#include <windows.h>

class CommandLine;

namespace remoting {

class HostService {
 public:
  HostService();
  ~HostService();

  // This function parses the command line and selects the action routine.
  bool InitWithCommandLine(const CommandLine* command_line);

  // Invoke the choosen action routine.
  int Run();

 private:
  // This routine registers the service with the service control manager.
  int Install();

  static VOID CALLBACK OnServiceStopped(PVOID context);

  // This routine uninstalls the service previously regerested by Install().
  int Remove();

  int RunAsService();
  int RunInConsole();

  int (HostService::*run_routine_)();
};

}  // namespace remoting

#endif  // REMOTING_HOST_REMOTING_HOST_SERVICE_WIN_H_
