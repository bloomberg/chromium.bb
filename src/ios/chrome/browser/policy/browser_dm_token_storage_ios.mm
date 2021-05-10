// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/policy/browser_dm_token_storage_ios.h"

#import <Foundation/Foundation.h>

#include "base/base64url.h"
#include "base/files/file_util.h"
#include "base/files/important_file_writer.h"
#include "base/hash/sha1.h"
#include "base/ios/device_util.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#import "components/policy/core/common/policy_loader_ios_constants.h"
#include "ios/chrome/browser/file_metadata_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace policy {

namespace {

const char kDmTokenBaseDir[] =
    FILE_PATH_LITERAL("Google/Chrome Cloud Enrollment/");
const char kEnrollmentTokenPolicyName[] = "CloudManagementEnrollmentToken";

bool GetDmTokenFilePath(base::FilePath* token_file_path,
                        const std::string& client_id,
                        bool create_dir) {
  if (!base::PathService::Get(base::DIR_APP_DATA, token_file_path))
    return false;

  *token_file_path = token_file_path->Append(kDmTokenBaseDir);

  if (create_dir && !base::CreateDirectory(*token_file_path))
    return false;

  std::string filename;
  base::Base64UrlEncode(base::SHA1HashString(client_id),
                        base::Base64UrlEncodePolicy::OMIT_PADDING, &filename);
  *token_file_path = token_file_path->Append(filename.c_str());

  return true;
}

bool StoreDMTokenInDirAppDataDir(const std::string& token,
                                 const std::string& client_id) {
  base::FilePath token_file_path;
  if (!GetDmTokenFilePath(&token_file_path, client_id, true)) {
    NOTREACHED();
    return false;
  }

  if (!base::ImportantFileWriter::WriteFileAtomically(token_file_path, token)) {
    return false;
  }

  SetSkipSystemBackupAttributeToItem(token_file_path, true);
  return true;
}

}  // namespace

BrowserDMTokenStorageIOS::BrowserDMTokenStorageIOS()
    : task_runner_(base::ThreadPool::CreateTaskRunner({base::MayBlock()})) {}

BrowserDMTokenStorageIOS::~BrowserDMTokenStorageIOS() {}

std::string BrowserDMTokenStorageIOS::InitClientId() {
  return ios::device_util::GetVendorId();
}

std::string BrowserDMTokenStorageIOS::InitEnrollmentToken() {
  NSDictionary* raw_policies = [[NSUserDefaults standardUserDefaults]
      dictionaryForKey:kPolicyLoaderIOSConfigurationKey];
  NSString* token =
      raw_policies[base::SysUTF8ToNSString(kEnrollmentTokenPolicyName)];

  if (token) {
    return base::TrimWhitespaceASCII(base::SysNSStringToUTF8(token),
                                     base::TRIM_ALL)
        .as_string();
  }

  return std::string();
}

std::string BrowserDMTokenStorageIOS::InitDMToken() {
  base::FilePath token_file_path;
  if (!GetDmTokenFilePath(&token_file_path, InitClientId(), false))
    return std::string();

  std::string token;
  if (!base::ReadFileToString(token_file_path, &token))
    return std::string();

  return base::TrimWhitespaceASCII(token, base::TRIM_ALL).as_string();
}

bool BrowserDMTokenStorageIOS::InitEnrollmentErrorOption() {
  // No error should be shown if enrollment fails on iOS.
  return false;
}

BrowserDMTokenStorage::StoreTask BrowserDMTokenStorageIOS::SaveDMTokenTask(
    const std::string& token,
    const std::string& client_id) {
  return base::BindOnce(&StoreDMTokenInDirAppDataDir, token, client_id);
}

scoped_refptr<base::TaskRunner>
BrowserDMTokenStorageIOS::SaveDMTokenTaskRunner() {
  return task_runner_;
}

}  // namespace policy
