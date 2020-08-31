// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/cloud_content_scanning/deep_scanning_browsertest_base.h"
#include "base/bind_helpers.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/deep_scanning_dialog_views.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/fake_deep_scanning_dialog_delegate.h"
#include "chrome/browser/safe_browsing/dm_token_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/safe_browsing/core/features.h"

namespace safe_browsing {

namespace {

constexpr char kDmToken[] = "dm_token";

constexpr base::TimeDelta kInitialUIDelay =
    base::TimeDelta::FromMilliseconds(100);
constexpr base::TimeDelta kMinimumPendingDelay =
    base::TimeDelta::FromMilliseconds(400);
constexpr base::TimeDelta kSuccessTimeout =
    base::TimeDelta::FromMilliseconds(100);

class UnresponsiveDeepScanningDialogDelegate
    : public FakeDeepScanningDialogDelegate {
 public:
  using FakeDeepScanningDialogDelegate::FakeDeepScanningDialogDelegate;

  static std::unique_ptr<DeepScanningDialogDelegate> Create(
      base::RepeatingClosure delete_closure,
      StatusCallback status_callback,
      EncryptionStatusCallback encryption_callback,
      std::string dm_token,
      content::WebContents* web_contents,
      Data data,
      CompletionCallback callback) {
    auto ret = std::make_unique<UnresponsiveDeepScanningDialogDelegate>(
        delete_closure, status_callback, encryption_callback,
        std::move(dm_token), web_contents, std::move(data),
        std::move(callback));
    return ret;
  }

 private:
  void UploadTextForDeepScanning(
      std::unique_ptr<BinaryUploadService::Request> request) override {
    // Do nothing.
  }

  void UploadFileForDeepScanning(
      BinaryUploadService::Result result,
      const base::FilePath& path,
      std::unique_ptr<BinaryUploadService::Request> request) override {
    // Do nothing.
  }
};

}  // namespace

DeepScanningBrowserTestBase::DeepScanningBrowserTestBase() {
  // Enable every deep scanning features.
  scoped_feature_list_.InitWithFeatures(
      {kContentComplianceEnabled, kMalwareScanEnabled}, {});

  // Change the time values of the upload UI to smaller ones to make tests
  // showing it run faster.
  DeepScanningDialogViews::SetInitialUIDelayForTesting(kInitialUIDelay);
  DeepScanningDialogViews::SetMinimumPendingDialogTimeForTesting(
      kMinimumPendingDelay);
  DeepScanningDialogViews::SetSuccessDialogTimeoutForTesting(kSuccessTimeout);
}

DeepScanningBrowserTestBase::~DeepScanningBrowserTestBase() = default;

void DeepScanningBrowserTestBase::TearDownOnMainThread() {
  DeepScanningDialogDelegate::ResetFactoryForTesting();

  SetDlpPolicy(CheckContentComplianceValues::CHECK_NONE);
  SetMalwarePolicy(SendFilesForMalwareCheckValues::DO_NOT_SCAN);
  SetWaitPolicy(DelayDeliveryUntilVerdictValues::DELAY_NONE);
  SetAllowPasswordProtectedFilesPolicy(
      AllowPasswordProtectedFilesValues::ALLOW_UPLOADS_AND_DOWNLOADS);
  SetBlockUnsupportedFileTypesPolicy(
      BlockUnsupportedFiletypesValues::BLOCK_UNSUPPORTED_FILETYPES_NONE);
  SetBlockLargeFileTransferPolicy(BlockLargeFileTransferValues::BLOCK_NONE);
  SetUnsafeEventsReportingPolicy(false);
}

void DeepScanningBrowserTestBase::SetDlpPolicy(
    CheckContentComplianceValues state) {
  g_browser_process->local_state()->SetInteger(prefs::kCheckContentCompliance,
                                               state);
}

void DeepScanningBrowserTestBase::SetMalwarePolicy(
    SendFilesForMalwareCheckValues state) {
  browser()->profile()->GetPrefs()->SetInteger(
      prefs::kSafeBrowsingSendFilesForMalwareCheck, state);
}

void DeepScanningBrowserTestBase::SetWaitPolicy(
    DelayDeliveryUntilVerdictValues state) {
  g_browser_process->local_state()->SetInteger(
      prefs::kDelayDeliveryUntilVerdict, state);
}

void DeepScanningBrowserTestBase::SetAllowPasswordProtectedFilesPolicy(
    AllowPasswordProtectedFilesValues state) {
  g_browser_process->local_state()->SetInteger(
      prefs::kAllowPasswordProtectedFiles, state);
}

void DeepScanningBrowserTestBase::SetBlockUnsupportedFileTypesPolicy(
    BlockUnsupportedFiletypesValues state) {
  g_browser_process->local_state()->SetInteger(
      prefs::kBlockUnsupportedFiletypes, state);
}

void DeepScanningBrowserTestBase::SetBlockLargeFileTransferPolicy(
    BlockLargeFileTransferValues state) {
  g_browser_process->local_state()->SetInteger(prefs::kBlockLargeFileTransfer,
                                               state);
}

void DeepScanningBrowserTestBase::SetUnsafeEventsReportingPolicy(bool report) {
  g_browser_process->local_state()->SetBoolean(
      prefs::kUnsafeEventsReportingEnabled, report);
}

void DeepScanningBrowserTestBase::AddUrlToCheckComplianceOfDownloads(
    const std::string& url) {
  ListPrefUpdate(g_browser_process->local_state(),
                 prefs::kURLsToCheckComplianceOfDownloadedContent)
      ->Append(url);
}

void DeepScanningBrowserTestBase::SetUpDelegate() {
  SetDMTokenForTesting(policy::DMToken::CreateValidTokenForTesting(kDmToken));
  DeepScanningDialogDelegate::SetFactoryForTesting(base::BindRepeating(
      &FakeDeepScanningDialogDelegate::Create, base::DoNothing(),
      base::Bind(&DeepScanningBrowserTestBase::StatusCallback,
                 base::Unretained(this)),
      base::Bind(&DeepScanningBrowserTestBase::EncryptionStatusCallback,
                 base::Unretained(this)),
      kDmToken));
}

void DeepScanningBrowserTestBase::SetUpUnresponsiveDelegate() {
  SetDMTokenForTesting(policy::DMToken::CreateValidTokenForTesting(kDmToken));
  DeepScanningDialogDelegate::SetFactoryForTesting(base::BindRepeating(
      &UnresponsiveDeepScanningDialogDelegate::Create, base::DoNothing(),
      base::Bind(&DeepScanningBrowserTestBase::StatusCallback,
                 base::Unretained(this)),
      base::Bind(&DeepScanningBrowserTestBase::EncryptionStatusCallback,
                 base::Unretained(this)),
      kDmToken));
}

void DeepScanningBrowserTestBase::SetQuitClosure(
    base::RepeatingClosure quit_closure) {
  quit_closure_ = quit_closure;
}

void DeepScanningBrowserTestBase::CallQuitClosure() {
  if (!quit_closure_.is_null())
    quit_closure_.Run();
}

void DeepScanningBrowserTestBase::SetStatusCallbackResponse(
    DeepScanningClientResponse response) {
  status_callback_response_ = response;
}

DeepScanningClientResponse DeepScanningBrowserTestBase::StatusCallback(
    const base::FilePath& path) {
  return status_callback_response_;
}

bool DeepScanningBrowserTestBase::EncryptionStatusCallback(
    const base::FilePath& path) {
  return false;
}

void DeepScanningBrowserTestBase::CreateFilesForTest(
    const std::vector<std::string>& paths,
    const std::vector<std::string>& contents,
    DeepScanningDialogDelegate::Data* data) {
  ASSERT_EQ(paths.size(), contents.size());

  ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

  for (size_t i = 0; i < paths.size(); ++i) {
    base::FilePath path = temp_dir_.GetPath().AppendASCII(paths[i]);
    created_file_paths_.emplace_back(path);
    base::File file(path, base::File::FLAG_CREATE | base::File::FLAG_WRITE);
    file.WriteAtCurrentPos(contents[i].data(), contents[i].size());
    data->paths.emplace_back(path);
  }
}

const std::vector<base::FilePath>&
DeepScanningBrowserTestBase::created_file_paths() const {
  return created_file_paths_;
}

}  // namespace safe_browsing
