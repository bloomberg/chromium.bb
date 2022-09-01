// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/persistence/linux_key_persistence_delegate.h"

#include "base/base64.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/stringprintf.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

base::FilePath::CharType kFileName[] = FILE_PATH_LITERAL("test_file");

// Represents gibberish that gets appended to the file.
constexpr char kGibberish[] = "dfnsdfjdsn";

// Represents an OS key.
constexpr char kValidKeyWrappedBase64[] =
    "MIGHAgEAMBMGByqGSM49AgEGCCqGSM49AwEHBG0wawIBAQQg3VGyKUYrI0M5VOGIw0dh3D0s26"
    "0xeKGcOKZ76A+LTQuhRANCAAQ8rmn96lycvM/"
    "WTQn4FZnjucsKdj2YrUkcG42LWoC2WorIp8BETdwYr2OhGAVBmSVpg9iyi5gtZ9JGZzMceWOJ";

// String containing invalid base64 characters, like % and the whitespace.
constexpr char kInvalidBase64String[] = "? %";

constexpr char kValidTPMKeyFileContent[] =
    "{\"signingKey\":"
    "\"MIGHAgEAMBMGByqGSM49AgEGCCqGSM49AwEHBG0wawIBAQQg3VGyKUYrI0M5VOGIw0dh3D0s"
    "260xeKGcOKZ76A+LTQuhRANCAAQ8rmn96lycvM/"
    "WTQn4FZnjucsKdj2YrUkcG42LWoC2WorIp8BETdwYr2OhGAVBmSVpg9iyi5gtZ9JGZzMceWOJ"
    "\",\"trustLevel\":1}";
constexpr char kValidOSKeyFileContent[] =
    "{\"signingKey\":"
    "\"MIGHAgEAMBMGByqGSM49AgEGCCqGSM49AwEHBG0wawIBAQQg3VGyKUYrI0M5VOGIw0dh3D0s"
    "260xeKGcOKZ76A+LTQuhRANCAAQ8rmn96lycvM/"
    "WTQn4FZnjucsKdj2YrUkcG42LWoC2WorIp8BETdwYr2OhGAVBmSVpg9iyi5gtZ9JGZzMceWOJ"
    "\",\"trustLevel\":2}";
constexpr char kInvalidTrustLevelKeyFileContent[] =
    "{\"signingKey\":"
    "\"MIGHAgEAMBMGByqGSM49AgEGCCqGSM49AwEHBG0wawIBAQQg3VGyKUYrI0M5VOGIw0dh3D0s"
    "260xeKGcOKZ76A+LTQuhRANCAAQ8rmn96lycvM/"
    "WTQn4FZnjucsKdj2YrUkcG42LWoC2WorIp8BETdwYr2OhGAVBmSVpg9iyi5gtZ9JGZzMceWOJ"
    "\",\"trustLevel\":100}";

std::vector<uint8_t> ParseKeyWrapped(base::StringPiece encoded_wrapped) {
  std::string decoded_key;
  if (!base::Base64Decode(encoded_wrapped, &decoded_key)) {
    return std::vector<uint8_t>();
  }

  return std::vector<uint8_t>(decoded_key.begin(), decoded_key.end());
}

}  // namespace

namespace enterprise_connectors {

using BPKUR = enterprise_management::BrowserPublicKeyUploadRequest;

class LinuxKeyPersistenceDelegateTest : public testing::Test {
 public:
  LinuxKeyPersistenceDelegateTest() {
    EXPECT_TRUE(scoped_dir_.CreateUniqueTempDir());
    LinuxKeyPersistenceDelegate::SetFilePathForTesting(GetKeyFilePath());
  }

 protected:
  base::FilePath GetKeyFilePath() {
    return scoped_dir_.GetPath().Append(kFileName);
  }

  bool CreateFile(base::StringPiece content) {
    return base::WriteFile(GetKeyFilePath(), content);
  }

  std::string GetFileContents() {
    std::string file_contents;
    base::ReadFileToString(GetKeyFilePath(), &file_contents);
    return file_contents;
  }

  base::ScopedTempDir scoped_dir_;
  LinuxKeyPersistenceDelegate persistence_delegate_;
};

// Tests when the file does not exist and a write operation is attempted.
TEST_F(LinuxKeyPersistenceDelegateTest, StoreKeyPair_FileDoesNotExist) {
  EXPECT_FALSE(persistence_delegate_.StoreKeyPair(
      BPKUR::KEY_TRUST_LEVEL_UNSPECIFIED, std::vector<uint8_t>()));
  EXPECT_FALSE(base::PathExists(GetKeyFilePath()));
}

// Tests storing a key with an unspecified trust level.
TEST_F(LinuxKeyPersistenceDelegateTest, StoreKeyPair_UnspecifiedKey) {
  CreateFile("");
  EXPECT_TRUE(persistence_delegate_.StoreKeyPair(
      BPKUR::KEY_TRUST_LEVEL_UNSPECIFIED, std::vector<uint8_t>()));
  EXPECT_EQ("", GetFileContents());
}

// Tests when a OS key is stored and file contents are modified before storing
// a new OS key pair.
TEST_F(LinuxKeyPersistenceDelegateTest, StoreKeyPair_ValidOSKeyPair) {
  CreateFile("");
  EXPECT_TRUE(persistence_delegate_.StoreKeyPair(
      BPKUR::CHROME_BROWSER_OS_KEY, ParseKeyWrapped(kValidKeyWrappedBase64)));
  EXPECT_EQ(kValidOSKeyFileContent, GetFileContents());

  // Modifying file contents
  base::File file = base::File(GetKeyFilePath(),
                               base::File::FLAG_OPEN | base::File::FLAG_APPEND);
  EXPECT_TRUE(file.WriteAtCurrentPos(kGibberish, strlen(kGibberish)) > 0);
  std::string expected_file_contents(kValidOSKeyFileContent);
  expected_file_contents.append(kGibberish);
  EXPECT_EQ(expected_file_contents, GetFileContents());

  EXPECT_TRUE(persistence_delegate_.StoreKeyPair(
      BPKUR::CHROME_BROWSER_OS_KEY, ParseKeyWrapped(kValidKeyWrappedBase64)));
  EXPECT_EQ(kValidOSKeyFileContent, GetFileContents());
}

// Tests when a TPM key is stored and file contents are modified before storing
// a new TPM key pair.
TEST_F(LinuxKeyPersistenceDelegateTest, StoreKeyPair_ValidTPMKeyPair) {
  CreateFile("");
  EXPECT_TRUE(persistence_delegate_.StoreKeyPair(
      BPKUR::CHROME_BROWSER_TPM_KEY, ParseKeyWrapped(kValidKeyWrappedBase64)));
  EXPECT_EQ(kValidTPMKeyFileContent, GetFileContents());

  // Modifying file contents
  base::File file = base::File(GetKeyFilePath(),
                               base::File::FLAG_OPEN | base::File::FLAG_APPEND);
  EXPECT_TRUE(file.WriteAtCurrentPos(kGibberish, strlen(kGibberish)) > 0);
  std::string expected_file_contents(kValidTPMKeyFileContent);
  expected_file_contents.append(kGibberish);
  EXPECT_EQ(expected_file_contents, GetFileContents());

  EXPECT_TRUE(persistence_delegate_.StoreKeyPair(
      BPKUR::CHROME_BROWSER_TPM_KEY, ParseKeyWrapped(kValidKeyWrappedBase64)));
  EXPECT_EQ(kValidTPMKeyFileContent, GetFileContents());
}

// Tests trying to load a key when there is no file.
TEST_F(LinuxKeyPersistenceDelegateTest, LoadKeyPair_NoKeyFile) {
  auto [trust_level, wrapped] = persistence_delegate_.LoadKeyPair();
  EXPECT_EQ(trust_level, BPKUR::KEY_TRUST_LEVEL_UNSPECIFIED);
  EXPECT_TRUE(wrapped.empty());
}

// Tests loading a valid OS key from a key file.
TEST_F(LinuxKeyPersistenceDelegateTest, LoadKeyPair_ValidOSKeyFile) {
  ASSERT_TRUE(CreateFile(kValidOSKeyFileContent));
  auto [trust_level, wrapped] = persistence_delegate_.LoadKeyPair();
  EXPECT_EQ(trust_level, BPKUR::CHROME_BROWSER_OS_KEY);
  EXPECT_FALSE(wrapped.empty());
  EXPECT_EQ(wrapped, ParseKeyWrapped(kValidKeyWrappedBase64));
}

// Tests loading a valid TPM key from a key file.
TEST_F(LinuxKeyPersistenceDelegateTest, LoadKeyPair_ValidTPMKeyFile) {
  ASSERT_TRUE(CreateFile(kValidTPMKeyFileContent));
  auto [trust_level, wrapped] = persistence_delegate_.LoadKeyPair();
  EXPECT_EQ(trust_level, BPKUR::CHROME_BROWSER_TPM_KEY);
  EXPECT_FALSE(wrapped.empty());
  EXPECT_EQ(wrapped, ParseKeyWrapped(kValidKeyWrappedBase64));
}

// Tests loading a key from a key file with an invalid trust level.
TEST_F(LinuxKeyPersistenceDelegateTest, LoadKeyPair_InvalidTrustLevel) {
  ASSERT_TRUE(CreateFile(kInvalidTrustLevelKeyFileContent));
  auto [trust_level, wrapped] = persistence_delegate_.LoadKeyPair();
  EXPECT_EQ(trust_level, BPKUR::KEY_TRUST_LEVEL_UNSPECIFIED);
  EXPECT_TRUE(wrapped.empty());
}

// Tests loading a key from a key file when the signing key property is missing.
TEST_F(LinuxKeyPersistenceDelegateTest, LoadKeyPair_MissingSigningKey) {
  const char file_content[] = "{\"trustLevel\":\"2\"}";

  ASSERT_TRUE(CreateFile(file_content));
  auto [trust_level, wrapped] = persistence_delegate_.LoadKeyPair();
  EXPECT_EQ(trust_level, BPKUR::KEY_TRUST_LEVEL_UNSPECIFIED);
  EXPECT_TRUE(wrapped.empty());
}

// Tests loading a key from a key file when the trust level property is missing.
TEST_F(LinuxKeyPersistenceDelegateTest, LoadKeyPair_MissingTrustLevel) {
  const std::string file_content =
      base::StringPrintf("{\"signingKey\":\"%s\"}", kValidKeyWrappedBase64);

  ASSERT_TRUE(CreateFile(file_content));
  auto [trust_level, wrapped] = persistence_delegate_.LoadKeyPair();
  EXPECT_EQ(trust_level, BPKUR::KEY_TRUST_LEVEL_UNSPECIFIED);
  EXPECT_TRUE(wrapped.empty());
}

// Tests loading a key from a key file when the file content is invalid (not a
// JSON dictionary).
TEST_F(LinuxKeyPersistenceDelegateTest, LoadKeyPair_InvalidContent) {
  const char file_content[] = "just some text";

  ASSERT_TRUE(CreateFile(file_content));
  auto [trust_level, wrapped] = persistence_delegate_.LoadKeyPair();
  EXPECT_EQ(trust_level, BPKUR::KEY_TRUST_LEVEL_UNSPECIFIED);
  EXPECT_TRUE(wrapped.empty());
}

// Tests loading a key from a key file when there is a valid key, but the key
// file contains random trailing values.
TEST_F(LinuxKeyPersistenceDelegateTest, LoadKeyPair_TrailingGibberish) {
  const std::string file_content = base::StringPrintf(
      "{\"signingKey\":\"%s\",\"trustLevel\":\"2\"}someother random content",
      kValidKeyWrappedBase64);

  ASSERT_TRUE(CreateFile(file_content));
  auto [trust_level, wrapped] = persistence_delegate_.LoadKeyPair();
  EXPECT_EQ(trust_level, BPKUR::KEY_TRUST_LEVEL_UNSPECIFIED);
  EXPECT_TRUE(wrapped.empty());
}

// Tests loading a key from a key file when the key value is not a valid
// base64 encoded string.
TEST_F(LinuxKeyPersistenceDelegateTest, LoadKeyPair_KeyNotBase64) {
  const std::string file_content = base::StringPrintf(
      "{\"signingKey\":\"%s\",\"trustLevel\":\"2\"}", kInvalidBase64String);

  ASSERT_TRUE(CreateFile(file_content));
  auto [trust_level, wrapped] = persistence_delegate_.LoadKeyPair();
  EXPECT_EQ(trust_level, BPKUR::KEY_TRUST_LEVEL_UNSPECIFIED);
  EXPECT_TRUE(wrapped.empty());
}

// Tests the flow of both storing and loading a key.
TEST_F(LinuxKeyPersistenceDelegateTest, StoreAndLoadKeyPair) {
  ASSERT_TRUE(CreateFile(""));
  auto trust_level = BPKUR::CHROME_BROWSER_TPM_KEY;
  auto wrapped = ParseKeyWrapped(kValidKeyWrappedBase64);
  EXPECT_TRUE(persistence_delegate_.StoreKeyPair(trust_level, wrapped));

  auto [loaded_trust_level, loaded_wrapped] =
      persistence_delegate_.LoadKeyPair();
  EXPECT_EQ(trust_level, loaded_trust_level);
  EXPECT_EQ(wrapped, loaded_wrapped);
}

}  // namespace enterprise_connectors
