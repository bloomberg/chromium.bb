// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/test/fake_server/fake_server_network_resources.h"

#include "base/threading/thread_task_runner_handle.h"
#include "components/sync/base/cancelation_signal.h"
#include "components/sync/engine/net/http_post_provider_factory.h"
#include "components/sync/test/fake_server/fake_server.h"
#include "components/sync/test/fake_server/fake_server_http_post_provider.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

using syncer::CancelationSignal;
using syncer::HttpPostProviderFactory;
using syncer::NetworkTimeUpdateCallback;

namespace fake_server {

FakeServerNetworkResources::FakeServerNetworkResources(
    const base::WeakPtr<FakeServer>& fake_server)
    : fake_server_(fake_server),
      fake_server_task_runner_(base::ThreadTaskRunnerHandle::Get()) {}

FakeServerNetworkResources::~FakeServerNetworkResources() {}

std::unique_ptr<syncer::HttpPostProviderFactory>
FakeServerNetworkResources::GetHttpPostProviderFactory(
    std::unique_ptr<network::SharedURLLoaderFactoryInfo>
        url_loader_factory_info,
    const NetworkTimeUpdateCallback& network_time_update_callback,
    CancelationSignal* cancelation_signal) {
  return std::make_unique<FakeServerHttpPostProviderFactory>(
      fake_server_, fake_server_task_runner_);
}

}  // namespace fake_server
