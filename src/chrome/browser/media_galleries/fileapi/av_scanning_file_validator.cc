// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media_galleries/fileapi/av_scanning_file_validator.h"

#if defined(OS_WIN)
#include <windows.h>
#include <objbase.h>
#include <shlobj.h>
#include <wrl/client.h>
#endif

#include <string>

#include "base/bind.h"
#include "base/callback.h"
#include "base/feature_list.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/task_runner_util.h"
#include "base/threading/scoped_blocking_call.h"
#include "build/build_config.h"
#include "chrome/common/chrome_constants.h"
#include "components/download/quarantine/quarantine.h"
#include "content/public/browser/browser_thread.h"
#include "url/gurl.h"

using content::BrowserThread;

namespace {

#if defined(OS_WIN)
// This feature controls whether the QuarantineFile() function should be used
// to perform the AV scan instead of directly invoking the IAttachmentExecute
// interface.
constexpr base::Feature kMediaGalleriesQuarantineFile{
    "MediaGalleriesQuarantineFile", base::FEATURE_DISABLED_BY_DEFAULT};

base::File::Error ScanFile(const base::FilePath& dest_platform_path) {
  base::ScopedBlockingCall scoped_blocking_call(base::BlockingType::MAY_BLOCK);

  if (base::FeatureList::IsEnabled(kMediaGalleriesQuarantineFile)) {
    download::QuarantineFileResult quarantine_result = download::QuarantineFile(
        dest_platform_path, GURL(), GURL(), std::string());

    return quarantine_result == download::QuarantineFileResult::OK
               ? base::File::FILE_OK
               : base::File::FILE_ERROR_SECURITY;
  }

  Microsoft::WRL::ComPtr<IAttachmentExecute> attachment_services;
  HRESULT hr = ::CoCreateInstance(CLSID_AttachmentServices, nullptr, CLSCTX_ALL,
                                  IID_PPV_ARGS(&attachment_services));

  if (FAILED(hr)) {
    // The thread must have COM initialized.
    DCHECK_NE(CO_E_NOTINITIALIZED, hr);
    return base::File::FILE_ERROR_SECURITY;
  }

  hr = attachment_services->SetLocalPath(dest_platform_path.value().c_str());
  if (FAILED(hr))
    return base::File::FILE_ERROR_SECURITY;

  // A failure in the Save() call below could result in the downloaded file
  // being deleted.
  HRESULT scan_result = attachment_services->Save();
  if (scan_result == S_OK)
    return base::File::FILE_OK;

  return base::File::FILE_ERROR_SECURITY;
}
#endif

}  // namespace

AVScanningFileValidator::~AVScanningFileValidator() {}

void AVScanningFileValidator::StartPostWriteValidation(
    const base::FilePath& dest_platform_path,
    const ResultCallback& result_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

#if defined(OS_WIN)
  base::PostTaskAndReplyWithResult(
      base::CreateCOMSTATaskRunnerWithTraits(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE})
          .get(),
      FROM_HERE, base::Bind(&ScanFile, dest_platform_path), result_callback);
#else
  result_callback.Run(base::File::FILE_OK);
#endif
}

AVScanningFileValidator::AVScanningFileValidator() {
}
