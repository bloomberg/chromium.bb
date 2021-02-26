// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_LOCAL_SEARCH_SERVICE_INDEX_SYNC_PROXY_H_
#define CHROMEOS_COMPONENTS_LOCAL_SEARCH_SERVICE_INDEX_SYNC_PROXY_H_

#include <vector>

#include "base/strings/string16.h"
#include "chromeos/components/local_search_service/public/mojom/local_search_service_proxy.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace chromeos {
namespace local_search_service {

class IndexSync;

class IndexSyncProxy : public mojom::IndexSyncProxy {
 public:
  explicit IndexSyncProxy(IndexSync* index);
  ~IndexSyncProxy() override;

  void BindReceiver(mojo::PendingReceiver<mojom::IndexSyncProxy> receiver);

  // mojom::IndexSyncProxy:
  void GetSize(GetSizeCallback callback) override;
  void AddOrUpdate(const std::vector<Data>& data,
                   AddOrUpdateCallback callback) override;
  void Delete(const std::vector<std::string>& ids,
              DeleteCallback callback) override;
  void Find(const base::string16& query,
            uint32_t max_results,
            FindCallback callback) override;
  void ClearIndex(ClearIndexCallback callback) override;

 private:
  IndexSync* const index_;
  mojo::ReceiverSet<mojom::IndexSyncProxy> receivers_;
};

}  // namespace local_search_service
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_LOCAL_SEARCH_SERVICE_INDEX_SYNC_PROXY_H_
