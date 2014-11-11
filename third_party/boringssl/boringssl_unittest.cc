// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdarg.h>

#include <string>

#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/strings/string_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

void TestProcess(const std::string& name,
                 const std::vector<base::CommandLine::StringType>& args) {
  base::FilePath exe_dir;
  ASSERT_TRUE(PathService::Get(base::DIR_EXE, &exe_dir));
  base::FilePath test_binary =
      exe_dir.AppendASCII("boringssl_" + name);
  base::CommandLine cmd(test_binary);

  for (size_t i = 0; i < args.size(); ++i) {
    cmd.AppendArgNative(args[i]);
  }

  std::string output;
  EXPECT_TRUE(base::GetAppOutput(cmd, &output));
  // Account for Windows line endings.
  ReplaceSubstringsAfterOffset(&output, 0, "\r\n", "\n");

  const bool ok = output.size() >= 5 &&
                  memcmp("PASS\n", &output[output.size() - 5], 5) == 0 &&
                  (output.size() == 5 || output[output.size() - 6] == '\n');

  EXPECT_TRUE(ok) << output;
}

void TestSimple(const std::string& name) {
  std::vector<base::CommandLine::StringType> empty;
  TestProcess(name, empty);
}

bool BoringSSLPath(base::FilePath* result) {
  if (!PathService::Get(base::DIR_SOURCE_ROOT, result))
    return false;

  *result = result->Append(FILE_PATH_LITERAL("third_party"));
  *result = result->Append(FILE_PATH_LITERAL("boringssl"));
  *result = result->Append(FILE_PATH_LITERAL("src"));
  return true;
}

bool CryptoCipherPath(base::FilePath *result) {
  if (!BoringSSLPath(result))
    return false;

  *result = result->Append(FILE_PATH_LITERAL("crypto"));
  *result = result->Append(FILE_PATH_LITERAL("cipher"));
  return true;
}

}  // anonymous namespace

TEST(BoringSSL, AES128GCM) {
  base::FilePath data_file;
  ASSERT_TRUE(CryptoCipherPath(&data_file));
  data_file = data_file.Append(FILE_PATH_LITERAL("aes_128_gcm_tests.txt"));

  std::vector<base::CommandLine::StringType> args;
  args.push_back(FILE_PATH_LITERAL("aes-128-gcm"));
  args.push_back(data_file.value());

  TestProcess("aead_test", args);
}

TEST(BoringSSL, AES256GCM) {
  base::FilePath data_file;
  ASSERT_TRUE(CryptoCipherPath(&data_file));
  data_file = data_file.Append(FILE_PATH_LITERAL("aes_256_gcm_tests.txt"));

  std::vector<base::CommandLine::StringType> args;
  args.push_back(FILE_PATH_LITERAL("aes-256-gcm"));
  args.push_back(data_file.value());

  TestProcess("aead_test", args);
}

TEST(BoringSSL, ChaCha20Poly1305) {
  base::FilePath data_file;
  ASSERT_TRUE(CryptoCipherPath(&data_file));
  data_file =
      data_file.Append(FILE_PATH_LITERAL("chacha20_poly1305_tests.txt"));

  std::vector<base::CommandLine::StringType> args;
  args.push_back(FILE_PATH_LITERAL("chacha20-poly1305"));
  args.push_back(data_file.value());

  TestProcess("aead_test", args);
}

TEST(BoringSSL, RC4MD5) {
  base::FilePath data_file;
  ASSERT_TRUE(CryptoCipherPath(&data_file));
  data_file = data_file.Append(FILE_PATH_LITERAL("rc4_md5_tests.txt"));

  std::vector<base::CommandLine::StringType> args;
  args.push_back(FILE_PATH_LITERAL("rc4-md5"));
  args.push_back(data_file.value());

  TestProcess("aead_test", args);
}

TEST(BoringSSL, AESKW128) {
  base::FilePath data_file;
  ASSERT_TRUE(CryptoCipherPath(&data_file));
  data_file = data_file.Append(FILE_PATH_LITERAL("aes_128_key_wrap_tests.txt"));

  std::vector<base::CommandLine::StringType> args;
  args.push_back(FILE_PATH_LITERAL("aes-128-key-wrap"));
  args.push_back(data_file.value());

  TestProcess("aead_test", args);
}

TEST(BoringSSL, AESKW256) {
  base::FilePath data_file;
  ASSERT_TRUE(CryptoCipherPath(&data_file));
  data_file = data_file.Append(FILE_PATH_LITERAL("aes_256_key_wrap_tests.txt"));

  std::vector<base::CommandLine::StringType> args;
  args.push_back(FILE_PATH_LITERAL("aes-256-key-wrap"));
  args.push_back(data_file.value());

  TestProcess("aead_test", args);
}

TEST(BoringSSL, Base64) {
  TestSimple("base64_test");
}

TEST(BoringSSL, BIO) {
  TestSimple("bio_test");
}

TEST(BoringSSL, BN) {
  TestSimple("bn_test");
}

TEST(BoringSSL, ByteString) {
  TestSimple("bytestring_test");
}

TEST(BoringSSL, ConstantTime) {
  TestSimple("constant_time_test");
}

TEST(BoringSSL, Cipher) {
  base::FilePath data_file;
  ASSERT_TRUE(CryptoCipherPath(&data_file));
  data_file = data_file.Append(FILE_PATH_LITERAL("cipher_test.txt"));

  std::vector<base::CommandLine::StringType> args;
  args.push_back(data_file.value());

  TestProcess("cipher_test", args);
}

TEST(BoringSSL, DH) {
  TestSimple("dh_test");
}

TEST(BoringSSL, Digest) {
  TestSimple("digest_test");
}

TEST(BoringSSL, DSA) {
  TestSimple("dsa_test");
}

TEST(BoringSSL, EC) {
  TestSimple("ec_test");
}

TEST(BoringSSL, ECDSA) {
  TestSimple("ecdsa_test");
}

TEST(BoringSSL, ERR) {
  TestSimple("err_test");
}

TEST(BoringSSL, GCM) {
  TestSimple("gcm_test");
}

TEST(BoringSSL, HMAC) {
  TestSimple("hmac_test");
}

TEST(BoringSSL, LH) {
  TestSimple("lhash_test");
}

TEST(BoringSSL, RSA) {
  TestSimple("rsa_test");
}

TEST(BoringSSL, PKCS7) {
  TestSimple("pkcs7_test");
}

TEST(BoringSSL, PKCS12) {
  TestSimple("pkcs12_test");
}

TEST(BoringSSL, ExampleMul) {
  TestSimple("example_mul");
}

TEST(BoringSSL, EVP) {
  TestSimple("evp_test");
}

TEST(BoringSSL, SSL) {
  TestSimple("ssl_test");
}

TEST(BoringSSL, PQueue) {
  TestSimple("pqueue_test");
}
