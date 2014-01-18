// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_TEST_FAKE_SERVER_FAKE_SERVER_NETWORK_RESOURCES_H_
#define SYNC_TEST_FAKE_SERVER_FAKE_SERVER_NETWORK_RESOURCES_H_

#include "base/memory/scoped_ptr.h"
#include "sync/internal_api/public/network_resources.h"
#include "sync/internal_api/public/network_time_update_callback.h"

namespace net {
class URLRequestContextGetter;
}  // namespace net

namespace syncer {

class FakeServer;
class HttpPostProviderFactory;

class FakeServerNetworkResources : public NetworkResources {
  public:
   FakeServerNetworkResources(FakeServer* fake_server);
   virtual ~FakeServerNetworkResources();

   // NetworkResources
   virtual scoped_ptr<HttpPostProviderFactory> GetHttpPostProviderFactory(
       net::URLRequestContextGetter* baseline_context_getter,
       const NetworkTimeUpdateCallback& network_time_update_callback,
       CancelationSignal* cancelation_signal) OVERRIDE;

  private:
   FakeServer* const fake_server_;
};

}  // namespace syncer

#endif  // SYNC_TEST_FAKE_SERVER_FAKE_SERVER_NETWORK_RESOURCES_H_
