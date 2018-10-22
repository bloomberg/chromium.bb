// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/test_content_browser_client.h"

#include "base/files/file_path.h"
#include "base/logging.h"
#include "storage/browser/quota/quota_settings.h"

#if defined(OS_ANDROID)
#include "content/shell/android/shell_descriptors.h"
#endif

namespace content {

TestContentBrowserClient::TestContentBrowserClient() {
}

TestContentBrowserClient::~TestContentBrowserClient() {
}

base::FilePath TestContentBrowserClient::GetDefaultDownloadDirectory() {
  if (!download_dir_.IsValid()) {
    bool result = download_dir_.CreateUniqueTempDir();
    CHECK(result);
  }
  return download_dir_.GetPath();
}

void TestContentBrowserClient::GetQuotaSettings(
    BrowserContext* context,
    StoragePartition* partition,
    storage::OptionalQuotaSettingsCallback callback) {
  std::move(callback).Run(storage::GetHardCodedSettings(100 * 1024 * 1024));
}

#if defined(OS_ANDROID)
void TestContentBrowserClient::GetAdditionalMappedFilesForChildProcess(
    const base::CommandLine& command_line,
    int child_process_id,
    content::PosixFileDescriptorInfo* mappings) {
  mappings->ShareWithRegion(
      kShellPakDescriptor,
      base::GlobalDescriptors::GetInstance()->Get(kShellPakDescriptor),
      base::GlobalDescriptors::GetInstance()->GetRegion(kShellPakDescriptor));
}
#endif
}  // namespace content
