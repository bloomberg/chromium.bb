// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "base/strings/stringize_macros.h"
#include "remoting/host/server_log_entry_host.h"
#include "remoting/signaling/server_log_entry.h"
#include "remoting/signaling/server_log_entry_unittest.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libjingle/source/talk/xmllite/xmlelement.h"

using buzz::XmlAttr;
using buzz::XmlElement;

namespace remoting {

TEST(ServerLogEntryHostTest, MakeForSessionStateChange) {
  scoped_ptr<ServerLogEntry> entry(MakeLogEntryForSessionStateChange(true));
  scoped_ptr<XmlElement> stanza = entry->ToStanza();
  std::string error;
  std::map<std::string, std::string> key_value_pairs;
  key_value_pairs["role"] = "host";
  key_value_pairs["event-name"] = "session-state";
  key_value_pairs["session-state"] = "connected";
  std::set<std::string> keys;
  ASSERT_TRUE(VerifyStanza(key_value_pairs, keys, stanza.get(), &error))
      << error;
}

TEST(ServerLogEntryHostTest, MakeForHeartbeat) {
  scoped_ptr<ServerLogEntry> entry(MakeLogEntryForHeartbeat());
  scoped_ptr<XmlElement> stanza = entry->ToStanza();
  std::string error;
  std::map<std::string, std::string> key_value_pairs;
  key_value_pairs["role"] = "host";
  key_value_pairs["event-name"] = "heartbeat";
  std::set<std::string> keys;
  ASSERT_TRUE(VerifyStanza(key_value_pairs, keys, stanza.get(), &error))
      << error;
}

TEST(ServerLogEntryHostTest, AddHostFields) {
  scoped_ptr<ServerLogEntry> entry(MakeLogEntryForSessionStateChange(true));
  AddHostFieldsToLogEntry(entry.get());
  scoped_ptr<XmlElement> stanza = entry->ToStanza();
  std::string error;
  std::map<std::string, std::string> key_value_pairs;
  key_value_pairs["role"] = "host";
  key_value_pairs["event-name"] = "session-state";
  key_value_pairs["session-state"] = "connected";
  std::set<std::string> keys;
  keys.insert("cpu");
#if defined(OS_WIN)
  key_value_pairs["os-name"] = "Windows";
  keys.insert("os-version");
#elif defined(OS_MACOSX)
  key_value_pairs["os-name"] = "Mac";
  keys.insert("os-version");
#elif defined(OS_CHROMEOS)
  key_value_pairs["os-name"] = "ChromeOS";
  keys.insert("os-version");
#elif defined(OS_LINUX)
  key_value_pairs["os-name"] = "Linux";
#endif
  key_value_pairs["host-version"] = STRINGIZE(VERSION);
  ASSERT_TRUE(VerifyStanza(key_value_pairs, keys, stanza.get(), &error)) <<
      error;
}

TEST(ServerLogEntryHostTest, AddModeField1) {
  scoped_ptr<ServerLogEntry> entry(MakeLogEntryForSessionStateChange(true));
  entry->AddModeField(ServerLogEntry::IT2ME);
  scoped_ptr<XmlElement> stanza = entry->ToStanza();
  std::string error;
  std::map<std::string, std::string> key_value_pairs;
  key_value_pairs["role"] = "host";
  key_value_pairs["event-name"] = "session-state";
  key_value_pairs["session-state"] = "connected";
  key_value_pairs["mode"] = "it2me";
  std::set<std::string> keys;
  ASSERT_TRUE(VerifyStanza(key_value_pairs, keys, stanza.get(), &error)) <<
      error;
}

TEST(ServerLogEntryHostTest, AddModeField2) {
  scoped_ptr<ServerLogEntry> entry(MakeLogEntryForSessionStateChange(true));
  entry->AddModeField(ServerLogEntry::ME2ME);
  scoped_ptr<XmlElement> stanza = entry->ToStanza();
  std::string error;
  std::map<std::string, std::string> key_value_pairs;
  key_value_pairs["role"] = "host";
  key_value_pairs["event-name"] = "session-state";
  key_value_pairs["session-state"] = "connected";
  key_value_pairs["mode"] = "me2me";
  std::set<std::string> keys;
  ASSERT_TRUE(VerifyStanza(key_value_pairs, keys, stanza.get(), &error)) <<
      error;
}

}  // namespace remoting
