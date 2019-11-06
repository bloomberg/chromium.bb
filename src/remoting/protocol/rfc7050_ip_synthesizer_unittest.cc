// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/rfc7050_ip_synthesizer.h"

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/test/scoped_task_environment.h"
#include "net/base/sys_addrinfo.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Invoke;
using testing::Return;

namespace {

const rtc::SocketAddress kTestIpv4Address("172.217.3.206", 1234);
const rtc::SocketAddress kTestIpv6Address("2607:f8b0:400a:809::200e", 1234);

// 64:ff9b::172.217.3.206
const rtc::SocketAddress kTestSynthesizedIpv6Address("64:ff9b::acd9:3ce", 1234);

rtc::IPAddress StringToIp(const std::string& ip_string) {
  rtc::IPAddress ip;
  EXPECT_TRUE(rtc::IPFromString(ip_string, &ip));
  return ip;
}

const rtc::IPAddress kDns64IpPrefix = StringToIp("64:ff9b::");
const rtc::IPAddress kResolvedIpv4HostIp = StringToIp("192.0.0.170");
const rtc::IPAddress kResolvedIpv4MappedHostIp =
    kResolvedIpv4HostIp.AsIPv6Address();

// 64:ff9b::192.0.0.170
const rtc::IPAddress kResolvedSynthesizedHostIp =
    StringToIp("64:ff9b::c000:aa");

}  // namespace

namespace remoting {
namespace protocol {

class Rfc7050IpSynthesizerTest : public testing::Test {
 public:
  void SetUp() override;
  void TearDown() override;

 protected:
  void ExpectHostResolvedToIp(const rtc::IPAddress& ip);

  MOCK_METHOD1(ResolveIpv4OnlyHost, int(rtc::IPAddress*));
  std::unique_ptr<Rfc7050IpSynthesizer> ip_synthesizer_;
  std::unique_ptr<base::RunLoop> run_loop_;

 private:
  base::test::ScopedTaskEnvironment scoped_task_environment_;
};

void Rfc7050IpSynthesizerTest::SetUp() {
  run_loop_ = std::make_unique<base::RunLoop>();
  ip_synthesizer_.reset(new Rfc7050IpSynthesizer());
  ip_synthesizer_->resolve_ipv4_only_host_ = base::BindRepeating(
      &Rfc7050IpSynthesizerTest::ResolveIpv4OnlyHost, base::Unretained(this));
}

void Rfc7050IpSynthesizerTest::TearDown() {
  ip_synthesizer_.reset();
  run_loop_.reset();
}

void Rfc7050IpSynthesizerTest::ExpectHostResolvedToIp(
    const rtc::IPAddress& ip) {
  EXPECT_CALL(*this, ResolveIpv4OnlyHost(_))
      .WillOnce(Invoke([ip](rtc::IPAddress* out_ip) {
        *out_ip = ip;
        return 0;
      }));
}

TEST_F(Rfc7050IpSynthesizerTest, GetNativeSocketWithoutUpdateDns64Prefix) {
  EXPECT_EQ(kTestIpv4Address,
            ip_synthesizer_->ToNativeSocket(kTestIpv4Address));
  EXPECT_EQ(kTestIpv6Address,
            ip_synthesizer_->ToNativeSocket(kTestIpv6Address));
}

TEST_F(Rfc7050IpSynthesizerTest, HostFailedToResolve_NoName) {
  EXPECT_CALL(*this, ResolveIpv4OnlyHost(_)).WillOnce(Return(EAI_NONAME));
  ip_synthesizer_->UpdateDns64Prefix(run_loop_->QuitClosure());
  run_loop_->Run();

  EXPECT_EQ(kTestIpv4Address,
            ip_synthesizer_->ToNativeSocket(kTestIpv4Address));
  EXPECT_EQ(kTestIpv6Address,
            ip_synthesizer_->ToNativeSocket(kTestIpv6Address));
}

TEST_F(Rfc7050IpSynthesizerTest, HostFailedToResolve_OtherError) {
  EXPECT_CALL(*this, ResolveIpv4OnlyHost(_)).WillOnce(Return(EAI_FAIL));
  ip_synthesizer_->UpdateDns64Prefix(run_loop_->QuitClosure());
  run_loop_->Run();

  EXPECT_EQ(kTestIpv4Address,
            ip_synthesizer_->ToNativeSocket(kTestIpv4Address));
  EXPECT_EQ(kTestIpv6Address,
            ip_synthesizer_->ToNativeSocket(kTestIpv6Address));
}

TEST_F(Rfc7050IpSynthesizerTest, HostResolvedToIpv4) {
  ExpectHostResolvedToIp(kResolvedIpv4HostIp);
  ip_synthesizer_->UpdateDns64Prefix(run_loop_->QuitClosure());
  run_loop_->Run();

  EXPECT_EQ(kTestIpv4Address,
            ip_synthesizer_->ToNativeSocket(kTestIpv4Address));
  EXPECT_EQ(kTestIpv6Address,
            ip_synthesizer_->ToNativeSocket(kTestIpv6Address));
}

TEST_F(Rfc7050IpSynthesizerTest, HostResolvedToIpv4MappedIpv6) {
  ExpectHostResolvedToIp(kResolvedIpv4MappedHostIp);
  ip_synthesizer_->UpdateDns64Prefix(run_loop_->QuitClosure());
  run_loop_->Run();

  EXPECT_EQ(kTestIpv4Address,
            ip_synthesizer_->ToNativeSocket(kTestIpv4Address));
  EXPECT_EQ(kTestIpv6Address,
            ip_synthesizer_->ToNativeSocket(kTestIpv6Address));
}

TEST_F(Rfc7050IpSynthesizerTest, HostResolvedToSynthesizedIpv6) {
  ExpectHostResolvedToIp(kResolvedSynthesizedHostIp);
  ip_synthesizer_->UpdateDns64Prefix(run_loop_->QuitClosure());
  run_loop_->Run();

  EXPECT_EQ(kTestSynthesizedIpv6Address,
            ip_synthesizer_->ToNativeSocket(kTestIpv4Address));
  EXPECT_EQ(kTestIpv6Address,
            ip_synthesizer_->ToNativeSocket(kTestIpv6Address));
}

}  // namespace protocol
}  // namespace remoting
