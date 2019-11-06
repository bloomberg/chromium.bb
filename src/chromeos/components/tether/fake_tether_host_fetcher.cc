// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/tether/fake_tether_host_fetcher.h"

#include "base/memory/ptr_util.h"

namespace chromeos {

namespace tether {

FakeTetherHostFetcher::FakeTetherHostFetcher(
    const multidevice::RemoteDeviceRefList& tether_hosts)
    : tether_hosts_(tether_hosts) {}

FakeTetherHostFetcher::FakeTetherHostFetcher()
    : FakeTetherHostFetcher(multidevice::RemoteDeviceRefList()) {}

FakeTetherHostFetcher::~FakeTetherHostFetcher() = default;

void FakeTetherHostFetcher::NotifyTetherHostsUpdated() {
  TetherHostFetcher::NotifyTetherHostsUpdated();
}

bool FakeTetherHostFetcher::HasSyncedTetherHosts() {
  return !tether_hosts_.empty();
}

void FakeTetherHostFetcher::FetchAllTetherHosts(
    const TetherHostFetcher::TetherHostListCallback& callback) {
  ProcessFetchAllTetherHostsRequest(tether_hosts_, callback);
}

void FakeTetherHostFetcher::FetchTetherHost(
    const std::string& device_id,
    const TetherHostFetcher::TetherHostCallback& callback) {
  ProcessFetchSingleTetherHostRequest(device_id, tether_hosts_, callback);
}

}  // namespace tether

}  // namespace chromeos
