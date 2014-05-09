// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/server_log_entry.h"

#include "base/logging.h"
#include "base/macros.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringize_macros.h"
#include "base/strings/stringprintf.h"
#include "base/sys_info.h"
#include "remoting/base/constants.h"
#include "remoting/client/chromoting_stats.h"
#include "remoting/protocol/connection_to_host.h"
#include "remoting/protocol/errors.h"
#include "third_party/libjingle/source/talk/xmllite/xmlelement.h"

using base::StringPrintf;
using base::SysInfo;
using buzz::QName;
using buzz::XmlElement;
using remoting::protocol::ConnectionToHost;

namespace remoting {

namespace client {

namespace {
const char kLogCommand[] = "log";

const char kLogEntry[] = "entry";

const char kKeyEventName[] = "event-name";
const char kValueEventNameSessionState[] = "session-state";
const char kValueEventNameStatistics[] = "connection-statistics";
const char kValueEventNameSessionIdOld[] = "session-id-old";
const char kValueEventNameSessionIdNew[] = "session-id-new";

const char kKeyRole[] = "role";
const char kValueRoleClient[] = "client";

const char kKeyMode[] = "mode";
const char kValueModeIt2Me[] = "it2me";
const char kValueModeMe2Me[] = "me2me";

const char kKeySessionState[] = "session-state";
const char kValueSessionStateConnected[] = "connected";
const char kValueSessionStateClosed[] = "closed";

const char kKeyOsName[] = "os-name";
const char kKeyOsVersion[] = "os-version";
const char kKeyAppVersion[] = "app-version";

const char kKeyCpu[] = "cpu";

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
    protocol::ConnectionToHost::State state,
    protocol::ErrorCode error) {
  scoped_ptr<ServerLogEntry> entry(new ServerLogEntry());
  entry->Set(kKeyRole, kValueRoleClient);
  entry->Set(kKeyEventName, kValueEventNameSessionState);

  entry->Set(kKeySessionState, GetValueSessionState(state));
  if (error != protocol::OK) {
    entry->Set("connection-error", GetValueError(error));
  }

  return entry.Pass();
}

// static
scoped_ptr<ServerLogEntry> ServerLogEntry::MakeForStatistics(
    ChromotingStats* statistics) {
  scoped_ptr<ServerLogEntry> entry(new ServerLogEntry());
  entry->Set(kKeyRole, kValueRoleClient);
  entry->Set(kKeyEventName, kValueEventNameStatistics);

  entry->Set("video-bandwidth",
             StringPrintf("%.2f", statistics->video_bandwidth()->Rate()));
  entry->Set("capture-latency",
             StringPrintf("%.2f", statistics->video_capture_ms()->Average()));
  entry->Set("encode-latency",
             StringPrintf("%.2f", statistics->video_encode_ms()->Average()));
  entry->Set("decode-latency",
             StringPrintf("%.2f", statistics->video_decode_ms()->Average()));
  entry->Set("render-latency",
             StringPrintf("%.2f", statistics->video_frame_rate()->Rate()));
  entry->Set("roundtrip-latency",
             StringPrintf("%.2f", statistics->round_trip_ms()->Average()));

  return entry.Pass();
}

// static
scoped_ptr<ServerLogEntry> ServerLogEntry::MakeForSessionIdOld(
    const std::string& session_id) {
  scoped_ptr<ServerLogEntry> entry(new ServerLogEntry());
  entry->Set(kKeyRole, kValueRoleClient);
  entry->Set(kKeyEventName, kValueEventNameSessionIdOld);
  entry->AddSessionId(session_id);
  return entry.Pass();
}

// static
scoped_ptr<ServerLogEntry> ServerLogEntry::MakeForSessionIdNew(
    const std::string& session_id) {
  scoped_ptr<ServerLogEntry> entry(new ServerLogEntry());
  entry->Set(kKeyRole, kValueRoleClient);
  entry->Set(kKeyEventName, kValueEventNameSessionIdNew);
  entry->AddSessionId(session_id);
  return entry.Pass();
}

void ServerLogEntry::AddClientFields() {
  Set(kKeyOsName, SysInfo::OperatingSystemName());
  Set(kKeyOsVersion, SysInfo::OperatingSystemVersion());
  Set(kKeyAppVersion, STRINGIZE(VERSION));
  Set(kKeyCpu, SysInfo::OperatingSystemArchitecture());
};

void ServerLogEntry::AddModeField(ServerLogEntry::Mode mode) {
  Set(kKeyMode, GetValueMode(mode));
}

void ServerLogEntry::AddSessionId(const std::string& session_id) {
  Set("session-id", session_id);
}

void ServerLogEntry::AddSessionDuration(base::TimeDelta duration) {
  Set("session-duration", base::Int64ToString(duration.InSeconds()));
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
const char* ServerLogEntry::GetValueSessionState(
    ConnectionToHost::State state) {
  switch (state) {
    // Where possible, these are the same strings that the webapp sends for the
    // corresponding state - see remoting/webapp/server_log_entry.js.
    case ConnectionToHost::INITIALIZING:
      return "initializing";
    case ConnectionToHost::CONNECTING:
      return "connecting";
    case ConnectionToHost::AUTHENTICATED:
      return "authenticated";
    case ConnectionToHost::CONNECTED:
      return kValueSessionStateConnected;
    case ConnectionToHost::FAILED:
      return "connection-failed";
    case ConnectionToHost::CLOSED:
      return kValueSessionStateClosed;
    default:
      NOTREACHED();
      return NULL;
  }
}

// static
const char* ServerLogEntry::GetValueError(protocol::ErrorCode error) {
  switch (error) {
    // Where possible, these are the same strings that the webapp sends for the
    // corresponding error - see remoting/webapp/server_log_entry.js.
    case protocol::OK:
      return "none";
    case protocol::PEER_IS_OFFLINE:
      return "host-is-offline";
    case protocol::SESSION_REJECTED:
      return "session-rejected";
    case protocol::INCOMPATIBLE_PROTOCOL:
      return "incompatible-protocol";
    case protocol::AUTHENTICATION_FAILED:
      return "authentication-failed";
    case protocol::CHANNEL_CONNECTION_ERROR:
      return "channel-connection-error";
    case protocol::SIGNALING_ERROR:
      return "signaling-error";
    case protocol::SIGNALING_TIMEOUT:
      return "signaling-timeout";
    case protocol::HOST_OVERLOAD:
      return "host-overload";
    case protocol::UNKNOWN_ERROR:
      return "unknown-error";
    default:
      NOTREACHED();
      return NULL;
  }
}

void ServerLogEntry::AddEventName(const std::string& event_name) {
  Set("event-name", event_name);
}

void ServerLogEntry::Set(const std::string& key, const std::string& value) {
  values_map_[key] = value;
}

}  // namespace client

}  // namespace remoting
