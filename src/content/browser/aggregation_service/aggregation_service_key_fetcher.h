// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_AGGREGATION_SERVICE_AGGREGATION_SERVICE_KEY_FETCHER_H_
#define CONTENT_BROWSER_AGGREGATION_SERVICE_AGGREGATION_SERVICE_KEY_FETCHER_H_

#include <memory>
#include <vector>

#include "base/callback_forward.h"
#include "base/containers/circular_deque.h"
#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/aggregation_service/public_key.h"
#include "content/common/content_export.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace url {
class Origin;
}  // namespace url

namespace content {

class AggregationServiceStorageContext;

// This class is responsible for requesting keys from storage, owned by the
// assembler.
class CONTENT_EXPORT AggregationServiceKeyFetcher {
 public:
  // This class is responsible for fetching public keys from helper servers over
  // the network.
  class NetworkFetcher {
   public:
    virtual ~NetworkFetcher() = default;

    using NetworkFetchCallback =
        base::OnceCallback<void(absl::optional<PublicKeyset>)>;

    // Fetch public keys for `origin` from the helper servers. Returns
    // absl::nullopt in case of network or parsing error.
    virtual void FetchPublicKeys(const url::Origin& origin,
                                 NetworkFetchCallback callback) = 0;
  };

  enum class PublicKeyFetchStatus {
    // TODO(crbug.com/1217823): Propagate up more granular errors.
    kOk,
    kPublicKeyFetchFailed,
    kMaxValue = kPublicKeyFetchFailed,
  };

  using FetchCallback =
      base::OnceCallback<void(absl::optional<PublicKey>, PublicKeyFetchStatus)>;

  AggregationServiceKeyFetcher(
      AggregationServiceStorageContext* storage_context,
      std::unique_ptr<NetworkFetcher> network_fetcher);
  AggregationServiceKeyFetcher(const AggregationServiceKeyFetcher& other) =
      delete;
  AggregationServiceKeyFetcher& operator=(
      const AggregationServiceKeyFetcher& other) = delete;
  virtual ~AggregationServiceKeyFetcher();

  // Gets a currently valid public key for `origin` and triggers the `callback`
  // once completed.
  //
  // Helper server's keys must be rotated weekly which is primarily to limit the
  // impact of a compromised key. Any public key must be valid when fetched and
  // this will be enforced by the key fetcher. This ensures that the key used to
  // encrypt is valid at encryption time.
  //
  // To further limit the impact of a compromised key, we will support "key
  // slicing". That is, each helper server may make multiple public keys
  // available. At encryption time, the fetcher will (uniformly at random) pick
  // one of the public keys to use. This selection should be made independently
  // between reports so that the key choice cannot be used to partition reports
  // into separate groups of users. Virtual for mocking in tests.
  virtual void GetPublicKey(const url::Origin& origin, FetchCallback callback);

 private:
  // Called when public keys are received from the storage.
  void OnPublicKeysReceivedFromStorage(const url::Origin& origin,
                                       std::vector<PublicKey> keys);

  // Keys are fetched from the network if they are not found in storage.
  void FetchPublicKeysFromNetwork(const url::Origin& origin);

  // Called when public keys are received from the network fetcher.
  void OnPublicKeysReceivedFromNetwork(const url::Origin& origin,
                                       absl::optional<PublicKeyset> keyset);

  // Runs callbacks for pending requests for `origin` with the public keys
  // received from the network or storage. Any keys specified must be currently
  // valid.
  void RunCallbacksForOrigin(const url::Origin& origin,
                             const std::vector<PublicKey>& keys);

  // Using a raw pointer is safe because `storage_context_` is guaranteed to
  // outlive `this`.
  raw_ptr<AggregationServiceStorageContext> storage_context_;

  // Map of all origins that are currently waiting for the public keys, and
  // their associated fetch callbacks. Used to cache ongoing requests to the
  // storage or network to prevent looking up the same key multiple times at
  // once.
  base::flat_map<url::Origin, base::circular_deque<FetchCallback>>
      origin_callbacks_;

  // Responsible for issuing requests to network for fetching public keys.
  std::unique_ptr<NetworkFetcher> network_fetcher_;

  base::WeakPtrFactory<AggregationServiceKeyFetcher> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_AGGREGATION_SERVICE_AGGREGATION_SERVICE_KEY_FETCHER_H_
