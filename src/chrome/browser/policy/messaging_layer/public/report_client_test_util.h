// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_MESSAGING_LAYER_PUBLIC_REPORT_CLIENT_TEST_UTIL_H_
#define CHROME_BROWSER_POLICY_MESSAGING_LAYER_PUBLIC_REPORT_CLIENT_TEST_UTIL_H_

#include "report_client.h"

#include "base/files/file_path.h"
#include "base/strings/string_piece.h"

namespace reporting {

class ReportingClient::TestEnvironment {
 public:
  TestEnvironment(const TestEnvironment& other) = delete;
  TestEnvironment& operator=(const TestEnvironment& other) = delete;
  TestEnvironment(const base::FilePath& reporting_path,
                  base::StringPiece verification_key,
                  policy::CloudPolicyClient* client);
  ~TestEnvironment();

 private:
  ReportingClient::StorageModuleCreateCallback saved_storage_create_cb_;
  GetCloudPolicyClientCallback saved_build_cloud_policy_client_cb_;
  std::unique_ptr<EncryptedReportingUploadProvider> saved_upload_provider_;
};
}  // namespace reporting

#endif  // CHROME_BROWSER_POLICY_MESSAGING_LAYER_PUBLIC_REPORT_CLIENT_TEST_UTIL_H_
