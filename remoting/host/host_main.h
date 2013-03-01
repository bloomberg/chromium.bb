// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_ENTRY_POINT_H_
#define REMOTING_HOST_ENTRY_POINT_H_

#include "remoting/host/host_export.h"

namespace remoting {

// "--elevate=<binary>" requests |binary| to be launched elevated (possibly
// causing a UAC prompt).
extern const char kElevateSwitchName[];

// "--type=<type>" specifies the kind of process to run.
extern const char kProcessTypeSwitchName[];

extern const char kProcessTypeHost[];

#if defined(REMOTING_MULTI_PROCESS)
extern const char kProcessTypeDaemon[];
extern const char kProcessTypeDesktop[];
#endif  // defined(REMOTING_MULTI_PROCESS)

#if defined(OS_WIN)
extern const char kProcessTypeController[];
extern const char kProcessTypeRdpDesktopSession[];
#endif  // defined(OS_WIN)

// The common entry point exported from remoting_core.dll. It uses
// "--type==<type>" command line parameter to determine the kind of process it
// needs to run.
HOST_EXPORT int HostMain(int argc, char** argv);

} // namespace remoting

#endif  // REMOTING_HOST_ENTRY_POINT_H_
