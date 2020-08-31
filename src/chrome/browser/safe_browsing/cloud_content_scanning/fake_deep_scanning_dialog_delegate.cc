// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/cloud_content_scanning/fake_deep_scanning_dialog_delegate.h"

#include <base/callback.h>
#include <base/logging.h>
#include "base/task/post_task.h"
#include "base/time/time.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/binary_upload_service.h"
#include "content/public/browser/browser_thread.h"

namespace safe_browsing {

namespace {

base::TimeDelta response_delay = base::TimeDelta::FromSeconds(0);

}  // namespace

BinaryUploadService::Result FakeDeepScanningDialogDelegate::result_ =
    BinaryUploadService::Result::SUCCESS;

FakeDeepScanningDialogDelegate::FakeDeepScanningDialogDelegate(
    base::RepeatingClosure delete_closure,
    StatusCallback status_callback,
    EncryptionStatusCallback encryption_callback,
    std::string dm_token,
    content::WebContents* web_contents,
    Data data,
    CompletionCallback callback)
    : DeepScanningDialogDelegate(web_contents,
                                 std::move(data),
                                 std::move(callback),
                                 DeepScanAccessPoint::UPLOAD),
      delete_closure_(delete_closure),
      status_callback_(status_callback),
      encryption_callback_(encryption_callback),
      dm_token_(std::move(dm_token)) {}

FakeDeepScanningDialogDelegate::~FakeDeepScanningDialogDelegate() {
  if (!delete_closure_.is_null())
    delete_closure_.Run();
}

// static
void FakeDeepScanningDialogDelegate::SetResponseResult(
    BinaryUploadService::Result result) {
  result_ = result;
}

// static
std::unique_ptr<DeepScanningDialogDelegate>
FakeDeepScanningDialogDelegate::Create(
    base::RepeatingClosure delete_closure,
    StatusCallback status_callback,
    EncryptionStatusCallback encryption_callback,
    std::string dm_token,
    content::WebContents* web_contents,
    Data data,
    CompletionCallback callback) {
  auto ret = std::make_unique<FakeDeepScanningDialogDelegate>(
      delete_closure, status_callback, encryption_callback, std::move(dm_token),
      web_contents, std::move(data), std::move(callback));
  return ret;
}

// static
void FakeDeepScanningDialogDelegate::SetResponseDelay(base::TimeDelta delay) {
  response_delay = delay;
}

// static
DeepScanningClientResponse FakeDeepScanningDialogDelegate::SuccessfulResponse(
    bool include_dlp,
    bool include_malware) {
  DeepScanningClientResponse response;
  if (include_dlp) {
    response.mutable_dlp_scan_verdict()->set_status(
        DlpDeepScanningVerdict::SUCCESS);
  }
  if (include_malware) {
    response.mutable_malware_scan_verdict()->set_verdict(
        MalwareDeepScanningVerdict::CLEAN);
  }

  return response;
}

// static
DeepScanningClientResponse FakeDeepScanningDialogDelegate::MalwareResponse(
    MalwareDeepScanningVerdict::Verdict verdict) {
  DeepScanningClientResponse response;
  response.mutable_malware_scan_verdict()->set_verdict(verdict);
  return response;
}

// static
DeepScanningClientResponse FakeDeepScanningDialogDelegate::DlpResponse(
    DlpDeepScanningVerdict::Status status,
    const std::string& rule_name,
    DlpDeepScanningVerdict::TriggeredRule::Action action) {
  DeepScanningClientResponse response;
  response.mutable_dlp_scan_verdict()->set_status(status);

  if (!rule_name.empty()) {
    DlpDeepScanningVerdict::TriggeredRule* rule =
        response.mutable_dlp_scan_verdict()->add_triggered_rules();
    rule->set_rule_name(rule_name);
    rule->set_action(action);
  }

  return response;
}

// static
DeepScanningClientResponse
FakeDeepScanningDialogDelegate::MalwareAndDlpResponse(
    MalwareDeepScanningVerdict::Verdict verdict,
    DlpDeepScanningVerdict::Status status,
    const std::string& rule_name,
    DlpDeepScanningVerdict::TriggeredRule::Action action) {
  DeepScanningClientResponse response;
  *response.mutable_malware_scan_verdict() =
      FakeDeepScanningDialogDelegate::MalwareResponse(verdict)
          .malware_scan_verdict();
  *response.mutable_dlp_scan_verdict() =
      FakeDeepScanningDialogDelegate::DlpResponse(status, rule_name, action)
          .dlp_scan_verdict();
  return response;
}

void FakeDeepScanningDialogDelegate::Response(
    base::FilePath path,
    std::unique_ptr<BinaryUploadService::Request> request) {
  DeepScanningClientResponse response =
      (status_callback_.is_null() ||
       result_ != BinaryUploadService::Result::SUCCESS)
          ? DeepScanningClientResponse()
          : status_callback_.Run(path);
  if (path.empty())
    StringRequestCallback(result_, response);
  else
    FileRequestCallback(path, result_, response);
}

void FakeDeepScanningDialogDelegate::UploadTextForDeepScanning(
    std::unique_ptr<BinaryUploadService::Request> request) {
  DCHECK_EQ(
      DlpDeepScanningClientRequest::WEB_CONTENT_UPLOAD,
      request->deep_scanning_request().dlp_scan_request().content_source());
  DCHECK_EQ(dm_token_, request->deep_scanning_request().dm_token());

  // Simulate a response.
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&FakeDeepScanningDialogDelegate::Response,
                     weakptr_factory_.GetWeakPtr(), base::FilePath(),
                     std::move(request)),
      response_delay);
}

void FakeDeepScanningDialogDelegate::UploadFileForDeepScanning(
    BinaryUploadService::Result result,
    const base::FilePath& path,
    std::unique_ptr<BinaryUploadService::Request> request) {
  DCHECK(!path.empty());
  DCHECK_EQ(dm_token_, request->deep_scanning_request().dm_token());

  // Simulate a response.
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&FakeDeepScanningDialogDelegate::Response,
                     weakptr_factory_.GetWeakPtr(), path, std::move(request)),
      response_delay);
}

}  // namespace safe_browsing
