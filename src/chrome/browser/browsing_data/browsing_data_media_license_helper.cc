// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/browsing_data_media_license_helper.h"

#include <memory>
#include <set>

#include "base/bind.h"
#include "base/location.h"
#include "base/task/sequenced_task_runner.h"
#include "components/browsing_data/content/browsing_data_helper.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_quota_util.h"
#include "storage/browser/file_system/plugin_private_file_system_backend.h"
#include "storage/common/file_system/file_system_types.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "url/origin.h"

using content::BrowserThread;

namespace {

// An implementation of the BrowsingDataMediaLicenseHelper interface that
// determine data on media licenses in a given |filesystem_context| and
// returns a list of MediaLicenseInfo items to a client.
class BrowsingDataMediaLicenseHelperImpl final
    : public BrowsingDataMediaLicenseHelper {
 public:
  // BrowsingDataMediaLicenseHelper implementation
  explicit BrowsingDataMediaLicenseHelperImpl(
      storage::FileSystemContext* filesystem_context);

  BrowsingDataMediaLicenseHelperImpl(
      const BrowsingDataMediaLicenseHelperImpl&) = delete;
  BrowsingDataMediaLicenseHelperImpl& operator=(
      const BrowsingDataMediaLicenseHelperImpl&) = delete;

  void StartFetching(FetchCallback callback) final;
  void DeleteMediaLicenseOrigin(const GURL& origin) final;

 private:
  ~BrowsingDataMediaLicenseHelperImpl() final;

  // Enumerates all filesystem files, storing the resulting list into
  // file_system_file_ for later use. This must be called on the file
  // task runner.
  void FetchMediaLicenseInfoOnFileTaskRunner(FetchCallback callback);

  // Deletes all file systems associated with |origin|. This must be called on
  // the file task runner.
  void DeleteMediaLicenseOriginOnFileTaskRunner(const GURL& origin);

  // Returns the file task runner for the |filesystem_context_|.
  base::SequencedTaskRunner* file_task_runner() {
    return filesystem_context_->default_file_task_runner();
  }

  // Keep a reference to the FileSystemContext object for the current profile
  // for use on the file task runner.
  scoped_refptr<storage::FileSystemContext> filesystem_context_;
};

BrowsingDataMediaLicenseHelperImpl::BrowsingDataMediaLicenseHelperImpl(
    storage::FileSystemContext* filesystem_context)
    : filesystem_context_(filesystem_context) {
  DCHECK(filesystem_context_.get());
}

BrowsingDataMediaLicenseHelperImpl::~BrowsingDataMediaLicenseHelperImpl() {}

void BrowsingDataMediaLicenseHelperImpl::StartFetching(FetchCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!callback.is_null());
  file_task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&BrowsingDataMediaLicenseHelperImpl::
                                    FetchMediaLicenseInfoOnFileTaskRunner,
                                this, std::move(callback)));
}

void BrowsingDataMediaLicenseHelperImpl::DeleteMediaLicenseOrigin(
    const GURL& origin) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  file_task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&BrowsingDataMediaLicenseHelperImpl::
                                    DeleteMediaLicenseOriginOnFileTaskRunner,
                                this, origin));
}

void BrowsingDataMediaLicenseHelperImpl::FetchMediaLicenseInfoOnFileTaskRunner(
    FetchCallback callback) {
  DCHECK(file_task_runner()->RunsTasksInCurrentSequence());
  DCHECK(!callback.is_null());

  const storage::FileSystemType kType = storage::kFileSystemTypePluginPrivate;

  storage::PluginPrivateFileSystemBackend* backend =
      static_cast<storage::PluginPrivateFileSystemBackend*>(
          filesystem_context_->GetFileSystemBackend(kType));

  // Determine the set of StorageKeys used.
  std::vector<blink::StorageKey> storage_keys =
      backend->GetStorageKeysForTypeOnFileTaskRunner(kType);
  std::list<MediaLicenseInfo> result;
  for (const auto& storage_key : storage_keys) {
    if (!browsing_data::HasWebScheme(storage_key.origin().GetURL()))
      continue;  // Non-websafe state is not considered browsing data.

    int64_t size;
    base::Time last_modified_time;
    backend->GetOriginDetailsOnFileTaskRunner(filesystem_context_.get(),
                                              storage_key.origin(), &size,
                                              &last_modified_time);
    result.emplace_back(storage_key.origin().GetURL(), size,
                                      last_modified_time);
  }

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), result));
}

void BrowsingDataMediaLicenseHelperImpl::
    DeleteMediaLicenseOriginOnFileTaskRunner(const GURL& origin) {
  DCHECK(file_task_runner()->RunsTasksInCurrentSequence());

  const storage::FileSystemType kType = storage::kFileSystemTypePluginPrivate;
  storage::FileSystemBackend* backend =
      filesystem_context_->GetFileSystemBackend(kType);
  storage::FileSystemQuotaUtil* quota_util = backend->GetQuotaUtil();
  // TODO(https://crbug.com/1231162): determine whether EME/CDM/plugin private
  // file system will be partitioned and use the appropriate StorageKey.
  quota_util->DeleteStorageKeyDataOnFileTaskRunner(
      filesystem_context_.get(), filesystem_context_->quota_manager_proxy(),
      blink::StorageKey(url::Origin::Create(origin)), kType);
}

}  // namespace

BrowsingDataMediaLicenseHelper::MediaLicenseInfo::MediaLicenseInfo(
    const GURL& origin,
    int64_t size,
    base::Time last_modified_time)
    : origin(origin), size(size), last_modified_time(last_modified_time) {}

BrowsingDataMediaLicenseHelper::MediaLicenseInfo::MediaLicenseInfo(
    const MediaLicenseInfo& other) = default;

BrowsingDataMediaLicenseHelper::MediaLicenseInfo::~MediaLicenseInfo() {}

// static
BrowsingDataMediaLicenseHelper* BrowsingDataMediaLicenseHelper::Create(
    storage::FileSystemContext* filesystem_context) {
  return new BrowsingDataMediaLicenseHelperImpl(filesystem_context);
}
