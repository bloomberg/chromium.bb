// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/syncable/syncable_id.h"

#include <iosfwd>

#include "base/values.h"

using std::ostream;
using std::string;

namespace syncer {
namespace syncable {

ostream& operator<<(ostream& out, const Id& id) {
  out << id.s_;
  return out;
}

base::StringValue* Id::ToValue() const {
  return new base::StringValue(s_);
}

string Id::GetServerId() const {
  // Currently root is the string "0". We need to decide on a true value.
  // "" would be convenient here, as the IsRoot call would not be needed.
  if (IsRoot())
    return "0";
  return s_.substr(1);
}

Id Id::CreateFromServerId(const string& server_id) {
  Id id;
  if (server_id == "0")
    id.s_ = "r";
  else
    id.s_ = string("s") + server_id;
  return id;
}

Id Id::CreateFromClientString(const string& local_id) {
  Id id;
  if (local_id == "0")
    id.s_ = "r";
  else
    id.s_ = string("c") + local_id;
  return id;
}

Id Id::GetLexicographicSuccessor() const {
  // The successor of a string is given by appending the least
  // character in the alphabet.
  Id id = *this;
  id.s_.push_back(0);
  return id;
}

// static
Id Id::GetLeastIdForLexicographicComparison() {
  Id id;
  id.s_.clear();
  return id;
}

Id GetNullId() {
  return Id();  // Currently == root.
}

}  // namespace syncable
}  // namespace syncer
