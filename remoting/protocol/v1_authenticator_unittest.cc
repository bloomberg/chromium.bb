// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/v1_authenticator.h"

#include "base/bind.h"
#include "net/base/net_errors.h"
#include "remoting/protocol/authenticator.h"
#include "remoting/protocol/authenticator_test_base.h"
#include "remoting/protocol/channel_authenticator.h"
#include "remoting/protocol/connection_tester.h"
#include "remoting/protocol/fake_session.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libjingle/source/talk/xmllite/xmlelement.h"

using testing::_;
using testing::DeleteArg;
using testing::Mock;
using testing::SaveArg;

namespace remoting {
namespace protocol {

namespace {

const int kMessageSize = 100;
const int kMessages = 1;

const char kClientJid[] = "host2@gmail.com/321";

const char kTestSharedSecret[] = "1234-1234-5678";
const char kTestSharedSecretBad[] = "0000-0000-0001";

}  // namespace

class V1AuthenticatorTest : public AuthenticatorTestBase {
 public:
  V1AuthenticatorTest() {
  }
  virtual ~V1AuthenticatorTest() {
  }

 protected:
  void InitAuthenticators(const std::string& client_secret,
                          const std::string& host_secret) {
    host_.reset(new V1HostAuthenticator(
        host_cert_, *private_key_, host_secret, kClientJid));
    client_.reset(new V1ClientAuthenticator(kClientJid, client_secret));
  }

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

  client_auth_ = client_->CreateChannelAuthenticator();
  host_auth_ = host_->CreateChannelAuthenticator();
  RunChannelAuth(false);

  EXPECT_TRUE(client_socket_.get() != NULL);
  EXPECT_TRUE(host_socket_.get() != NULL);

  StreamConnectionTester tester(host_socket_.get(), client_socket_.get(),
                                kMessageSize, kMessages);

  tester.Start();
  message_loop_.Run();
  tester.CheckResults();
}

// Verify that connection is rejected when secrets don't match.
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
