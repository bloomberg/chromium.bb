// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "remoting/host/server_log_entry.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libjingle/source/talk/xmllite/xmlelement.h"

using buzz::XmlAttr;
using buzz::XmlElement;

namespace remoting {

class ServerLogEntryTest : public testing::Test {
 protected:
  // Verifies a logging stanza.
  // |keyValuePairs| lists the keys that must have specified values, and |keys|
  // lists the keys that must be present, but may have arbitrary values.
  // There must be no other keys.
  static bool VerifyStanza(
      const std::map<std::string, std::string>& key_value_pairs,
      const std::set<std::string> keys,
      const XmlElement* elem,
      std::string* error) {
    int attrCount = 0;
    for (const XmlAttr* attr = elem->FirstAttr(); attr != NULL;
         attr = attr->NextAttr(), attrCount++) {
      if (attr->Name().Namespace().length() != 0) {
        *error = "attribute has non-empty namespace " +
            attr->Name().Namespace();
        return false;
      }
      const std::string& key = attr->Name().LocalPart();
      const std::string& value = attr->Value();
      std::map<std::string, std::string>::const_iterator iter =
          key_value_pairs.find(key);
      if (iter == key_value_pairs.end()) {
        if (keys.find(key) == keys.end()) {
          *error = "unexpected attribute " + key;
          return false;
        }
      } else {
        if (iter->second != value) {
          *error = "attribute " + key + " has value " + iter->second +
              ": expected " + value;
          return false;
        }
      }
    }
    int attr_count_expected = key_value_pairs.size() + keys.size();
    if (attrCount != attr_count_expected) {
      std::stringstream s;
      s << "stanza has " << attrCount << " keys: expected "
        << attr_count_expected;
      *error = s.str();
      return false;
    }
    return true;
  }
};

TEST_F(ServerLogEntryTest, MakeSessionStateChange) {
  scoped_ptr<ServerLogEntry> entry(
      ServerLogEntry::MakeSessionStateChange(true));
  scoped_ptr<XmlElement> stanza(entry->ToStanza());
  std::string error;
  std::map<std::string, std::string> key_value_pairs;
  key_value_pairs["role"] = "host";
  key_value_pairs["event-name"] = "session-state";
  key_value_pairs["session-state"] = "connected";
  std::set<std::string> keys;
  ASSERT_TRUE(VerifyStanza(key_value_pairs, keys, stanza.get(), &error)) <<
      error;
}

TEST_F(ServerLogEntryTest, AddHostFields) {
  scoped_ptr<ServerLogEntry> entry(
      ServerLogEntry::MakeSessionStateChange(true));
  entry->AddHostFields();
  scoped_ptr<XmlElement> stanza(entry->ToStanza());
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
  ASSERT_TRUE(VerifyStanza(key_value_pairs, keys, stanza.get(), &error)) <<
      error;
}

}  // namespace remoting
