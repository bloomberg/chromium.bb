// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media_galleries/fileapi/av_scanning_file_validator.h"

#include <string>

#include "base/callback.h"
#include "build/build_config.h"
#include "components/download/public/common/quarantine_connection.h"
#include "content/public/browser/browser_thread.h"

#if defined(OS_WIN)
#include "base/bind.h"
#include "base/files/file_path.h"
#include "components/services/quarantine/public/mojom/quarantine.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "url/gurl.h"
#endif  // defined(OS_WIN)

namespace {

#if defined(OS_WIN)
void OnFileQuarantined(AVScanningFileValidator::ResultCallback result_callback,
                       quarantine::mojom::QuarantineFileResult result) {
  std::move(result_callback)
      .Run(result == quarantine::mojom::QuarantineFileResult::OK
               ? base::File::FILE_OK
               : base::File::FILE_ERROR_SECURITY);
}

void ScanFile(
    const base::FilePath& dest_platform_path,
    download::QuarantineConnectionCallback quarantine_connection_callback,
    AVScanningFileValidator::ResultCallback result_callback) {
  mojo::Remote<quarantine::mojom::Quarantine> quarantine_remote;
  if (quarantine_connection_callback) {
    quarantine_connection_callback.Run(
        quarantine_remote.BindNewPipeAndPassReceiver());
  }

  if (quarantine_remote) {
    quarantine_remote->QuarantineFile(
        dest_platform_path, GURL(), GURL(), std::string(),
        base::BindOnce(&OnFileQuarantined, std::move(result_callback)));
  } else {
    std::move(result_callback).Run(base::File::FILE_OK);
  }
}
#endif  // defined(OS_WIN)

}  // namespace

AVScanningFileValidator::~AVScanningFileValidator() {}

void AVScanningFileValidator::StartPostWriteValidation(
    const base::FilePath& dest_platform_path,
    ResultCallback result_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

#if defined(OS_WIN)
  ScanFile(dest_platform_path, quarantine_connection_callback_,
           std::move(result_callback));
#else
  std::move(result_callback).Run(base::File::FILE_OK);
#endif
}

AVScanningFileValidator::AVScanningFileValidator(
    download::QuarantineConnectionCallback quarantine_connection_callback)
    : quarantine_connection_callback_(quarantine_connection_callback) {}
