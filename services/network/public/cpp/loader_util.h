// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_LOADER_UTIL_H_
#define SERVICES_NETWORK_PUBLIC_CPP_LOADER_UTIL_H_

#include "base/memory/ref_counted.h"

class GURL;

namespace net {
class HttpRawRequestHeaders;
class HttpResponseHeaders;
class URLRequest;
}  // namespace net

// This header contains utility functions and constants shared between network
// service and the old loading path in content/. Once Network Service is the
// only path this should move out of the public directory.
namespace network {
struct HttpRawRequestResponseInfo;
struct ResourceResponse;

// The name of the "Accept" header.
extern const char kAcceptHeader[];

// Accept header used for frame requests.
extern const char kFrameAcceptHeader[];

// The default Accept header value to use if none were specified.
extern const char kDefaultAcceptHeader[];

// Helper utilities shared between network service and ResourceDispatcherHost
// code paths.

// Whether the response body should be sniffed in order to determine the MIME
// type of the response.
bool ShouldSniffContent(net::URLRequest* url_request,
                        ResourceResponse* response);

// Fill HttpRawRequestResponseInfo based on raw headers.
scoped_refptr<HttpRawRequestResponseInfo> BuildRawRequestResponseInfo(
    const net::URLRequest& request,
    const net::HttpRawRequestHeaders& raw_request_headers,
    const net::HttpResponseHeaders* raw_response_headers);

// Returns the referrer based on the validity of the URL and command line flags.
std::string ComputeReferrer(const GURL& referrer);

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_LOADER_UTIL_H_
