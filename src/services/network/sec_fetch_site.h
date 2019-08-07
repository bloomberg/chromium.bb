// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_SEC_FETCH_SITE_H_
#define SERVICES_NETWORK_SEC_FETCH_SITE_H_

#include "base/component_export.h"
#include "url/gurl.h"

namespace net {
class URLRequest;
}  // namespace net

namespace network {

namespace mojom {
class URLLoaderFactoryParams;
}  // namespace mojom

// Sets the right Sec-Fetch-Site request header on |request|, comparing the
// origins of |request.url_chain()| and |pending_redirect_url| against
// |request.initiator()|.
//
// Note that |pending_redirect_url| is optional - it should be set only when
// calling this method from net::URLRequest::Delegate::OnReceivedRedirect (in
// this case |request.url_chain()| won't yet contain the URL being redirected
// to).
//
// Spec: https://mikewest.github.io/sec-metadata/#sec-fetch-site-header
COMPONENT_EXPORT(NETWORK_SERVICE)
void SetSecFetchSiteHeader(net::URLRequest* request,
                           const GURL* pending_redirect_url,
                           const mojom::URLLoaderFactoryParams& factory_params);

}  // namespace network

#endif  // SERVICES_NETWORK_SEC_FETCH_SITE_H_
