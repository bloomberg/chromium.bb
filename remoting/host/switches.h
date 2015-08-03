// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_SWITCHES_H_
#define REMOTING_HOST_SWITCHES_H_

namespace remoting {

// The command line switch specifying the daemon IPC endpoint.
extern const char kDaemonPipeSwitchName[];

extern const char kElevateSwitchName[];

// "--help" prints the usage message.
extern const char kHelpSwitchName[];

// Used to specify the type of the process. kProcessType* constants specify
// possible values.
extern const char kProcessTypeSwitchName[];

// "--?" prints the usage message.
extern const char kQuestionSwitchName[];

// The command line switch used to get version of the daemon.
extern const char kVersionSwitchName[];

// Values for kProcessTypeSwitchName.
extern const char kProcessTypeController[];
extern const char kProcessTypeDaemon[];
extern const char kProcessTypeDesktop[];
extern const char kProcessTypeHost[];
extern const char kProcessTypeRdpDesktopSession[];

}  // namespace remoting

#endif  // REMOTING_HOST_SWITCHES_H_
