// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_CLOUD_EXTERNAL_DATA_STORE_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_CLOUD_EXTERNAL_DATA_STORE_H_

#include <stddef.h>

#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "components/policy/core/common/cloud/cloud_external_data_manager.h"

namespace base {
class SequencedTaskRunner;
}

namespace policy {

class ResourceCache;

// Stores external data referenced by policies. Data is keyed by (policy, hash),
// the name of the policy referencing it and its SHA1 hash. Outdated entries are
// removed by calling Prune() with the list of (policy, hash) entries that are
// to be kept. Instances of this class may be created on any thread and may
// share the same cache, however:
// * After creation, the cache and all stores using it must always be accessed
//   via the same |task_runner| only.
// * Stores sharing a cache must use different cache_keys to avoid namespace
//   overlaps.
// * The cache must outlive all stores using it.
class CloudExternalDataStore {
 public:
  CloudExternalDataStore(const std::string& cache_key,
                         scoped_refptr<base::SequencedTaskRunner> task_runner,
                         ResourceCache* cache);
  ~CloudExternalDataStore();

  // Removes all entries from the store whose (policy, hash) pair is not found
  // in |metadata|.
  void Prune(const CloudExternalDataManager::Metadata& metadata);

  // Stores |data| under (policy, hash). Returns true if the store succeeded.
  bool Store(const std::string& policy,
             const std::string& hash,
             const std::string& data);

  // Loads the entry at (policy, hash) into |data|, verifies that it does not
  // exceed |max_size| and matches the expected |hash|, then returns true.
  // Returns false if no entry is found at (policy, hash), there is a problem
  // during the load, the entry exceeds |max_size| or does not match |hash|.
  bool Load(const std::string& policy,
            const std::string& hash,
            size_t max_size,
            std::string* data);

 private:
  std::string cache_key_;

  // Task runner that |this| runs on.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  ResourceCache* cache_;  // Not owned.

  DISALLOW_COPY_AND_ASSIGN(CloudExternalDataStore);
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_CLOUD_EXTERNAL_DATA_STORE_H_
