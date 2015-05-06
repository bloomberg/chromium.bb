// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_SIGNALING_JID_UTIL_H_
#define REMOTING_SIGNALING_JID_UTIL_H_

#include <string>

namespace remoting {

// Normalizes the |jid| by converting case-insensitive parts (node and domain)
// to lower-case.
std::string NormalizeJid(const std::string& jid);

}  // namespace remoting

#endif  // REMOTING_SIGNALING_JID_UTIL_H_
