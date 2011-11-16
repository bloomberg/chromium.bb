// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/tests/test_net_address_private.h"

#include "ppapi/cpp/private/net_address_private.h"
#include "ppapi/c/private/ppb_net_address_private.h"
#include "ppapi/tests/testing_instance.h"

// Other than |GetAnyAddress()|, there's no way to actually get
// |PP_NetAddress_Private| structs from just this interface. We'll cheat and
// synthesize some.
// TODO(viettrungluu): This is very fragile and implementation-dependent. :(
#if defined(_WIN32)
#define OS_WIN
#elif defined(__linux__) || defined(__APPLE__) || defined(__FreeBSD__) || \
      defined(__OpenBSD__) || defined(__sun) || defined(__native_client__)
#define OS_POSIX
#else
#error "Unsupported platform."
#endif

#if defined(OS_WIN)
#include <ws2tcpip.h>
#elif defined(OS_POSIX)
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#endif

using pp::NetAddressPrivate;

namespace {

PP_NetAddress_Private MakeIPv4NetAddress(const char* host, int port) {
  PP_NetAddress_Private addr = PP_NetAddress_Private();
  addr.size = sizeof(sockaddr_in);
  sockaddr_in* a = reinterpret_cast<sockaddr_in*>(addr.data);
  a->sin_family = AF_INET;
  a->sin_port = htons(port);
  a->sin_addr.s_addr = inet_addr(host);
  return addr;
}

// TODO(viettrungluu): Also add IPv6 tests.

}  // namespace

REGISTER_TEST_CASE(NetAddressPrivate);

TestNetAddressPrivate::TestNetAddressPrivate(TestingInstance* instance)
    : TestCase(instance) {
}

bool TestNetAddressPrivate::Init() {
  return NetAddressPrivate::IsAvailable();
}

void TestNetAddressPrivate::RunTests(const std::string& filter) {
  RUN_TEST(AreEqual, filter);
  RUN_TEST(AreHostsEqual, filter);
  RUN_TEST(Describe, filter);
  RUN_TEST(ReplacePort, filter);
  RUN_TEST(GetAnyAddress, filter);
}

std::string TestNetAddressPrivate::TestAreEqual() {
  // No comparisons should ever be done with invalid addresses.
  PP_NetAddress_Private invalid = PP_NetAddress_Private();
  ASSERT_FALSE(NetAddressPrivate::AreEqual(invalid, invalid));

  PP_NetAddress_Private localhost_80 = MakeIPv4NetAddress("127.0.0.1", 80);
  ASSERT_TRUE(NetAddressPrivate::AreEqual(localhost_80, localhost_80));
  ASSERT_FALSE(NetAddressPrivate::AreEqual(localhost_80, invalid));

  PP_NetAddress_Private localhost_1234 = MakeIPv4NetAddress("127.0.0.1", 1234);
  ASSERT_FALSE(NetAddressPrivate::AreEqual(localhost_80, localhost_1234));

  PP_NetAddress_Private other_80 = MakeIPv4NetAddress("192.168.0.1", 80);
  ASSERT_FALSE(NetAddressPrivate::AreEqual(localhost_80, other_80));

  PASS();
}

std::string TestNetAddressPrivate::TestAreHostsEqual() {
  // No comparisons should ever be done with invalid addresses.
  PP_NetAddress_Private invalid = PP_NetAddress_Private();
  ASSERT_FALSE(NetAddressPrivate::AreHostsEqual(invalid, invalid));

  PP_NetAddress_Private localhost_80 = MakeIPv4NetAddress("127.0.0.1", 80);
  ASSERT_TRUE(NetAddressPrivate::AreHostsEqual(localhost_80, localhost_80));
  ASSERT_FALSE(NetAddressPrivate::AreHostsEqual(localhost_80, invalid));

  PP_NetAddress_Private localhost_1234 = MakeIPv4NetAddress("127.0.0.1", 1234);
  ASSERT_TRUE(NetAddressPrivate::AreHostsEqual(localhost_80, localhost_1234));

  PP_NetAddress_Private other_80 = MakeIPv4NetAddress("192.168.0.1", 80);
  ASSERT_FALSE(NetAddressPrivate::AreHostsEqual(localhost_80, other_80));

  PASS();
}

std::string TestNetAddressPrivate::TestDescribe() {
  PP_NetAddress_Private invalid = PP_NetAddress_Private();
  ASSERT_EQ("", NetAddressPrivate::Describe(invalid, false));
  ASSERT_EQ("", NetAddressPrivate::Describe(invalid, true));

  PP_NetAddress_Private localhost_80 = MakeIPv4NetAddress("127.0.0.1", 80);
  ASSERT_EQ("127.0.0.1", NetAddressPrivate::Describe(localhost_80, false));
  ASSERT_EQ("127.0.0.1:80", NetAddressPrivate::Describe(localhost_80, true));

  PP_NetAddress_Private localhost_1234 = MakeIPv4NetAddress("127.0.0.1", 1234);
  ASSERT_EQ("127.0.0.1", NetAddressPrivate::Describe(localhost_1234, false));
  ASSERT_EQ("127.0.0.1:1234", NetAddressPrivate::Describe(localhost_1234,
                                                          true));

  PP_NetAddress_Private other_80 = MakeIPv4NetAddress("192.168.0.1", 80);
  ASSERT_EQ("192.168.0.1", NetAddressPrivate::Describe(other_80, false));
  ASSERT_EQ("192.168.0.1:80", NetAddressPrivate::Describe(other_80, true));

  PASS();
}

std::string TestNetAddressPrivate::TestReplacePort() {
  // Assume that |AreEqual()| works correctly.
  PP_NetAddress_Private result = PP_NetAddress_Private();

  PP_NetAddress_Private invalid = PP_NetAddress_Private();
  ASSERT_FALSE(NetAddressPrivate::ReplacePort(invalid, 1234, &result));

  PP_NetAddress_Private localhost_80 = MakeIPv4NetAddress("127.0.0.1", 80);
  ASSERT_TRUE(NetAddressPrivate::ReplacePort(localhost_80, 1234, &result));
  PP_NetAddress_Private localhost_1234 = MakeIPv4NetAddress("127.0.0.1", 1234);
  ASSERT_TRUE(NetAddressPrivate::AreEqual(result, localhost_1234));

  // Test that having the out param being the same as the in param works
  // properly.
  ASSERT_TRUE(NetAddressPrivate::ReplacePort(result, 80, &result));
  ASSERT_TRUE(NetAddressPrivate::AreEqual(result, localhost_80));

  PASS();
}

std::string TestNetAddressPrivate::TestGetAnyAddress() {
  // Just make sure it doesn't crash and such.
  PP_NetAddress_Private result = PP_NetAddress_Private();

  NetAddressPrivate::GetAnyAddress(false, &result);
  ASSERT_TRUE(NetAddressPrivate::AreEqual(result, result));

  NetAddressPrivate::GetAnyAddress(true, &result);
  ASSERT_TRUE(NetAddressPrivate::AreEqual(result, result));

  PASS();
}
