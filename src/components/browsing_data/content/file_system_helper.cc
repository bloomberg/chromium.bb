// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browsing_data/content/file_system_helper.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "components/browsing_data/content/browsing_data_helper.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/native_io_context.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_quota_util.h"
#include "storage/common/file_system/file_system_types.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "url/origin.h"

using content::BrowserThread;

namespace storage {
class FileSystemContext;
}  // namespace storage

namespace browsing_data {

FileSystemHelper::FileSystemHelper(
    storage::FileSystemContext* filesystem_context,
    const std::vector<storage::FileSystemType>& additional_types,
    content::NativeIOContext* native_io_context)
    : filesystem_context_(filesystem_context),
      native_io_context_(native_io_context) {
  for (storage::FileSystemType type : additional_types)
    types_.push_back(type);
  DCHECK(filesystem_context_.get());
}

FileSystemHelper::~FileSystemHelper() {}

base::SequencedTaskRunner* FileSystemHelper::file_task_runner() {
  return filesystem_context_->default_file_task_runner();
}

void FileSystemHelper::StartFetching(FetchCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!callback.is_null());

  file_task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&FileSystemHelper::FetchFileSystemInfoInFileThread, this,
                     base::BindOnce(&FileSystemHelper::DidFetchFileSystemInfo,
                                    this, std::move(callback))));
}

void FileSystemHelper::DeleteFileSystemOrigin(const url::Origin& origin) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  file_task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &FileSystemHelper::DeleteFileSystemForStorageKeyInFileThread, this,
          blink::StorageKey(origin)));
  native_io_context_->DeleteStorageKeyData(blink::StorageKey(origin),
                                           base::DoNothing());
}

void FileSystemHelper::FetchFileSystemInfoInFileThread(FetchCallback callback) {
  DCHECK(file_task_runner()->RunsTasksInCurrentSequence());
  DCHECK(!callback.is_null());

  std::list<FileSystemInfo> result;
  std::map<GURL, FileSystemInfo> file_system_info_map;
  for (storage::FileSystemType type : types_) {
    storage::FileSystemQuotaUtil* quota_util =
        filesystem_context_->GetQuotaUtil(type);
    DCHECK(quota_util);
    std::vector<blink::StorageKey> storage_keys =
        quota_util->GetStorageKeysForTypeOnFileTaskRunner(type);
    for (const auto& current : storage_keys) {
      if (!HasWebScheme(current.origin().GetURL()))
        continue;  // Non-websafe state is not considered browsing data.
      int64_t usage = quota_util->GetStorageKeyUsageOnFileTaskRunner(
          filesystem_context_.get(), current, type);
      auto inserted =
          file_system_info_map
              .insert(std::make_pair(current.origin().GetURL(),
                                     FileSystemInfo(current.origin())))
              .first;
      inserted->second.usage_map[type] = usage;
    }
  }

  for (const auto& iter : file_system_info_map)
    result.push_back(iter.second);

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), result));
}

void FileSystemHelper::DeleteFileSystemForStorageKeyInFileThread(
    const blink::StorageKey& storage_key) {
  DCHECK(file_task_runner()->RunsTasksInCurrentSequence());
  filesystem_context_->DeleteDataForStorageKeyOnFileTaskRunner(storage_key);
}

void FileSystemHelper::DidFetchFileSystemInfo(
    FetchCallback callback,
    const std::list<FileSystemInfo>& file_system_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  native_io_context_->GetStorageKeyUsageMap(
      base::BindOnce(&FileSystemHelper::AppendNativeIOInfoToFileSystemInfo,
                     this, std::move(callback), std::move(file_system_info)));
}

void FileSystemHelper::AppendNativeIOInfoToFileSystemInfo(
    FetchCallback callback,
    const std::list<FileSystemInfo>& file_system_info_list,
    const std::map<blink::StorageKey, int64_t>& native_io_usage_map) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  std::list<FileSystemInfo> result = file_system_info_list;
  std::map<GURL, FileSystemInfo> file_system_info_map;
  for (const auto& current : native_io_usage_map) {
    url::Origin origin = current.first.origin();
    if (!HasWebScheme(origin.GetURL()))
      continue;  // Non-websafe state is not considered browsing data.
    int64_t usage = current.second;
    auto inserted =
        file_system_info_map
            .insert(std::make_pair(origin.GetURL(), FileSystemInfo(origin)))
            .first;
    inserted->second
        .usage_map[storage::FileSystemType::kFileSystemTypeTemporary] = usage;
  }
  for (const auto& iter : file_system_info_map)
    result.push_back(iter.second);
  std::move(callback).Run(result);
}

FileSystemHelper::FileSystemInfo::FileSystemInfo(const url::Origin& origin)
    : origin(origin) {}

FileSystemHelper::FileSystemInfo::FileSystemInfo(const FileSystemInfo& other) =
    default;

FileSystemHelper::FileSystemInfo::~FileSystemInfo() {}

CannedFileSystemHelper::CannedFileSystemHelper(
    storage::FileSystemContext* filesystem_context,
    const std::vector<storage::FileSystemType>& additional_types,
    content::NativeIOContext* native_io_context)
    : FileSystemHelper(filesystem_context,
                       additional_types,
                       native_io_context) {}

CannedFileSystemHelper::~CannedFileSystemHelper() {}

void CannedFileSystemHelper::Add(const url::Origin& origin) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!HasWebScheme(origin.GetURL()))
    return;  // Non-websafe state is not considered browsing data.
  pending_origins_.insert(origin);
}

void CannedFileSystemHelper::Reset() {
  pending_origins_.clear();
}

bool CannedFileSystemHelper::empty() const {
  return pending_origins_.empty();
}

size_t CannedFileSystemHelper::GetCount() const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return pending_origins_.size();
}

void CannedFileSystemHelper::StartFetching(FetchCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!callback.is_null());

  std::list<FileSystemInfo> result;
  for (const auto& origin : pending_origins_)
    result.emplace_back(origin);

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), result));
}

void CannedFileSystemHelper::DeleteFileSystemOrigin(const url::Origin& origin) {
  pending_origins_.erase(origin);
  FileSystemHelper::DeleteFileSystemOrigin(origin);
}

}  // namespace browsing_data
