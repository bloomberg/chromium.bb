// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/test/fake_server/fake_server_network_resources.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_task_runner_handle.h"
#include "sync/internal_api/public/base/cancelation_signal.h"
#include "sync/internal_api/public/http_post_provider_factory.h"
#include "sync/internal_api/public/network_time_update_callback.h"
#include "sync/test/fake_server/fake_server.h"
#include "sync/test/fake_server/fake_server_http_post_provider.h"

using syncer::CancelationSignal;
using syncer::HttpPostProviderFactory;
using syncer::NetworkTimeUpdateCallback;

namespace fake_server {

FakeServerNetworkResources::FakeServerNetworkResources(
    const base::WeakPtr<FakeServer>& fake_server)
        : fake_server_(fake_server) { }

FakeServerNetworkResources::~FakeServerNetworkResources() {}

std::unique_ptr<syncer::HttpPostProviderFactory>
FakeServerNetworkResources::GetHttpPostProviderFactory(
    const scoped_refptr<net::URLRequestContextGetter>& baseline_context_getter,
    const NetworkTimeUpdateCallback& network_time_update_callback,
    CancelationSignal* cancelation_signal) {
  return base::WrapUnique<syncer::HttpPostProviderFactory>(
      new FakeServerHttpPostProviderFactory(
          fake_server_, base::ThreadTaskRunnerHandle::Get()));
}

}  // namespace fake_server
