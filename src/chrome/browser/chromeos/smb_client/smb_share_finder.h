// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_SMB_CLIENT_SMB_SHARE_FINDER_H_
#define CHROME_BROWSER_CHROMEOS_SMB_CLIENT_SMB_SHARE_FINDER_H_

#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/smb_client/discovery/host_locator.h"
#include "chrome/browser/chromeos/smb_client/discovery/network_scanner.h"
#include "chrome/browser/chromeos/smb_client/smb_url.h"
#include "chromeos/dbus/smb_provider_client.h"

namespace chromeos {
namespace smb_client {

// The callback that will be passed to GatherSharesInNetwork. The shares found
// will have a format of "smb://host/share". This will be called once per host.
using GatherSharesResponse =
    base::RepeatingCallback<void(const std::vector<SmbUrl>& shares_gathered)>;

// The callback that will be passed to GatherSharesInNetwork. Used to implicitly
// convert GatherSharesResponse to a OnceCallback.
using GatherSharesInNetworkResponse =
    base::OnceCallback<void(const std::vector<SmbUrl>& shares_gathered)>;

// The callback run to indicate the scan for hosts on the network is complete.
using HostDiscoveryResponse = base::OnceClosure;

// This class is responsible for finding hosts in a network and getting the
// available shares for each host found.
class SmbShareFinder : public base::SupportsWeakPtr<SmbShareFinder> {
 public:
  explicit SmbShareFinder(SmbProviderClient* client);
  ~SmbShareFinder();

  // Gathers the hosts in the network using |scanner_| and gets the shares for
  // each of the hosts found. |discovery_callback| runs once when host
  // disovery is complete. |shares_callback| only runs once when all entries
  // from hosts are stored to |shares| and will contain the paths to the shares
  // found (e.g. "smb://host/share").
  void GatherSharesInNetwork(HostDiscoveryResponse discovery_callback,
                             GatherSharesInNetworkResponse shares_callback);

  // Gathers the hosts in the network using |scanner_|. Runs
  // |discovery_callback| upon completion. No data is returned to the caller,
  // but hosts are cached in |scanner_| and can be used for name resolution.
  void DiscoverHostsInNetwork(HostDiscoveryResponse discovery_callback);

  // Registers HostLocator |locator| to |scanner_|.
  void RegisterHostLocator(std::unique_ptr<HostLocator> locator);

  // Attempts to resolve |url|. Returns the resolved url if successful,
  // otherwise returns ToString of |url|.
  std::string GetResolvedUrl(const SmbUrl& url) const;

 private:
  // Handles the response from discovering hosts in the network.
  void OnHostsDiscovered(HostDiscoveryResponse discovery_callback,
                         bool success,
                         const HostMap& hosts);

  // Handles the response from finding hosts in the network.
  void OnHostsFound(bool success, const HostMap& hosts);

  // Handles the response from getting shares for a given host.
  void OnSharesFound(const std::string& host_name,
                     smbprovider::ErrorType error,
                     const smbprovider::DirectoryEntryListProto& entries);

  // Executes all the DiscoveryCallbacks inside |discovery_callbacks_|.
  void RunDiscoveryCallbacks();

  // Executes all the SharesCallback inside |shares_callback_|.
  void RunSharesCallbacks(const std::vector<SmbUrl>& shares);

  // Executes all the SharesCallback inside |shares_callback_| with an empty
  // vector of SmbUrl.
  void RunEmptySharesCallbacks();

  // Inserts HostDiscoveryResponse in |discovery_callbacks_| and inserts
  // GatherSharesInNetworkResponse in |shares_callbacks_|.
  void InsertDiscoveryAndShareCallbacks(
      HostDiscoveryResponse discovery_callback,
      GatherSharesInNetworkResponse shares_callback);

  // Inserts |shares_callback| to |share_callbacks_|.
  void InsertShareCallback(GatherSharesInNetworkResponse shares_callback);

  NetworkScanner scanner_;

  SmbProviderClient* client_;  // Not owned.

  uint32_t host_counter_ = 0u;

  std::vector<HostDiscoveryResponse> discovery_callbacks_;
  std::vector<GatherSharesInNetworkResponse> share_callbacks_;

  std::vector<SmbUrl> shares_;

  DISALLOW_COPY_AND_ASSIGN(SmbShareFinder);
};

}  // namespace smb_client
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_SMB_CLIENT_SMB_SHARE_FINDER_H_
