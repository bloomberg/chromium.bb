// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/persistence/linux_key_persistence_delegate.h"

#include <fcntl.h>
#include <grp.h>
#include <sys/file.h>
#include <sys/stat.h>

#include <string>
#include <vector>

#include "base/base64.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/posix/eintr_wrapper.h"
#include "base/syslog_logging.h"
#include "base/values.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/shared_command_constants.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

using BPKUR = enterprise_management::BrowserPublicKeyUploadRequest;
using BPKUP = enterprise_management::BrowserPublicKeyUploadResponse;

namespace enterprise_connectors {

namespace {

// The mode the signing key file should have.
constexpr int kFileMode = 0664;

constexpr int kMaxBufferSize = 2048;
constexpr char kSigningKeyName[] = "signingKey";
constexpr char kSigningKeyTrustLevel[] = "trustLevel";

// The path to the policy file should be the same as the
// chrome::DIR_POLICY_FILES. This code runs in the chrome-management-service
// and thus cannot directly use chrome::DIR_POLICY_FILES
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
base::FilePath::CharType kDirPolicyPath[] =
    FILE_PATH_LITERAL("/etc/opt/chrome/policies");
#else
base::FilePath::CharType kDirPolicyPath[] =
    FILE_PATH_LITERAL("/etc/chromium/policies");
#endif

absl::optional<base::FilePath>& GetTestFilePathStorage() {
  static base::NoDestructor<absl::optional<base::FilePath>> storage;
  return *storage;
}

base::FilePath GetSigningKeyFilePath() {
  auto& storage = GetTestFilePathStorage();
  if (storage) {
    return storage.value();
  }
  base::FilePath path(kDirPolicyPath);
  return path.Append(constants::kSigningKeyFilePath);
}

base::File OpenSigningKeyFile(uint32_t flags) {
  return base::File(GetSigningKeyFilePath(), flags);
}

bool LogFailure(const std::string& log_message) {
  SYSLOG(ERROR) << log_message;
  return false;
}

}  // namespace

LinuxKeyPersistenceDelegate::LinuxKeyPersistenceDelegate() = default;
LinuxKeyPersistenceDelegate::~LinuxKeyPersistenceDelegate() = default;

bool LinuxKeyPersistenceDelegate::CheckRotationPermissions() {
  base::FilePath signing_key_path = GetSigningKeyFilePath();
  locked_file_ = base::File(signing_key_path, base::File::FLAG_OPEN |
                                                  base::File::FLAG_READ |
                                                  base::File::FLAG_WRITE);
  if (!locked_file_ || !locked_file_->IsValid() ||
      HANDLE_EINTR(flock(locked_file_->GetPlatformFile(), LOCK_EX | LOCK_NB)) ==
          -1) {
    return LogFailure(
        "Device trust key rotation failed. Could not acquire lock on the "
        "signing key storage.");
  }

  int mode;
  if (!base::GetPosixFilePermissions(signing_key_path, &mode))
    return LogFailure(
        "Device trust key rotation failed. Could not get permissions "
        "for the signing key storage.");

  struct stat st;
  stat(signing_key_path.value().c_str(), &st);
  gid_t signing_key_file_gid = st.st_gid;
  struct group* chrome_mgmt_group = getgrnam(constants::kGroupName);

  if (!chrome_mgmt_group || signing_key_file_gid != chrome_mgmt_group->gr_gid ||
      mode != kFileMode)
    return LogFailure(
        "Device trust key rotation failed. Incorrect permissions "
        "for the signing key storage.");
  return true;
}

bool LinuxKeyPersistenceDelegate::StoreKeyPair(
    KeyPersistenceDelegate::KeyTrustLevel trust_level,
    std::vector<uint8_t> wrapped) {
  base::File file = OpenSigningKeyFile(base::File::FLAG_OPEN_TRUNCATED |
                                       base::File::FLAG_WRITE);
  if (trust_level == BPKUR::KEY_TRUST_LEVEL_UNSPECIFIED) {
    DCHECK_EQ(wrapped.size(), 0u);
    return file.error_details() == base::File::FILE_OK;
  }

  if (!file.IsValid())
    return LogFailure(
        "Device trust key rotation failed. Could not open the signing key file "
        "for writing.");

  // Storing key and trust level information.
  base::Value keyinfo(base::Value::Type::DICTIONARY);
  const std::string encoded_key = base::Base64Encode(wrapped);
  keyinfo.SetKey(kSigningKeyName, base::Value(encoded_key));
  keyinfo.SetKey(kSigningKeyTrustLevel, base::Value(trust_level));
  std::string keyinfo_str;
  if (!base::JSONWriter::Write(keyinfo, &keyinfo_str))
    return LogFailure(
        "Device trust key rotation failed. Could not format signing key "
        "information for storage.");

  bool write_result =
      file.WriteAtCurrentPos(keyinfo_str.c_str(), keyinfo_str.length()) > 0
          ? true
          : LogFailure(
                "Device trust key rotation failed. Could not write to the "
                "signing key storage.");

  return write_result;
}

KeyPersistenceDelegate::KeyInfo LinuxKeyPersistenceDelegate::LoadKeyPair() {
  std::string file_content;
  if (!base::ReadFileToStringWithMaxSize(GetSigningKeyFilePath(), &file_content,
                                         kMaxBufferSize)) {
    return invalid_key_info();
  }

  // Get dictionary key info.
  auto keyinfo = base::JSONReader::Read(file_content);
  if (!keyinfo || !keyinfo->is_dict()) {
    return invalid_key_info();
  }

  // Get the trust level.
  auto stored_trust_level = keyinfo->FindIntKey(kSigningKeyTrustLevel);
  KeyTrustLevel trust_level = BPKUR::KEY_TRUST_LEVEL_UNSPECIFIED;
  if (stored_trust_level == BPKUR::CHROME_BROWSER_TPM_KEY) {
    trust_level = BPKUR::CHROME_BROWSER_TPM_KEY;
  } else if (stored_trust_level == BPKUR::CHROME_BROWSER_OS_KEY) {
    trust_level = BPKUR::CHROME_BROWSER_OS_KEY;
  } else {
    return invalid_key_info();
  }

  // Get the key.
  std::string* encoded_key = keyinfo->FindStringKey(kSigningKeyName);
  std::string decoded_key;

  if (!encoded_key) {
    return invalid_key_info();
  }

  if (!base::Base64Decode(*encoded_key, &decoded_key)) {
    return invalid_key_info();
  }

  std::vector<uint8_t> key(decoded_key.begin(), decoded_key.end());
  return std::make_pair(trust_level, key);
}

std::unique_ptr<crypto::UnexportableKeyProvider>
LinuxKeyPersistenceDelegate::GetTpmBackedKeyProvider() {
  // TODO (http://b/210343211)
  return nullptr;
}

// static
void LinuxKeyPersistenceDelegate::SetFilePathForTesting(
    const base::FilePath& file_path) {
  auto& storage = GetTestFilePathStorage();
  storage.emplace(file_path);
}

}  // namespace enterprise_connectors
