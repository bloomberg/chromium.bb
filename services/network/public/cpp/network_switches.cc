// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/network_switches.h"

namespace network {

namespace switches {

// These mappings only apply to the host resolver.
const char kHostResolverRules[] = "host-resolver-rules";

// Enables saving net log events to a file. If a value is given, it used as the
// path the the file, otherwise the file is named netlog.json and placed in the
// user data directory.
const char kLogNetLog[] = "log-net-log";

// Don't send HTTP-Referer headers.
const char kNoReferrers[] = "no-referrers";

}  // namespace switches

}  // namespace network
