// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/server_log_entry.h"

#include "base/logging.h"
#include "base/strings/stringize_macros.h"
#include "base/sys_info.h"
#include "remoting/base/constants.h"
#include "remoting/protocol/session.h"
#include "third_party/libjingle/source/talk/xmllite/xmlelement.h"

using base::SysInfo;
using buzz::QName;
using buzz::XmlElement;
using remoting::protocol::Session;

namespace remoting {

namespace {
const char kLogCommand[] = "log";

const char kLogEntry[] = "entry";

const char kKeyEventName[] = "event-name";
const char kValueEventNameSessionState[] = "session-state";
const char kValueEventNameHeartbeat[] = "heartbeat";

const char kKeyRole[] = "role";
const char kValueRoleHost[] = "host";

const char kKeyMode[] = "mode";
const char kValueModeIt2Me[] = "it2me";
const char kValueModeMe2Me[] = "me2me";

const char kKeySessionState[] = "session-state";
const char kValueSessionStateConnected[] = "connected";
const char kValueSessionStateClosed[] = "closed";

const char kKeyOsName[] = "os-name";
const char kValueOsNameWindows[] = "Windows";
const char kValueOsNameLinux[] = "Linux";
const char kValueOsNameMac[] = "Mac";
const char kValueOsNameChromeOS[] = "ChromeOS";

const char kKeyOsVersion[] = "os-version";

const char kKeyHostVersion[] = "host-version";

const char kKeyCpu[] = "cpu";

const char kKeyConnectionType[] = "connection-type";

}  // namespace

ServerLogEntry::ServerLogEntry() {
}

ServerLogEntry::~ServerLogEntry() {
}

// static
scoped_ptr<buzz::XmlElement> ServerLogEntry::MakeStanza() {
  return scoped_ptr<buzz::XmlElement>(
      new XmlElement(QName(kChromotingXmlNamespace, kLogCommand)));
}

// static
scoped_ptr<ServerLogEntry> ServerLogEntry::MakeForSessionStateChange(
    bool connected) {
  scoped_ptr<ServerLogEntry> entry(new ServerLogEntry());
  entry->Set(kKeyRole, kValueRoleHost);
  entry->Set(kKeyEventName, kValueEventNameSessionState);
  entry->Set(kKeySessionState, GetValueSessionState(connected));
  return entry.Pass();
}

// static
scoped_ptr<ServerLogEntry> ServerLogEntry::MakeForHeartbeat() {
  scoped_ptr<ServerLogEntry> entry(new ServerLogEntry());
  entry->Set(kKeyRole, kValueRoleHost);
  entry->Set(kKeyEventName, kValueEventNameHeartbeat);
  return entry.Pass();
}

void ServerLogEntry::AddHostFields() {
#if defined(OS_WIN)
  Set(kKeyOsName, kValueOsNameWindows);
#elif defined(OS_MACOSX)
  Set(kKeyOsName, kValueOsNameMac);
#elif defined(OS_CHROMEOS)
  Set(kKeyOsName, kValueOsNameChromeOS);
#elif defined(OS_LINUX)
  Set(kKeyOsName, kValueOsNameLinux);
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
  Set(kKeyOsVersion, os_version.str());
#endif

  Set(kKeyHostVersion, STRINGIZE(VERSION));
  Set(kKeyCpu, SysInfo::OperatingSystemArchitecture());
};

void ServerLogEntry::AddModeField(ServerLogEntry::Mode mode) {
  Set(kKeyMode, GetValueMode(mode));
}

void ServerLogEntry::AddConnectionTypeField(
    protocol::TransportRoute::RouteType type) {
  Set(kKeyConnectionType, protocol::TransportRoute::GetTypeString(type));
}

// static
const char* ServerLogEntry::GetValueMode(ServerLogEntry::Mode mode) {
  switch (mode) {
    case IT2ME:
      return kValueModeIt2Me;
    case ME2ME:
      return kValueModeMe2Me;
    default:
      NOTREACHED();
      return NULL;
  }
}

scoped_ptr<XmlElement> ServerLogEntry::ToStanza() const {
  scoped_ptr<XmlElement> stanza(new XmlElement(QName(
      kChromotingXmlNamespace, kLogEntry)));
  ValuesMap::const_iterator iter;
  for (iter = values_map_.begin(); iter != values_map_.end(); ++iter) {
    stanza->AddAttr(QName(std::string(), iter->first), iter->second);
  }
  return stanza.Pass();
}

// static
const char* ServerLogEntry::GetValueSessionState(bool connected) {
  return connected ? kValueSessionStateConnected : kValueSessionStateClosed;
}

void ServerLogEntry::Set(const std::string& key, const std::string& value) {
  values_map_[key] = value;
}

}  // namespace remoting
