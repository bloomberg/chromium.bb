// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "crypto/rsa_private_key.h"
#include "remoting/protocol/v1_authenticator.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libjingle/source/talk/xmllite/xmlelement.h"

namespace remoting {
namespace protocol {

namespace {
const char kHostJid[] = "host1@gmail.com/123";
const char kClientJid[] = "host2@gmail.com/321";

const char kTestSharedSecret[] = "1234-1234-5678";
const char kTestSharedSecretBad[] = "0000-0000-0001";
}  // namespace

class V1AuthenticatorTest : public testing::Test {
 public:
  V1AuthenticatorTest() {
  }
  virtual ~V1AuthenticatorTest() {
  }

 protected:
  void InitAuthenticators(const std::string& client_secret,
                          const std::string& host_secret) {
    FilePath certs_dir;
    PathService::Get(base::DIR_SOURCE_ROOT, &certs_dir);
    certs_dir = certs_dir.AppendASCII("net");
    certs_dir = certs_dir.AppendASCII("data");
    certs_dir = certs_dir.AppendASCII("ssl");
    certs_dir = certs_dir.AppendASCII("certificates");

    FilePath cert_path = certs_dir.AppendASCII("unittest.selfsigned.der");
    std::string cert_der;
    ASSERT_TRUE(file_util::ReadFileToString(cert_path, &cert_der));

    FilePath key_path = certs_dir.AppendASCII("unittest.key.bin");
    std::string key_string;
    ASSERT_TRUE(file_util::ReadFileToString(key_path, &key_string));
    std::vector<uint8> key_vector(
        reinterpret_cast<const uint8*>(key_string.data()),
        reinterpret_cast<const uint8*>(key_string.data() +
                                       key_string.length()));
    private_key_.reset(
        crypto::RSAPrivateKey::CreateFromPrivateKeyInfo(key_vector));

    host_.reset(new V1HostAuthenticator(
        cert_der, private_key_.get(), host_secret, kClientJid));
    client_.reset(new V1ClientAuthenticator(kClientJid, client_secret));
  }

  void RunAuthExchange() {
    do {
      scoped_ptr<buzz::XmlElement> message;

      // Pass message from client to host.
      ASSERT_EQ(Authenticator::MESSAGE_READY, client_->state());
      message.reset(client_->GetNextMessage());
      ASSERT_TRUE(message.get());
      ASSERT_NE(Authenticator::MESSAGE_READY, client_->state());

      ASSERT_EQ(Authenticator::WAITING_MESSAGE, host_->state());
      host_->ProcessMessage(message.get());
      ASSERT_NE(Authenticator::WAITING_MESSAGE, host_->state());

      // Are we done yet?
      if (host_->state() == Authenticator::ACCEPTED ||
          host_->state() == Authenticator::REJECTED) {
        break;
      }

      // Pass message from host to client.
      ASSERT_EQ(Authenticator::MESSAGE_READY, host_->state());
      message.reset(host_->GetNextMessage());
      ASSERT_TRUE(message.get());
      ASSERT_NE(Authenticator::MESSAGE_READY, host_->state());

      ASSERT_EQ(Authenticator::WAITING_MESSAGE, client_->state());
      client_->ProcessMessage(message.get());
      ASSERT_NE(Authenticator::WAITING_MESSAGE, client_->state());
    } while (host_->state() != Authenticator::ACCEPTED &&
             host_->state() != Authenticator::REJECTED);
  }

  scoped_ptr<crypto::RSAPrivateKey> private_key_;
  scoped_ptr<V1HostAuthenticator> host_;
  scoped_ptr<V1ClientAuthenticator> client_;

  DISALLOW_COPY_AND_ASSIGN(V1AuthenticatorTest);
};

TEST_F(V1AuthenticatorTest, SuccessfulAuth) {
  {
    SCOPED_TRACE("RunAuthExchange");
    InitAuthenticators(kTestSharedSecret, kTestSharedSecret);
    RunAuthExchange();
  }
  ASSERT_EQ(Authenticator::ACCEPTED, host_->state());
  ASSERT_EQ(Authenticator::ACCEPTED, client_->state());
}

TEST_F(V1AuthenticatorTest, InvalidSecret) {
  {
    SCOPED_TRACE("RunAuthExchange");
    InitAuthenticators(kTestSharedSecretBad, kTestSharedSecret);
    RunAuthExchange();
  }
  ASSERT_EQ(Authenticator::REJECTED, host_->state());
}

}  // namespace protocol
}  // namespace remoting
