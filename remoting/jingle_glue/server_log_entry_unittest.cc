// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/jingle_glue/server_log_entry_unittest.h"

#include <sstream>

#include "third_party/libjingle/source/talk/xmllite/xmlelement.h"

using buzz::XmlAttr;
using buzz::XmlElement;

namespace remoting {

bool VerifyStanza(
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

}  // namespace remoting
