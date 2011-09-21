// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/fileapi/file_system_quota_util.h"

#include "base/compiler_specific.h"
#include "base/location.h"
#include "base/message_loop_proxy.h"

namespace fileapi {

void FileSystemQuotaUtil::Proxy::UpdateOriginUsage(
    quota::QuotaManagerProxy* proxy, const GURL& origin_url,
    fileapi::FileSystemType type, int64 delta) {
  if (!file_thread_->BelongsToCurrentThread()) {
    file_thread_->PostTask(FROM_HERE, NewRunnableMethod(
        this, &Proxy::UpdateOriginUsage, proxy, origin_url, type, delta));
    return;
  }
  if (quota_util_)
    quota_util_->UpdateOriginUsageOnFileThread(proxy, origin_url, type, delta);
}

void FileSystemQuotaUtil::Proxy::StartUpdateOrigin(
    const GURL& origin_url, fileapi::FileSystemType type) {
  if (!file_thread_->BelongsToCurrentThread()) {
    file_thread_->PostTask(FROM_HERE, NewRunnableMethod(
        this, &Proxy::StartUpdateOrigin, origin_url, type));
    return;
  }
  if (quota_util_)
    quota_util_->StartUpdateOriginOnFileThread(origin_url, type);
}

void FileSystemQuotaUtil::Proxy::EndUpdateOrigin(
    const GURL& origin_url, fileapi::FileSystemType type) {
  if (!file_thread_->BelongsToCurrentThread()) {
    file_thread_->PostTask(FROM_HERE, NewRunnableMethod(
        this, &Proxy::EndUpdateOrigin, origin_url, type));
    return;
  }
  if (quota_util_)
    quota_util_->EndUpdateOriginOnFileThread(origin_url, type);
}

FileSystemQuotaUtil::Proxy::Proxy(
    FileSystemQuotaUtil* quota_util,
    base::MessageLoopProxy* file_thread)
    : quota_util_(quota_util),
      file_thread_(file_thread) {
  DCHECK(quota_util);
}

FileSystemQuotaUtil::Proxy::~Proxy() {
}

FileSystemQuotaUtil::FileSystemQuotaUtil(base::MessageLoopProxy* file_thread)
    : proxy_(new Proxy(ALLOW_THIS_IN_INITIALIZER_LIST(this), file_thread)) {
}

FileSystemQuotaUtil::~FileSystemQuotaUtil() {
  proxy_->quota_util_ = NULL;
}

}  // namespace fileapi
