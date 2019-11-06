// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_SERVICES_MOJO_PROVISION_FETCHER_H_
#define MEDIA_MOJO_SERVICES_MOJO_PROVISION_FETCHER_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "media/base/provision_fetcher.h"
#include "media/mojo/mojom/provision_fetcher.mojom.h"
#include "media/mojo/services/media_mojo_export.h"

namespace media {

// A ProvisionFetcher that proxies to a mojom::ProvisionFetcherPtr.
class MEDIA_MOJO_EXPORT MojoProvisionFetcher : public ProvisionFetcher {
 public:
  explicit MojoProvisionFetcher(
      mojom::ProvisionFetcherPtr provision_fetcher_ptr);
  ~MojoProvisionFetcher() final;

  // ProvisionFetcher implementation:
  void Retrieve(const std::string& default_url,
                const std::string& request_data,
                const ResponseCB& response_cb) final;

 private:
  // Callback for mojom::ProvisionFetcherPtr::Retrieve().
  void OnResponse(const ResponseCB& response_cb,
                  bool success,
                  const std::string& response);

  mojom::ProvisionFetcherPtr provision_fetcher_ptr_;

  base::WeakPtrFactory<MojoProvisionFetcher> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(MojoProvisionFetcher);
};

}  // namespace media

#endif  // MEDIA_MOJO_SERVICES_MOJO_PROVISION_FETCHER_H_
