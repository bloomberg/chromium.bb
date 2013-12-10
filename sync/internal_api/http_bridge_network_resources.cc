// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/internal_api/public/http_bridge_network_resources.h"

#include "base/memory/scoped_ptr.h"
#include "net/url_request/url_request_context_getter.h"
#include "sync/internal_api/public/base/cancelation_signal.h"
#include "sync/internal_api/public/http_bridge.h"
#include "sync/internal_api/public/http_post_provider_factory.h"
#include "sync/internal_api/public/network_time_update_callback.h"

namespace syncer {

HttpBridgeNetworkResources::~HttpBridgeNetworkResources() {}

scoped_ptr<HttpPostProviderFactory>
    HttpBridgeNetworkResources::GetHttpPostProviderFactory(
        net::URLRequestContextGetter* baseline_context_getter,
        const NetworkTimeUpdateCallback& network_time_update_callback,
        CancelationSignal* cancelation_signal) {
  return make_scoped_ptr<HttpPostProviderFactory>(
      new HttpBridgeFactory(baseline_context_getter,
                            network_time_update_callback,
                            cancelation_signal));
}

}  // namespace syncer
