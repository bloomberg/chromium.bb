// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/server_log_entry_client.h"

#include <memory>

#include "base/strings/stringize_macros.h"
#include "base/sys_info.h"
#include "remoting/protocol/performance_tracker.h"
#include "remoting/signaling/server_log_entry.h"
#include "remoting/signaling/server_log_entry_unittest.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libjingle_xmpp/xmllite/xmlelement.h"

using base::SysInfo;
using buzz::XmlAttr;
using buzz::XmlElement;
using remoting::protocol::ConnectionToHost;

namespace remoting {

TEST(ServerLogEntryClientTest, SessionStateChange) {
  std::unique_ptr<ServerLogEntry> entry(MakeLogEntryForSessionStateChange(
      ConnectionToHost::CONNECTED, remoting::protocol::OK));
  std::unique_ptr<XmlElement> stanza = entry->ToStanza();
  std::string error;
  std::map<std::string, std::string> key_value_pairs;
  key_value_pairs["role"] = "client";
  key_value_pairs["event-name"] = "session-state";
  key_value_pairs["session-state"] = "connected";
  std::set<std::string> keys;
  ASSERT_TRUE(VerifyStanza(key_value_pairs, keys, stanza.get(), &error))
      << error;
}

TEST(ServerLogEntryClientTest, SessionStateChangeWithError) {
  std::unique_ptr<ServerLogEntry> entry(MakeLogEntryForSessionStateChange(
      ConnectionToHost::FAILED, remoting::protocol::PEER_IS_OFFLINE));
  std::unique_ptr<XmlElement> stanza = entry->ToStanza();
  std::string error;
  std::map<std::string, std::string> key_value_pairs;
  key_value_pairs["role"] = "client";
  key_value_pairs["event-name"] = "session-state";
  key_value_pairs["session-state"] = "connection-failed";
  key_value_pairs["connection-error"] = "host-is-offline";
  std::set<std::string> keys;
  ASSERT_TRUE(VerifyStanza(key_value_pairs, keys, stanza.get(), &error))
      << error;
}

TEST(ServerLogEntryClientTest, Statistics) {
  protocol::PerformanceTracker perf_tracker;
  std::unique_ptr<ServerLogEntry> entry(
      MakeLogEntryForStatistics(&perf_tracker));
  std::unique_ptr<XmlElement> stanza = entry->ToStanza();
  std::string error;
  std::map<std::string, std::string> key_value_pairs;
  key_value_pairs["role"] = "client";
  key_value_pairs["event-name"] = "connection-statistics";
  std::set<std::string> keys;
  keys.insert("video-bandwidth");
  keys.insert("capture-latency");
  keys.insert("encode-latency");
  keys.insert("decode-latency");
  keys.insert("render-latency");
  keys.insert("roundtrip-latency");
  ASSERT_TRUE(VerifyStanza(key_value_pairs, keys, stanza.get(), &error))
      << error;
}

TEST(ServerLogEntryClientTest, SessionIdChanged) {
  std::unique_ptr<ServerLogEntry> entry(MakeLogEntryForSessionIdOld("abc"));
  std::unique_ptr<XmlElement> stanza = entry->ToStanza();
  std::string error;
  std::map<std::string, std::string> key_value_pairs;
  key_value_pairs["role"] = "client";
  key_value_pairs["event-name"] = "session-id-old";
  key_value_pairs["session-id"] = "abc";
  std::set<std::string> keys;
  ASSERT_TRUE(VerifyStanza(key_value_pairs, keys, stanza.get(), &error))
      << error;

  entry = MakeLogEntryForSessionIdNew("def");
  stanza = entry->ToStanza();
  key_value_pairs["event-name"] = "session-id-new";
  key_value_pairs["session-id"] = "def";
  ASSERT_TRUE(VerifyStanza(key_value_pairs, keys, stanza.get(), &error))
      << error;
}

TEST(ServerLogEntryClientTest, AddClientFields) {
  std::unique_ptr<ServerLogEntry> entry(MakeLogEntryForSessionStateChange(
      ConnectionToHost::CONNECTED, remoting::protocol::OK));
  AddClientFieldsToLogEntry(entry.get());
  std::unique_ptr<XmlElement> stanza = entry->ToStanza();
  std::string error;
  std::map<std::string, std::string> key_value_pairs;
  key_value_pairs["role"] = "client";
  key_value_pairs["event-name"] = "session-state";
  key_value_pairs["session-state"] = "connected";
  key_value_pairs["os-name"] = SysInfo::OperatingSystemName();
  key_value_pairs["os-version"] = SysInfo::OperatingSystemVersion();
  key_value_pairs["app-version"] = STRINGIZE(VERSION);
  key_value_pairs["cpu"] = SysInfo::OperatingSystemArchitecture();
  std::set<std::string> keys;
  ASSERT_TRUE(VerifyStanza(key_value_pairs, keys, stanza.get(), &error)) <<
      error;
}

TEST(ServerLogEntryClientTest, AddSessionDuration) {
  std::unique_ptr<ServerLogEntry> entry(MakeLogEntryForSessionStateChange(
      ConnectionToHost::CONNECTED, remoting::protocol::OK));
  AddSessionDurationToLogEntry(entry.get(), base::TimeDelta::FromSeconds(123));
  std::unique_ptr<XmlElement> stanza = entry->ToStanza();
  std::string error;
  std::map<std::string, std::string> key_value_pairs;
  key_value_pairs["role"] = "client";
  key_value_pairs["event-name"] = "session-state";
  key_value_pairs["session-state"] = "connected";
  key_value_pairs["session-duration"] = "123";
  std::set<std::string> keys;
  ASSERT_TRUE(VerifyStanza(key_value_pairs, keys, stanza.get(), &error))
      << error;
}

}  // namespace remoting
