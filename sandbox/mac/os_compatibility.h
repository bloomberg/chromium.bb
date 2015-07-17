// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is used to handle differences in Mach message IDs and structures
// that occur between different OS versions. The Mach messages that the sandbox
// is interested in are decoded using information derived from the open-source
// libraries, i.e. <http://www.opensource.apple.com/source/launchd/>. While
// these messages definitions are open source, they are not considered part of
// the stable OS API, and so differences do exist between OS versions.

#ifndef SANDBOX_MAC_OS_COMPATIBILITY_H_
#define SANDBOX_MAC_OS_COMPATIBILITY_H_

#include <mach/mach.h>

#include <string>

#include "sandbox/mac/message_server.h"

namespace sandbox {

using IPCMessageGetID = uint64_t (*)(const IPCMessage);

using IPCMessageRouter = bool (*)(const IPCMessage);

using LookUp2GetRequestName = std::string (*)(const IPCMessage);
using LookUp2FillReply = void (*)(IPCMessage, mach_port_t service_port);

using SwapIntegerIsGetOnly = bool (*)(const IPCMessage);

struct LaunchdCompatibilityShim {
  // Gets the message and subsystem ID of an IPC message.
  IPCMessageGetID ipc_message_get_subsystem;
  IPCMessageGetID ipc_message_get_id;

  // Returns true if the message is a Launchd look up request.
  IPCMessageRouter is_launchd_look_up;

  // Returns true if the message is a Launchd vproc swap_integer request.
  IPCMessageRouter is_launchd_vproc_swap_integer;

  // Returns true if the message is an XPC domain management message.
  IPCMessageRouter is_xpc_domain_management;

  // A function to take a look_up2 message and return the string service name
  // that was requested via the message.
  LookUp2GetRequestName look_up2_get_request_name;

  // A function to formulate a reply to a look_up2 message, given the reply
  // message and the port to return as the service.
  LookUp2FillReply look_up2_fill_reply;

  // A function to take a swap_integer message and return true if the message
  // is only getting the value of a key, neither setting it directly, nor
  // swapping two keys.
  SwapIntegerIsGetOnly swap_integer_is_get_only;
};

// Gets the compatibility shim for the launchd job subsystem.
const LaunchdCompatibilityShim GetLaunchdCompatibilityShim();

}  // namespace sandbox

#endif  // SANDBOX_MAC_OS_COMPATIBILITY_H_
