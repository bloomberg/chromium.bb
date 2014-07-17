// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/server_log_entry_host.h"

#include "base/strings/stringize_macros.h"
#include "base/sys_info.h"
#include "remoting/signaling/server_log_entry.h"

using base::SysInfo;

namespace remoting {

namespace {
const char kValueEventNameSessionState[] = "session-state";
const char kValueEventNameHeartbeat[] = "heartbeat";
const char kValueEventNameHostStatus[] = "host-status";

const char kValueRoleHost[] = "host";

const char kKeySessionState[] = "session-state";
const char kValueSessionStateConnected[] = "connected";
const char kValueSessionStateClosed[] = "closed";

const char kStatusName[] = "status";
const char kExitCodeName[] = "exit-code";

const char kKeyOsName[] = "os-name";

#if defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_CHROMEOS)
const char kKeyOsVersion[] = "os-version";
#endif

const char kKeyHostVersion[] = "host-version";

const char kKeyConnectionType[] = "connection-type";

const char* GetValueSessionState(bool connected) {
  return connected ? kValueSessionStateConnected : kValueSessionStateClosed;
}

}  // namespace

scoped_ptr<ServerLogEntry> MakeLogEntryForSessionStateChange(
    bool connected) {
  scoped_ptr<ServerLogEntry> entry(new ServerLogEntry());
  entry->AddRoleField(kValueRoleHost);
  entry->AddEventNameField(kValueEventNameSessionState);
  entry->Set(kKeySessionState, GetValueSessionState(connected));
  return entry.Pass();
}

scoped_ptr<ServerLogEntry> MakeLogEntryForHeartbeat() {
  scoped_ptr<ServerLogEntry> entry(new ServerLogEntry());
  entry->AddRoleField(kValueRoleHost);
  entry->AddEventNameField(kValueEventNameHeartbeat);
  return entry.Pass();
}

// static
scoped_ptr<ServerLogEntry> MakeLogEntryForHostStatus(
    HostStatusSender::HostStatus host_status, HostExitCodes exit_code) {
  scoped_ptr<ServerLogEntry> entry(new ServerLogEntry());
  entry->AddRoleField(kValueRoleHost);
  entry->AddEventNameField(kValueEventNameHostStatus);
  entry->Set(kStatusName, HostStatusSender::HostStatusToString(host_status));
  if (host_status == HostStatusSender::OFFLINE)
    entry->Set(kExitCodeName, ExitCodeToString(exit_code));
  return entry.Pass();
}

void AddHostFieldsToLogEntry(ServerLogEntry* entry) {
#if defined(OS_WIN)
  entry->Set(kKeyOsName, "Windows");
#elif defined(OS_MACOSX)
  entry->Set(kKeyOsName, "Mac");
#elif defined(OS_CHROMEOS)
  entry->Set(kKeyOsName, "ChromeOS");
#elif defined(OS_LINUX)
  entry->Set(kKeyOsName, "Linux");
#endif

  // SysInfo::OperatingSystemVersionNumbers is only defined for the following
  // OSes: see base/sys_info_unittest.cc.
#if defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_CHROMEOS)
  std::stringstream os_version;
  int32 os_major_version = 0;
  int32 os_minor_version = 0;
  int32 os_bugfix_version = 0;
  SysInfo::OperatingSystemVersionNumbers(&os_major_version, &os_minor_version,
                                         &os_bugfix_version);
  os_version << os_major_version << "." << os_minor_version << "."
             << os_bugfix_version;
  entry->Set(kKeyOsVersion, os_version.str());
#endif

  entry->Set(kKeyHostVersion, STRINGIZE(VERSION));
  entry->AddCpuField();
};

void AddConnectionTypeToLogEntry(ServerLogEntry* entry,
    protocol::TransportRoute::RouteType type) {
  entry->Set(kKeyConnectionType, protocol::TransportRoute::GetTypeString(type));
}

}  // namespace remoting
