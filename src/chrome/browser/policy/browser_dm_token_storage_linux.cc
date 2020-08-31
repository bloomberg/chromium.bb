// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/browser_dm_token_storage_linux.h"

#include <string>

#include "base/base64url.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/files/file_util.h"
#include "base/files/important_file_writer.h"
#include "base/hash/sha1.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/syslog_logging.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/task_runner_util.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "chrome/common/chrome_paths.h"

namespace policy {

namespace {

const char kDmTokenBaseDir[] = FILE_PATH_LITERAL("Policy/Enrollment/");
const char kEnrollmentTokenFilename[] =
    FILE_PATH_LITERAL("enrollment/CloudManagementEnrollmentToken");
const char kMachineIdFilename[] = FILE_PATH_LITERAL("/etc/machine-id");

// Enrollment Mandatory Option.
const char kEnrollmentOptionsFilePath[] =
    FILE_PATH_LITERAL("enrollment/CloudManagementEnrollmentOptions");
const char kEnrollmentMandatoryOption[] = "Mandatory";

bool GetDmTokenFilePath(base::FilePath* token_file_path,
                        const std::string& client_id,
                        bool create_dir) {
  if (!base::PathService::Get(chrome::DIR_USER_DATA, token_file_path))
    return false;

  *token_file_path = token_file_path->Append(kDmTokenBaseDir);

  if (create_dir && !base::CreateDirectory(*token_file_path))
    return false;

  *token_file_path = token_file_path->Append(client_id);

  return true;
}

bool StoreDMTokenInUserDataDir(const std::string& token,
                               const std::string& client_id) {
  base::FilePath token_file_path;
  if (!GetDmTokenFilePath(&token_file_path, client_id, true)) {
    NOTREACHED();
    return false;
  }

  return base::ImportantFileWriter::WriteFileAtomically(token_file_path, token);
}

}  // namespace

// static
BrowserDMTokenStorage* BrowserDMTokenStorage::Get() {
  if (storage_for_testing_)
    return storage_for_testing_;

  static base::NoDestructor<BrowserDMTokenStorageLinux> storage;
  return storage.get();
}

BrowserDMTokenStorageLinux::BrowserDMTokenStorageLinux()
    : task_runner_(base::ThreadPool::CreateTaskRunner({base::MayBlock()})) {}

BrowserDMTokenStorageLinux::~BrowserDMTokenStorageLinux() {}

std::string BrowserDMTokenStorageLinux::InitClientId() {
  // The client ID is derived from /etc/machine-id
  // (https://www.freedesktop.org/software/systemd/man/machine-id.html). As per
  // guidelines, this ID must not be transmitted outside of the machine, which
  // is why we hash it first and then encode it in base64 before transmitting
  // it.
  const int machine_id_size = 32;
  std::string machine_id;
  machine_id = ReadMachineIdFile();
  base::StringPiece machine_id_trimmed =
      base::TrimWhitespaceASCII(machine_id, base::TRIM_TRAILING);
  if (machine_id_trimmed.size() != machine_id_size) {
    SYSLOG(ERROR) << "Error: /etc/machine-id contains "
                  << machine_id_trimmed.size() << " characters ("
                  << machine_id_size << " were expected).";
    return std::string();
  }

  std::string machine_id_base64;
  base::Base64UrlEncode(base::SHA1HashString(std::string(machine_id_trimmed)),
                        base::Base64UrlEncodePolicy::OMIT_PADDING,
                        &machine_id_base64);
  return machine_id_base64;
}

std::string BrowserDMTokenStorageLinux::InitEnrollmentToken() {
  std::string enrollment_token;
  base::FilePath dir_policy_files_path;

  if (!base::PathService::Get(chrome::DIR_POLICY_FILES,
                              &dir_policy_files_path)) {
    return std::string();
  }

  base::FilePath token_file_path =
      dir_policy_files_path.Append(kEnrollmentTokenFilename);

  if (!base::ReadFileToString(token_file_path, &enrollment_token))
    return std::string();

  return base::TrimWhitespaceASCII(enrollment_token, base::TRIM_ALL)
      .as_string();
}

std::string BrowserDMTokenStorageLinux::InitDMToken() {
  base::FilePath token_file_path;
  if (!GetDmTokenFilePath(&token_file_path, RetrieveClientId(), false))
    return std::string();

  std::string token;
  if (!base::ReadFileToString(token_file_path, &token))
    return std::string();

  return base::TrimWhitespaceASCII(token, base::TRIM_ALL).as_string();
}

bool BrowserDMTokenStorageLinux::InitEnrollmentErrorOption() {
  std::string options;
  base::FilePath dir_policy_files_path;

  if (!base::PathService::Get(chrome::DIR_POLICY_FILES,
                              &dir_policy_files_path)) {
    return false;
  }

  base::FilePath options_file_path =
      dir_policy_files_path.Append(kEnrollmentOptionsFilePath);

  if (!base::ReadFileToString(options_file_path, &options))
    return false;

  return base::TrimWhitespaceASCII(options, base::TRIM_ALL).as_string() ==
         kEnrollmentMandatoryOption;
}

BrowserDMTokenStorage::StoreTask BrowserDMTokenStorageLinux::SaveDMTokenTask(
    const std::string& token,
    const std::string& client_id) {
  return base::BindOnce(&StoreDMTokenInUserDataDir, token, client_id);
}

scoped_refptr<base::TaskRunner>
BrowserDMTokenStorageLinux::SaveDMTokenTaskRunner() {
  return task_runner_;
}

std::string BrowserDMTokenStorageLinux::ReadMachineIdFile() {
  std::string machine_id;
  if (!base::ReadFileToString(base::FilePath(kMachineIdFilename), &machine_id))
    return std::string();
  return machine_id;
}

}  // namespace policy
