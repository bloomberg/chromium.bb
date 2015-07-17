// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/mac/os_compatibility.h"

#include <servers/bootstrap.h>
#include <unistd.h>

#include "base/mac/mac_util.h"
#include "sandbox/mac/xpc.h"

namespace sandbox {

namespace {

#pragma pack(push, 4)
// Verified from launchd-329.3.3 (10.6.8).
struct look_up2_request_10_6 {
  mach_msg_header_t Head;
  NDR_record_t NDR;
  name_t servicename;
  pid_t targetpid;
  uint64_t flags;
};

struct look_up2_reply_10_6 {
  mach_msg_header_t Head;
  mach_msg_body_t msgh_body;
  mach_msg_port_descriptor_t service_port;
};

// Verified from:
//   launchd-392.39 (10.7.5)
//   launchd-442.26.2 (10.8.5)
//   launchd-842.1.4 (10.9.0)
struct look_up2_request_10_7 {
  mach_msg_header_t Head;
  NDR_record_t NDR;
  name_t servicename;
  pid_t targetpid;
  uuid_t instanceid;
  uint64_t flags;
};

// look_up2_reply_10_7 is the same as the 10_6 version.

// Verified from:
//   launchd-329.3.3 (10.6.8)
//   launchd-392.39 (10.7.5)
//   launchd-442.26.2 (10.8.5)
//   launchd-842.1.4 (10.9.0)
typedef int vproc_gsk_t;  // Defined as an enum in liblaunch/vproc_priv.h.
struct swap_integer_request_10_6 {
  mach_msg_header_t Head;
  NDR_record_t NDR;
  vproc_gsk_t inkey;
  vproc_gsk_t outkey;
  int64_t inval;
};
#pragma pack(pop)

// TODO(rsesek): Libc provides strnlen() starting in 10.7.
size_t strnlen(const char* str, size_t maxlen) {
  size_t len = 0;
  for (; len < maxlen; ++len, ++str) {
    if (*str == '\0')
      break;
  }
  return len;
}

bool FalseMessageRouter(const IPCMessage message) {
  return false;
}

uint64_t MachGetMessageSubsystem(const IPCMessage message) {
  return (message.mach->msgh_id / 100) * 100;
}

uint64_t MachGetMessageID(const IPCMessage message) {
  return message.mach->msgh_id;
}

bool MachIsLaunchdLookUp(const IPCMessage message) {
  return MachGetMessageID(message) == 404;
}

bool MachIsLauchdVprocSwapInteger(const IPCMessage message) {
  return MachGetMessageID(message) == 416;
}

template <typename R>
std::string LaunchdLookUp2GetRequestName(const IPCMessage message) {
  mach_msg_header_t* header = message.mach;
  DCHECK_EQ(sizeof(R), header->msgh_size);
  const R* request = reinterpret_cast<const R*>(header);
  // Make sure the name is properly NUL-terminated.
  const size_t name_length =
      strnlen(request->servicename, BOOTSTRAP_MAX_NAME_LEN);
  std::string name = std::string(request->servicename, name_length);
  return name;
}

template <typename R>
void LaunchdLookUp2FillReply(IPCMessage message, mach_port_t port) {
  R* reply = reinterpret_cast<R*>(message.mach);
  reply->Head.msgh_size = sizeof(R);
  reply->Head.msgh_bits =
      MACH_MSGH_BITS_REMOTE(MACH_MSG_TYPE_MOVE_SEND_ONCE) |
      MACH_MSGH_BITS_COMPLEX;
  reply->msgh_body.msgh_descriptor_count = 1;
  reply->service_port.name = port;
  reply->service_port.disposition = MACH_MSG_TYPE_COPY_SEND;
  reply->service_port.type = MACH_MSG_PORT_DESCRIPTOR;
}

template <typename R>
bool LaunchdSwapIntegerIsGetOnly(const IPCMessage message) {
  const R* request = reinterpret_cast<const R*>(message.mach);
  return request->inkey == 0 && request->inval == 0 && request->outkey != 0;
}

uint64_t XPCGetMessageSubsystem(const IPCMessage message) {
  return xpc_dictionary_get_uint64(message.xpc, "subsystem");
}

uint64_t XPCGetMessageID(const IPCMessage message) {
  return xpc_dictionary_get_uint64(message.xpc, "routine");
}

bool XPCIsLaunchdLookUp(const IPCMessage message) {
  uint64_t subsystem = XPCGetMessageSubsystem(message);
  uint64_t id = XPCGetMessageID(message);
  // Lookup requests in XPC can either go through the Mach bootstrap subsytem
  // (5) from bootstrap_look_up(), or the XPC domain subsystem (3) for
  // xpc_connection_create(). Both use the same message format.
  return (subsystem == 5 && id == 207) || (subsystem == 3 && id == 804);
}

bool XPCIsLaunchdVprocSwapInteger(const IPCMessage message) {
  return XPCGetMessageSubsystem(message) == 6 &&
         XPCGetMessageID(message) == 301;
}

bool XPCIsDomainManagement(const IPCMessage message) {
  return XPCGetMessageSubsystem(message) == 3;
}

std::string XPCLaunchdLookUpGetRequestName(const IPCMessage message) {
  const char* name = xpc_dictionary_get_string(message.xpc, "name");
  const size_t name_length = strnlen(name, BOOTSTRAP_MAX_NAME_LEN);
  return std::string(name, name_length);
}

bool XPCLaunchdSwapIntegerIsGetOnly(const IPCMessage message) {
  return xpc_dictionary_get_bool(message.xpc, "set") == false &&
         xpc_dictionary_get_uint64(message.xpc, "ingsk") == 0 &&
         xpc_dictionary_get_int64(message.xpc, "in") == 0;
}

void XPCLaunchdLookUpFillReply(IPCMessage message, mach_port_t port) {
  xpc_dictionary_set_mach_send(message.xpc, "port", port);
}

}  // namespace

const LaunchdCompatibilityShim GetLaunchdCompatibilityShim() {
  LaunchdCompatibilityShim shim = {
    .ipc_message_get_subsystem = &MachGetMessageSubsystem,
    .ipc_message_get_id = &MachGetMessageID,
    .is_launchd_look_up = &MachIsLaunchdLookUp,
    .is_launchd_vproc_swap_integer = &MachIsLauchdVprocSwapInteger,
    .is_xpc_domain_management = &FalseMessageRouter,
    .look_up2_fill_reply = &LaunchdLookUp2FillReply<look_up2_reply_10_6>,
    .swap_integer_is_get_only =
        &LaunchdSwapIntegerIsGetOnly<swap_integer_request_10_6>,
  };

  if (base::mac::IsOSSnowLeopard()) {
    shim.look_up2_get_request_name =
        &LaunchdLookUp2GetRequestName<look_up2_request_10_6>;
  } else if (base::mac::IsOSLionOrLater() &&
             base::mac::IsOSMavericksOrEarlier()) {
    shim.look_up2_get_request_name =
        &LaunchdLookUp2GetRequestName<look_up2_request_10_7>;
  } else if (base::mac::IsOSYosemite()) {
    shim.ipc_message_get_subsystem = &XPCGetMessageSubsystem;
    shim.ipc_message_get_id = &XPCGetMessageID;
    shim.is_launchd_look_up = &XPCIsLaunchdLookUp;
    shim.is_launchd_vproc_swap_integer = &XPCIsLaunchdVprocSwapInteger;
    shim.is_xpc_domain_management = &XPCIsDomainManagement;
    shim.look_up2_get_request_name = &XPCLaunchdLookUpGetRequestName;
    shim.look_up2_fill_reply = &XPCLaunchdLookUpFillReply;
    shim.swap_integer_is_get_only = &XPCLaunchdSwapIntegerIsGetOnly;
  } else {
    DLOG(ERROR) << "Unknown OS, using launchd compatibility shim from 10.7.";
    shim.look_up2_get_request_name =
        &LaunchdLookUp2GetRequestName<look_up2_request_10_7>;
  }

  return shim;
}

}  // namespace sandbox
