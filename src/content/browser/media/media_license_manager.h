// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_MEDIA_LICENSE_MANAGER_H_
#define CONTENT_BROWSER_MEDIA_MEDIA_LICENSE_MANAGER_H_

#include <memory>

#include "base/callback_forward.h"
#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/types/pass_key.h"
#include "content/browser/media/media_license_quota_client.h"
#include "content/common/content_export.h"
#include "media/cdm/cdm_type.h"
#include "media/mojo/mojom/cdm_storage.mojom.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

namespace content {
class MediaLicenseStorageHost;

// Each StoragePartition owns exactly one instance of this class. This class
// creates and destroys MediaLicenseStorageHost instances to meet the
// demands for CDM from different storage keys.
//
// This class is not thread-safe, and all access to an instance must happen on
// the same sequence.
class CONTENT_EXPORT MediaLicenseManager {
 public:
  // CdmStorage provides per-storage key, per-CDM type storage.
  struct CONTENT_EXPORT BindingContext {
    BindingContext(const blink::StorageKey& storage_key,
                   const media::CdmType& cdm_type)
        : storage_key(storage_key), cdm_type(cdm_type) {}

    const blink::StorageKey storage_key;
    const media::CdmType cdm_type;
  };

  MediaLicenseManager(
      const base::FilePath& bucket_base_path,
      scoped_refptr<storage::SpecialStoragePolicy> special_storage_policy,
      scoped_refptr<storage::QuotaManagerProxy> quota_manager_proxy);
  MediaLicenseManager(const MediaLicenseManager&) = delete;
  MediaLicenseManager& operator=(const MediaLicenseManager&) = delete;
  ~MediaLicenseManager();

  void OpenCdmStorage(const BindingContext& binding_context,
                      mojo::PendingReceiver<media::mojom::CdmStorage> receiver);

  // Called by the MediaLicenseQuotaClient.
  void DeleteBucketData(
      const storage::BucketLocator& bucket,
      storage::mojom::QuotaClient::DeleteBucketDataCallback callback);

  // Returns an empty path if the database is in-memory.
  base::FilePath GetDatabasePath(const storage::BucketLocator& bucket_locator);

  // Called when a receiver is disconnected from a MediaLicenseStorageHost.
  //
  // `host` must be owned by this manager. `host` may be deleted.
  void OnHostReceiverDisconnect(
      MediaLicenseStorageHost* host,
      base::PassKey<MediaLicenseStorageHost> pass_key);

 private:
  void DidGetBucket(const blink::StorageKey& storage_key,
                    storage::QuotaErrorOr<storage::BucketInfo> result);

  SEQUENCE_CHECKER(sequence_checker_);

  // Root path of the storage bucket associated with the StoragePartition which
  // owns this class. If `bucket_base_path_` is empty, the profile is in-memory.
  const base::FilePath bucket_base_path_;

  // Tracks special rights for apps and extensions, may be null.
  const scoped_refptr<storage::SpecialStoragePolicy> special_storage_policy_;

  const scoped_refptr<storage::QuotaManagerProxy> quota_manager_proxy_;

  base::flat_map<blink::StorageKey, std::unique_ptr<MediaLicenseStorageHost>>
      hosts_ GUARDED_BY_CONTEXT(sequence_checker_);

  // Maps storage keys to a list of receivers which are awaiting bucket
  // information from the quota system before they can be bound.
  base::flat_map<
      blink::StorageKey,
      std::vector<std::pair<BindingContext,
                            mojo::PendingReceiver<media::mojom::CdmStorage>>>>
      pending_receivers_;

  MediaLicenseQuotaClient quota_client_ GUARDED_BY_CONTEXT(sequence_checker_);

  // Once the QuotaClient receiver is destroyed, the underlying mojo connection
  // is closed. Callbacks associated with mojo calls received over this
  // connection may only be dropped after the connection is closed. For this
  // reason, it's preferable to have the receiver be destroyed as early as
  // possible during the MediaLicenseManager destruction process.
  mojo::Receiver<storage::mojom::QuotaClient> quota_client_receiver_
      GUARDED_BY_CONTEXT(sequence_checker_);

  base::WeakPtrFactory<MediaLicenseManager> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_MEDIA_LICENSE_MANAGER_H_
