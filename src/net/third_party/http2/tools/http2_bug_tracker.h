// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_THIRD_PARTY_HTTP2_TOOLS_HTTP2_BUG_TRACKER_H_
#define NET_THIRD_PARTY_HTTP2_TOOLS_HTTP2_BUG_TRACKER_H_

#include "base/logging.h"

#define HTTP2_BUG LOG(DFATAL)
#define HTTP2_BUG_IF LOG_IF(DFATAL, (condition))
#define FLAGS_http2_always_log_bugs_for_tests (true)

#endif  // NET_THIRD_PARTY_HTTP2_TOOLS_HTTP2_BUG_TRACKER_H_
