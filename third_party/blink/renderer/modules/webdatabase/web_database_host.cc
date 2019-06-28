// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webdatabase/web_database_host.h"

#include <utility>

#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "third_party/blink/public/mojom/webdatabase/web_database.mojom-blink.h"
#include "third_party/blink/public/platform/interface_provider.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/sqlite/sqlite3.h"

namespace blink {

WebDatabaseHost* WebDatabaseHost::instance_ = nullptr;

// static
WebDatabaseHost& WebDatabaseHost::GetInstance() {
  DEFINE_STATIC_LOCAL(WebDatabaseHost, instance, ());
  return instance;
}

void WebDatabaseHost::Init() {
  Platform::Current()->GetInterfaceProvider()->GetInterface(
      pending_remote_.InitWithNewPipeAndPassReceiver());
}

mojom::blink::WebDatabaseHost& WebDatabaseHost::GetWebDatabaseHost() {
  if (!shared_remote_) {
    DCHECK(pending_remote_);
    shared_remote_ = mojo::SharedRemote<mojom::blink::WebDatabaseHost>(
        std::move(pending_remote_), base::CreateSequencedTaskRunnerWithTraits(
                                        {base::WithBaseSyncPrimitives()}));
  }

  return *shared_remote_;
}

base::File WebDatabaseHost::OpenFile(const String& vfs_file_name,
                                     int desired_flags) {
  base::File file;
  GetWebDatabaseHost().OpenFile(vfs_file_name, desired_flags, &file);
  return file;
}

int WebDatabaseHost::DeleteFile(const String& vfs_file_name, bool sync_dir) {
  int rv = SQLITE_IOERR_DELETE;
  GetWebDatabaseHost().DeleteFile(vfs_file_name, sync_dir, &rv);
  return rv;
}

int32_t WebDatabaseHost::GetFileAttributes(const String& vfs_file_name) {
  int32_t rv = -1;
  GetWebDatabaseHost().GetFileAttributes(vfs_file_name, &rv);
  return rv;
}

int64_t WebDatabaseHost::GetFileSize(const String& vfs_file_name) {
  int64_t rv = 0LL;
  GetWebDatabaseHost().GetFileSize(vfs_file_name, &rv);
  return rv;
}

bool WebDatabaseHost::SetFileSize(const String& vfs_file_name, int64_t size) {
  bool rv = false;
  GetWebDatabaseHost().SetFileSize(vfs_file_name, size, &rv);
  return rv;
}

int64_t WebDatabaseHost::GetSpaceAvailableForOrigin(
    const SecurityOrigin& origin) {
  int64_t rv = 0LL;
  GetWebDatabaseHost().GetSpaceAvailable(&origin, &rv);
  return rv;
}

}  // namespace blink
