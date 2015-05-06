// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/signaling/jid_util.h"

#include "base/strings/string_util.h"

namespace remoting {

std::string NormalizeJid(const std::string& jid) {
  size_t slash_pos = jid.find('/');

  // In case there the jid doesn't have resource id covert the whole value to
  // lower-case.
  if (slash_pos == std::string::npos)
    return base::StringToLowerASCII(jid);

  return base::StringToLowerASCII(jid.substr(0, slash_pos)) +
         jid.substr(slash_pos);
}

}  // namespace remoting
