// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/smb_client/smb_share_finder.h"

#include <vector>

#include "base/bind.h"
#include "chrome/browser/chromeos/smb_client/discovery/mdns_host_locator.h"
#include "chrome/browser/chromeos/smb_client/smb_constants.h"

namespace chromeos {
namespace smb_client {

SmbShareFinder::SmbShareFinder(SmbProviderClient* client) : client_(client) {}
SmbShareFinder::~SmbShareFinder() = default;

void SmbShareFinder::GatherSharesInNetwork(
    HostDiscoveryResponse discovery_callback,
    GatherSharesInNetworkResponse shares_callback) {
  const bool should_start_discovery = share_callbacks_.empty();

  if (discovery_callbacks_.empty() && !should_start_discovery) {
    // Host discovery completed but shares callback is still in progress.
    std::move(discovery_callback).Run();
    InsertShareCallback(std::move(shares_callback));
  } else {
    // Either GatherSharesInNetwork has not been called yet or it has and
    // discovery has not yet finished.
    InsertDiscoveryAndShareCallbacks(std::move(discovery_callback),
                                     std::move(shares_callback));
  }
  if (should_start_discovery) {
    scanner_.FindHostsInNetwork(
        base::BindOnce(&SmbShareFinder::OnHostsFound, AsWeakPtr()));
  }
}

void SmbShareFinder::DiscoverHostsInNetwork(
    HostDiscoveryResponse discovery_callback) {
  scanner_.FindHostsInNetwork(base::BindOnce(&SmbShareFinder::OnHostsDiscovered,
                                             AsWeakPtr(),
                                             std::move(discovery_callback)));
}

void SmbShareFinder::RegisterHostLocator(std::unique_ptr<HostLocator> locator) {
  scanner_.RegisterHostLocator(std::move(locator));
}

std::string SmbShareFinder::GetResolvedUrl(const SmbUrl& url) const {
  DCHECK(url.IsValid());

  const std::string ip_address = scanner_.ResolveHost(url.GetHost());
  if (ip_address.empty()) {
    return url.ToString();
  }

  return url.ReplaceHost(ip_address);
}

void SmbShareFinder::OnHostsDiscovered(HostDiscoveryResponse discovery_callback,
                                       bool success,
                                       const HostMap& hosts) {
  std::move(discovery_callback).Run();
}

void SmbShareFinder::OnHostsFound(bool success, const HostMap& hosts) {
  DCHECK_EQ(0u, host_counter_);

  RunDiscoveryCallbacks();

  if (!success) {
    LOG(ERROR) << "SmbShareFinder failed to find hosts";
    RunEmptySharesCallbacks();
    return;
  }

  if (hosts.empty()) {
    RunEmptySharesCallbacks();
    return;
  }

  host_counter_ = hosts.size();
  for (const auto& host : hosts) {
    const std::string& host_name = host.first;
    const std::string& resolved_address = host.second;
    const base::FilePath server_url(kSmbSchemePrefix + resolved_address);

    client_->GetShares(
        server_url,
        base::BindOnce(&SmbShareFinder::OnSharesFound, AsWeakPtr(), host_name));
  }
}

void SmbShareFinder::OnSharesFound(
    const std::string& host_name,
    smbprovider::ErrorType error,
    const smbprovider::DirectoryEntryListProto& entries) {
  DCHECK_GT(host_counter_, 0u);
  --host_counter_;

  if (error != smbprovider::ErrorType::ERROR_OK) {
    LOG(ERROR) << "Error finding shares: " << error;
    return;
  }

  for (const smbprovider::DirectoryEntryProto& entry : entries.entries()) {
    SmbUrl url(kSmbSchemePrefix + host_name + "/" + entry.name());
    if (url.IsValid()) {
      shares_.push_back(std::move(url));
    } else {
      LOG(WARNING) << "URL found is not valid";
    }
  }

  if (host_counter_ == 0) {
    RunSharesCallbacks(shares_);
  }
}

void SmbShareFinder::RunDiscoveryCallbacks() {
  for (auto& callback : discovery_callbacks_) {
    std::move(callback).Run();
  }
  discovery_callbacks_.clear();
}

void SmbShareFinder::RunSharesCallbacks(const std::vector<SmbUrl>& shares) {
  for (auto& share_callback : share_callbacks_) {
    std::move(share_callback).Run(shares);
  }
  share_callbacks_.clear();
  shares_.clear();
}

void SmbShareFinder::RunEmptySharesCallbacks() {
  RunSharesCallbacks(std::vector<SmbUrl>());
}

void SmbShareFinder::InsertDiscoveryAndShareCallbacks(
    HostDiscoveryResponse discovery_callback,
    GatherSharesInNetworkResponse share_callback) {
  discovery_callbacks_.push_back(std::move(discovery_callback));
  share_callbacks_.push_back(std::move(share_callback));
}

void SmbShareFinder::InsertShareCallback(
    GatherSharesInNetworkResponse share_callback) {
  share_callbacks_.push_back(std::move(share_callback));
}

}  // namespace smb_client
}  // namespace chromeos
