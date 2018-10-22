// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/nearby/hash_utils_impl.h"

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "chromeos/components/nearby/library/hash_utils.h"
#include "crypto/sha2.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace nearby {

class HashUtilsImplTest : public testing::Test {
 protected:
  HashUtilsImplTest() = default;

  // testing::Test:
  void SetUp() override { hash_utils_ = std::make_unique<HashUtilsImpl>(); }

  std::string ComputeMd5Hash(const std::string& input) {
    std::unique_ptr<location::nearby::ByteArray> md5_result =
        hash_utils_->md5(input);
    return std::string(md5_result->getData(), md5_result->size());
  }

  std::string ComputeSha256Hash(const std::string& input) {
    std::unique_ptr<location::nearby::ByteArray> sha256_result =
        hash_utils_->sha256(input);
    return std::string(sha256_result->getData(), sha256_result->size());
  }

  std::unique_ptr<location::nearby::HashUtils> hash_utils_;

 private:
  DISALLOW_COPY_AND_ASSIGN(HashUtilsImplTest);
};

// MD5 example data from http://www.ietf.org/rfc/rfc1321.txt A.5 Test Suite
TEST_F(HashUtilsImplTest, MD5_EmptyString) {
  EXPECT_EQ("d41d8cd98f00b204e9800998ecf8427e", ComputeMd5Hash(""));
}

TEST_F(HashUtilsImplTest, MD5_SingleCharacterString) {
  EXPECT_EQ("0cc175b9c0f1b6a831c399e269772661", ComputeMd5Hash("a"));
}

TEST_F(HashUtilsImplTest, MD5_ShortString) {
  EXPECT_EQ("900150983cd24fb0d6963f7d28e17f72", ComputeMd5Hash("abc"));
}

TEST_F(HashUtilsImplTest, MD5_StringWithSpace) {
  EXPECT_EQ("f96b697d7cb7938d525a2f31aaf161d0",
            ComputeMd5Hash("message digest"));
}

TEST_F(HashUtilsImplTest, MD5_AlphaString) {
  EXPECT_EQ("c3fcd3d76192e4007dfb496cca67e13b",
            ComputeMd5Hash("abcdefghijklmnopqrstuvwxyz"));
}

TEST_F(HashUtilsImplTest, MD5_AlphaNumericStringWithUpperLowerCase) {
  EXPECT_EQ("d174ab98d277d9f5a5611c2c9f419d9f",
            ComputeMd5Hash("ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                           "abcdefghijklmnopqrstuvwxyz"
                           "0123456789"));
}

TEST_F(HashUtilsImplTest, MD5_NumbersString) {
  EXPECT_EQ("57edf4a22be3c955ac49da2e2107b67a",
            ComputeMd5Hash("12345678901234567890"
                           "12345678901234567890"
                           "12345678901234567890"
                           "12345678901234567890"));
}

TEST_F(HashUtilsImplTest, SHA256_ShortString) {
  // Example B.1 from FIPS 180-2: one-block message.
  static const uint8_t expected[] = {
      0xba, 0x78, 0x16, 0xbf, 0x8f, 0x01, 0xcf, 0xea, 0x41, 0x41, 0x40,
      0xde, 0x5d, 0xae, 0x22, 0x23, 0xb0, 0x03, 0x61, 0xa3, 0x96, 0x17,
      0x7a, 0x9c, 0xb4, 0x10, 0xff, 0x61, 0xf2, 0x00, 0x15, 0xad};

  std::string output = ComputeSha256Hash("abc");
  ASSERT_EQ(crypto::kSHA256Length, output.size());
  for (size_t i = 0; i < crypto::kSHA256Length; i++)
    EXPECT_EQ(expected[i], static_cast<uint8_t>(output[i]));
}

TEST_F(HashUtilsImplTest, SHA256_MediumString) {
  // Example B.2 from FIPS 180-2: multi-block message.
  static const uint8_t expected[] = {
      0x24, 0x8d, 0x6a, 0x61, 0xd2, 0x06, 0x38, 0xb8, 0xe5, 0xc0, 0x26,
      0x93, 0x0c, 0x3e, 0x60, 0x39, 0xa3, 0x3c, 0xe4, 0x59, 0x64, 0xff,
      0x21, 0x67, 0xf6, 0xec, 0xed, 0xd4, 0x19, 0xdb, 0x06, 0xc1};

  std::string output = ComputeSha256Hash(
      "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq");
  ASSERT_EQ(crypto::kSHA256Length, output.size());
  for (size_t i = 0; i < crypto::kSHA256Length; i++)
    EXPECT_EQ(expected[i], static_cast<uint8_t>(output[i]));
}

TEST_F(HashUtilsImplTest, SHA256_MillionCharacterString) {
  // Example B.3 from FIPS 180-2: long message.
  static const uint8_t expected[] = {
      0xcd, 0xc7, 0x6e, 0x5c, 0x99, 0x14, 0xfb, 0x92, 0x81, 0xa1, 0xc7,
      0xe2, 0x84, 0xd7, 0x3e, 0x67, 0xf1, 0x80, 0x9a, 0x48, 0xa4, 0x97,
      0x20, 0x0e, 0x04, 0x6d, 0x39, 0xcc, 0xc7, 0x11, 0x2c, 0xd0};

  std::string output = ComputeSha256Hash(std::string(1000000, 'a'));
  ASSERT_EQ(crypto::kSHA256Length, output.size());
  for (size_t i = 0; i < crypto::kSHA256Length; i++)
    EXPECT_EQ(expected[i], static_cast<uint8_t>(output[i]));
}

}  // namespace nearby

}  // namespace chromeos
