// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_INTERNAL_API_PUBLIC_HTTP_BRIDGE_NETWORK_RESOURCES_H_
#define SYNC_INTERNAL_API_PUBLIC_HTTP_BRIDGE_NETWORK_RESOURCES_H_

#include "base/memory/scoped_ptr.h"
#include "sync/base/sync_export.h"
#include "sync/internal_api/public/network_resources.h"
#include "sync/internal_api/public/network_time_update_callback.h"

namespace net {
class URLRequestContextGetter;
}  // namespace net

namespace syncer {

class CancelationSignal;
class HttpPostProviderFactory;

class SYNC_EXPORT HttpBridgeNetworkResources : public NetworkResources {
  public:
   virtual ~HttpBridgeNetworkResources();

   // NetworkResources
   virtual scoped_ptr<HttpPostProviderFactory> GetHttpPostProviderFactory(
       net::URLRequestContextGetter* baseline_context_getter,
       const NetworkTimeUpdateCallback& network_time_update_callback,
       CancelationSignal* cancelation_signal) OVERRIDE;
};

}  // namespace syncer

#endif  // SYNC_INTERNAL_API_PUBLIC_HTTP_BRIDGE_NETWORK_RESOURCES_H_
