// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/server_log_entry.h"

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
const char kLogEntry[] = "entry";

const char kKeyEventName[] = "event-name";
const char kValueEventNameSessionState[] = "session-state";

const char kKeyRole[] = "role";
const char kValueRoleHost[] = "host";

const char kKeySessionState[] = "session-state";
const char kValueSessionStateConnected[] = "connected";
const char kValueSessionStateClosed[] = "closed";

const char kKeyOsName[] = "os-name";
const char kValueOsNameWindows[] = "Windows";
const char kValueOsNameLinux[] = "Linux";
const char kValueOsNameMac[] = "Mac";
const char kValueOsNameChromeOS[] = "ChromeOS";

const char kKeyOsVersion[] = "os-version";

const char kKeyCpu[] = "cpu";
}  // namespace

ServerLogEntry::ServerLogEntry() {
}

ServerLogEntry::~ServerLogEntry() {
}

ServerLogEntry* ServerLogEntry::MakeSessionStateChange(bool connected) {
  ServerLogEntry* entry = new ServerLogEntry();
  entry->Set(kKeyRole, kValueRoleHost);
  entry->Set(kKeyEventName, kValueEventNameSessionState);
  entry->Set(kKeySessionState, GetValueSessionState(connected));
  return entry;
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
  Set(kKeyOsVersion, os_version.str().c_str());
#endif

  Set(kKeyCpu, SysInfo::CPUArchitecture().c_str());
};

XmlElement* ServerLogEntry::ToStanza() const {
  XmlElement* stanza = new XmlElement(QName(
      kChromotingXmlNamespace, kLogEntry));
  ValuesMap::const_iterator iter;
  for (iter = values_map_.begin(); iter != values_map_.end(); ++iter) {
    stanza->AddAttr(QName("", iter->first), iter->second);
  }
  return stanza;
}

const char* ServerLogEntry::GetValueSessionState(bool connected) {
  return connected ? kValueSessionStateConnected : kValueSessionStateClosed;
}

void ServerLogEntry::Set(const char* key, const char* value) {
  values_map_[key] = value;
}

}  // namespace remoting
