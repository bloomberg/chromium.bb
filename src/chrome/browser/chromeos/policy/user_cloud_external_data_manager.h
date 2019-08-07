// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_USER_CLOUD_EXTERNAL_DATA_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_USER_CLOUD_EXTERNAL_DATA_MANAGER_H_

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/chromeos/policy/cloud_external_data_manager_base.h"
#include "components/policy/core/common/policy_details.h"

namespace base {
class SequencedTaskRunner;
}

namespace policy {

class CloudPolicyStore;
class ResourceCache;

// Downloads, verifies, caches and retrieves external data referenced by
// policies.
// This is the implementation for regular users on Chrome OS. The code would
// work on desktop platforms as well but for now, is used on Chrome OS only
// because no other platform has policies referencing external data.
class UserCloudExternalDataManager : public CloudExternalDataManagerBase {
 public:
  // |get_policy_details| is used to determine the maximum size that the
  // data referenced by each policy can have. Download scheduling, verification,
  // caching and retrieval tasks are done via the |backend_task_runner|, which
  // must support file I/O.  The manager is responsible for external data
  // references by policies in |policy_store|.
  UserCloudExternalDataManager(
      const GetChromePolicyDetailsCallback& get_policy_details,
      scoped_refptr<base::SequencedTaskRunner> backend_task_runner,
      const base::FilePath& cache_path,
      CloudPolicyStore* policy_store);
  ~UserCloudExternalDataManager() override;

 private:
  // Cache used to store downloaded external data. The |resource_cache_| is
  // owned by the manager but its destruction must be handled with care:
  // * The manager owns a |backend_| which owns an |external_data_store_| which
  //   uses the |resource_cache_|. The |external_data_store_| must be destroyed
  //   before the |resource_cache_|.
  // * After construction, |backend_|, |external_data_store_| and
  //   |resource_cache_| can only be accessed through the
  //   |backend_task_runner_|.
  //
  // It follows that in order to destroy |resource_cache_|, the manager must
  // take the following steps:
  // * Post a task to the |backend_task_runner_| that will tell the |backend_|
  //   to destroy the |external_data_store_|.
  // * Post a task to the |backend_task_runner_| that will destroy the
  //   |resource_cache_|.
  // Because of this destruction sequence, a scoped_ptr cannot be used.
  ResourceCache* resource_cache_;

  DISALLOW_COPY_AND_ASSIGN(UserCloudExternalDataManager);
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_USER_CLOUD_EXTERNAL_DATA_MANAGER_H_
