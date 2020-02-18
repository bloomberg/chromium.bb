// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/impl/ssl_context.h"

#include <stdio.h>
#include <stdlib.h>

#include <cstdio>
#include <string>

#include "absl/strings/string_view.h"
#include "gtest/gtest.h"
#include "platform/base/error.h"
#include "util/crypto/openssl_util.h"

namespace openscreen {
namespace platform {
namespace {

const char kTestCertificateBody[] = R"(
-----BEGIN CERTIFICATE-----
MIIC5DCCAcwCCQDi2o86WhqLRTANBgkqhkiG9w0BAQUFADA0MQswCQYDVQQGEwJV
UzETMBEGA1UECAwKV2FzaGluZ3RvbjEQMA4GA1UEBwwHU2VhdHRsZTAeFw0xOTA2
MDQyMjE3MzdaFw0yMDA2MDMyMjE3MzdaMDQxCzAJBgNVBAYTAlVTMRMwEQYDVQQI
DApXYXNoaW5ndG9uMRAwDgYDVQQHDAdTZWF0dGxlMIIBIjANBgkqhkiG9w0BAQEF
AAOCAQ8AMIIBCgKCAQEAu/EK2OjDBuBnJ8BEOV6gOStYywx9PV2rZohGv2XIjIN2
Q8WUP1vS20TidGDz+Hkwd/eG/9Kttb34vbuANzSJTSP2LSRoDGb5oi1bGQU4CbUh
VSpI64Iz/xoSihQIcPZQqUhQ4xD9/N8O1VRvAmc21Dt+qWDEuzP4bFbCgK/Dzls8
bYcVaixyErmLSH0wFlMum0rZAK1PKMlZXed7RVAaoS1yarIeOxaLrSJHtAzxi+xR
nH3+sjiQQg0FM0laz7Cib7DTQui8EG/ddVJRViJoPr2rXmn0bjqXQ3qXe4EtnaWt
0XMipQBAUgQtTI8W+mrfUJwWCFDLJWlR4SR+WvE3LQIDAQABMA0GCSqGSIb3DQEB
BQUAA4IBAQAqZEEVWDVfRGcZZVkO9HslraHrXf+e60Hgj1p8sgAsZ2ghUlJhPzNk
Y8qXnApzdxI3zsI2naVOVpFhe7Deu74gwM5s2AtHw0smTm07TzGqwp9/emOByJNZ
M4Lco+YrgOBQAcKuL4vZXIHl9bNFv8IhVUBtjF3DUo/eG9ENb+ezKD5iheOKFylX
Zl+B2KLs2gB31d+11rj2njBVI0MAgNT2u8hw3AE2NzcL736lA868ofqs6xC0Snn8
F6hTKnPKdM8PEC04jZBT6EIEsqBEzhYxVZIt95l0dFI+dNkU1YbkOi+BsR8my4W1
x+vTfK9RnTtXXPDNYZBpIleno6KTzoas
-----END CERTIFICATE-----
)";

const char kTestKeyBody[] = R"(
-----BEGIN PRIVATE KEY-----
MIIEwAIBADANBgkqhkiG9w0BAQEFAASCBKowggSmAgEAAoIBAQC78QrY6MMG4Gcn
wEQ5XqA5K1jLDH09XatmiEa/ZciMg3ZDxZQ/W9LbROJ0YPP4eTB394b/0q21vfi9
u4A3NIlNI/YtJGgMZvmiLVsZBTgJtSFVKkjrgjP/GhKKFAhw9lCpSFDjEP383w7V
VG8CZzbUO36pYMS7M/hsVsKAr8POWzxthxVqLHISuYtIfTAWUy6bStkArU8oyVld
53tFUBqhLXJqsh47FoutIke0DPGL7FGcff6yOJBCDQUzSVrPsKJvsNNC6LwQb911
UlFWImg+vateafRuOpdDepd7gS2dpa3RcyKlAEBSBC1Mjxb6at9QnBYIUMslaVHh
JH5a8TctAgMBAAECggEBAJ0a4fZwnJci/xg7oMxUTZt8oL0bs5WYt67+PCXC7+KG
Alak00gjeh/RdXvTkB4lMF8Yi8FOW+eQ5l20X3nGcQALD76ssE2txv/K6lwAANgc
kcCfmFVGgjC9msHR/TxwqvzXdsZZbff0fnHWIvXfUfYXxcnRGqNizkfCPtVUGFpm
evakogMaH4yfld4fgfs5MNOWu961Fd46kM8nNZTf9u+GSIYsxzywV7tO3TK+JBdf
lvNYUMyZYdiIsKq1xxVL0lxZRW2ocU8kQfQPOmsetfTQ0AVXjS1dxuCVYPkZMMtH
Jb3Poy6KnuzjXYyyFdlgp4mRbz1NA/rU7q5Oc+VcKAECgYEA5jlt2KW5dkvxmmsc
YZ3nktlBAhbtMF2QsfWvKrjx9PMe4oNJ6vN13P4hHnU7tbFW3lZRWSfyNmyQ0xUu
moa1JpB+FqU+x2T8Gj9x30yKiipdVh5a+wJUatOnP2N0cmhyCTh1BDpJxCJma/P/
76ClxEJRc68BaIzI3ZwLcwejmRsCgYEA0Pu6BPSd4xJy2FojWRyhO/9Pv6KkqN8u
kKH6Fj+00isUSBHsn6WS1HYAcs7WzzFLqeoj9FInB/SwMK5fmkCFKu+98YSgij3k
urkWD19OvbB08ifyxo6wZZDjyRKxnqBOB906rMfOpX8oLHxfvW2NeamKEdTXdDRS
zWBTgFEifVcCgYEAplXbzWl/I78e4gh9SvIBPBIHa/EQkZ8oSctOMbnJ5sY5DEL7
Buu7vl1FpHHjIBTuxFjtEVNRJo0t3bQyJacp/qDq2IWvY/TMSFKKfWEZv1V4dh4c
cbpvL3eYIK5Eldxsd8j4koNihHiuM1TpF0KkQbYAj2pjxBvjjGmPxB5Czv0CgYEA
gladW/3cgwS8j+cX6LoMbUj8yf58R756YkZDnaC5++JWUeSr+Z3gh5XZDcGnA64x
DJY1OmoKYdHgGVyHz8Kxy2eexYT8Q3v6reJFueytRW1KYsU7i07vAUluZ7e7A73j
LNBzidNMNLerrKMdax1qgRiWPizK+3Sut9x996eipR0CgYEAqmk3EYdlCBuJqFhU
1uNBfLXE58dVoqF0Fxr709hQFabi7E+m79q/Hyp83krFOvNG8I8ikoVjG8K7daPC
fdJ+sBFMfWlq42aP74oTOuX2vB7LihYcPgV3f0ULDkyP36EsGThrcKVeS/WZM+Ow
+KbgjSwXLaj99BV2JetebRQ4pL0=
-----END PRIVATE KEY-----
)";

class TemporaryFile {
 public:
  TemporaryFile() {
    char file_name[] = "tmp_XXXXXX";
    fd_ = mkstemp(file_name);
    file_name_ = std::string(file_name);
  }

  ~TemporaryFile() {
    close(fd_);
    remove(file_name_.c_str());
  }

  void Put(const char* data) {
    const size_t length = strlen(data);
    EXPECT_EQ(write(fd_, data, length), static_cast<ssize_t>(length));
  }

  const std::string& get_file_name() { return file_name_; }

 private:
  std::string file_name_;
  int fd_;
};

class SSLContextTest : public ::testing::Test {
 protected:
  void SetUp() override { EnsureOpenSSLInit(); }

  void TearDown() override { EnsureOpenSSLCleanup(); }
};
}  // namespace

TEST_F(SSLContextTest, InitFailsWithInvalidCertificateOrKey) {
  TemporaryFile cert;
  TemporaryFile key;

  auto context(SSLContext::Create(nullptr, nullptr));
  ASSERT_FALSE(context);

  context = SSLContext::Create(cert.get_file_name(), nullptr);
  ASSERT_FALSE(context);

  context = SSLContext::Create(nullptr, key.get_file_name());
  ASSERT_FALSE(context);

  context = SSLContext::Create(cert.get_file_name(), key.get_file_name());
  ASSERT_FALSE(context);
}

TEST_F(SSLContextTest, InitSucceedsWithValidCertAndKey) {
  TemporaryFile cert;
  TemporaryFile key;

  cert.Put(kTestCertificateBody);
  key.Put(kTestKeyBody);

  auto context = SSLContext::Create(cert.get_file_name(), key.get_file_name());
  ASSERT_TRUE(context);
}

TEST_F(SSLContextTest, CanCreateSSLPtr) {
  TemporaryFile cert;
  TemporaryFile key;

  cert.Put(kTestCertificateBody);
  key.Put(kTestKeyBody);

  auto context = SSLContext::Create(cert.get_file_name(), key.get_file_name());
  ASSERT_TRUE(context);
  auto ssl_ptr = context.value().CreateSSL();
  ASSERT_TRUE(ssl_ptr);
}

TEST_F(SSLContextTest, CanCreateMultipleContexts) {
  TemporaryFile cert;
  TemporaryFile key;

  cert.Put(kTestCertificateBody);
  key.Put(kTestKeyBody);

  ErrorOr<SSLContext> context =
      SSLContext::Create(cert.get_file_name(), key.get_file_name());
  ErrorOr<SSLContext> context2 =
      SSLContext::Create(cert.get_file_name(), key.get_file_name());
  ErrorOr<SSLContext> context3 =
      SSLContext::Create(cert.get_file_name(), key.get_file_name());

  ErrorOr<int> foo = ErrorOr<int>(Error::Code::kFileLoadFailure);

  ASSERT_TRUE(context);
  ASSERT_TRUE(context2);
  ASSERT_TRUE(context3);
}
}  // namespace platform
}  // namespace openscreen
