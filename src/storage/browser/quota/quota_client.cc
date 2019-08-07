// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/quota/quota_client.h"

namespace storage {

// Default implementation of PerformStorageCleanup for clients that have
// nothing to clean up.
void QuotaClient::PerformStorageCleanup(blink::mojom::StorageType type,
                                        base::OnceClosure callback) {
  std::move(callback).Run();
}
}  // namespace storage
