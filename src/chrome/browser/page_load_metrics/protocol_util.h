// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_LOAD_METRICS_PROTOCOL_UTIL_H_
#define CHROME_BROWSER_PAGE_LOAD_METRICS_PROTOCOL_UTIL_H_

#include "net/http/http_response_info.h"

namespace page_load_metrics {

enum class NetworkProtocol { kHttp11, kHttp2, kQuic, kOther };

// Returns a higher-level enum summary of the protocol described by the
// ConnectionInfo enum.
NetworkProtocol GetNetworkProtocol(
    net::HttpResponseInfo::ConnectionInfo connection_info);

}  // namespace page_load_metrics

#endif  // CHROME_BROWSER_PAGE_LOAD_METRICS_PROTOCOL_UTIL_H_
