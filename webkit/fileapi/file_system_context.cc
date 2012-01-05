// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/fileapi/file_system_context.h"

#include "base/file_util.h"
#include "base/message_loop_proxy.h"
#include "googleurl/src/gurl.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebFileSystem.h"
#include "webkit/fileapi/file_system_file_util.h"
#include "webkit/fileapi/file_system_options.h"
#include "webkit/fileapi/file_system_quota_client.h"
#include "webkit/fileapi/file_system_util.h"
#include "webkit/fileapi/sandbox_mount_point_provider.h"
#include "webkit/quota/quota_manager.h"
#include "webkit/quota/special_storage_policy.h"

#if defined(OS_CHROMEOS)
#include "webkit/chromeos/fileapi/cros_mount_point_provider.h"
#endif

using quota::QuotaClient;

namespace fileapi {

namespace {

QuotaClient* CreateQuotaClient(
    scoped_refptr<base::MessageLoopProxy> file_message_loop,
    FileSystemContext* context,
    bool is_incognito) {
  return new FileSystemQuotaClient(file_message_loop, context, is_incognito);
}

}  // anonymous namespace

FileSystemContext::FileSystemContext(
    scoped_refptr<base::MessageLoopProxy> file_message_loop,
    scoped_refptr<base::MessageLoopProxy> io_message_loop,
    scoped_refptr<quota::SpecialStoragePolicy> special_storage_policy,
    quota::QuotaManagerProxy* quota_manager_proxy,
    const FilePath& profile_path,
    const FileSystemOptions& options)
    : file_message_loop_(file_message_loop),
      io_message_loop_(io_message_loop),
      quota_manager_proxy_(quota_manager_proxy),
      sandbox_provider_(
          new SandboxMountPointProvider(
              file_message_loop,
              profile_path,
              options)) {
  if (quota_manager_proxy) {
    quota_manager_proxy->RegisterClient(CreateQuotaClient(
        file_message_loop, this, options.is_incognito()));
  }
#if defined(OS_CHROMEOS)
  external_provider_.reset(
      new chromeos::CrosMountPointProvider(special_storage_policy));
#endif
}

FileSystemContext::~FileSystemContext() {
}

bool FileSystemContext::DeleteDataForOriginOnFileThread(
    const GURL& origin_url) {
  DCHECK(file_message_loop_->BelongsToCurrentThread());
  DCHECK(sandbox_provider());

  // Delete temporary and persistent data.
  return
      sandbox_provider()->DeleteOriginDataOnFileThread(
          quota_manager_proxy(), origin_url, kFileSystemTypeTemporary) &&
      sandbox_provider()->DeleteOriginDataOnFileThread(
          quota_manager_proxy(), origin_url, kFileSystemTypePersistent);
}

bool FileSystemContext::DeleteDataForOriginAndTypeOnFileThread(
    const GURL& origin_url, FileSystemType type) {
  DCHECK(file_message_loop_->BelongsToCurrentThread());
  if (type == fileapi::kFileSystemTypeTemporary ||
      type == fileapi::kFileSystemTypePersistent) {
    DCHECK(sandbox_provider());
    return sandbox_provider()->DeleteOriginDataOnFileThread(
        quota_manager_proxy(), origin_url, type);
  }
  return false;
}

FileSystemQuotaUtil*
FileSystemContext::GetQuotaUtil(FileSystemType type) const {
  if (type == fileapi::kFileSystemTypeTemporary ||
      type == fileapi::kFileSystemTypePersistent) {
    DCHECK(sandbox_provider());
    return sandbox_provider()->quota_util();
  }
  return NULL;
}

FileSystemFileUtil* FileSystemContext::GetFileUtil(
    FileSystemType type) const {
  FileSystemMountPointProvider* mount_point_provider =
      GetMountPointProvider(type);
  if (!mount_point_provider)
    return NULL;
  return mount_point_provider->GetFileUtil();
}

FileSystemMountPointProvider* FileSystemContext::GetMountPointProvider(
    FileSystemType type) const {
  switch (type) {
    case kFileSystemTypeTemporary:
    case kFileSystemTypePersistent:
      return sandbox_provider_.get();
    case kFileSystemTypeExternal:
      return external_provider_.get();
    case kFileSystemTypeUnknown:
    default:
      NOTREACHED();
      return NULL;
  }
}

SandboxMountPointProvider*
FileSystemContext::sandbox_provider() const {
  return sandbox_provider_.get();
}

ExternalFileSystemMountPointProvider*
FileSystemContext::external_provider() const {
  return external_provider_.get();
}

void FileSystemContext::DeleteOnCorrectThread() const {
  if (!io_message_loop_->BelongsToCurrentThread()) {
    io_message_loop_->DeleteSoon(FROM_HERE, this);
    return;
  }
  delete this;
}

}  // namespace fileapi

COMPILE_ASSERT(int(WebKit::WebFileSystem::TypeTemporary) == \
               int(fileapi::kFileSystemTypeTemporary), mismatching_enums);
COMPILE_ASSERT(int(WebKit::WebFileSystem::TypePersistent) == \
               int(fileapi::kFileSystemTypePersistent), mismatching_enums);
COMPILE_ASSERT(int(WebKit::WebFileSystem::TypeExternal) == \
               int(fileapi::kFileSystemTypeExternal), mismatching_enums);
