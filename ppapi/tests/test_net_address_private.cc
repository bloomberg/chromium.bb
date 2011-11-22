// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/tests/test_net_address_private.h"

#include "base/basictypes.h"
#include "build/build_config.h"
#include "ppapi/cpp/private/net_address_private.h"
#include "ppapi/c/private/ppb_net_address_private.h"
#include "ppapi/tests/testing_instance.h"

// Other than |GetAnyAddress()|, there's no way to actually get
// |PP_NetAddress_Private| structs from just this interface. We'll cheat and
// synthesize some.

#if defined(OS_POSIX)
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#endif

#if defined(OS_MACOSX)
// This is a bit evil, but it's standard operating procedure for |s6_addr|....
#define s6_addr16 __u6_addr.__u6_addr16
#endif

#if defined(OS_WIN)
#include <ws2tcpip.h>

#define s6_addr16 u.Word
#endif

using pp::NetAddressPrivate;

namespace {

// |host| should be an IP address represented as text, e.g., "192.168.0.1".
PP_NetAddress_Private MakeIPv4NetAddress(const char* host, int port) {
  PP_NetAddress_Private addr = PP_NetAddress_Private();
  addr.size = sizeof(sockaddr_in);
  sockaddr_in* a = reinterpret_cast<sockaddr_in*>(addr.data);
  a->sin_family = AF_INET;
  a->sin_port = htons(port);
  a->sin_addr.s_addr = inet_addr(host);
  return addr;
}

// |host| should be an array of eight 16-bit numbers.
PP_NetAddress_Private MakeIPv6NetAddress(const uint16_t host[], uint16_t port,
                                         uint32_t scope_id) {
  PP_NetAddress_Private addr = PP_NetAddress_Private();
  addr.size = sizeof(sockaddr_in6);
  sockaddr_in6* a = reinterpret_cast<sockaddr_in6*>(addr.data);
  a->sin6_family = AF_INET6;
  a->sin6_port = htons(port);
  a->sin6_flowinfo = 0;
  for (int i = 0; i < 8; i++)
    a->sin6_addr.s6_addr16[i] = htons(host[i]);
  a->sin6_scope_id = scope_id;
  return addr;
}

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
  RUN_TEST(DescribeIPv6, filter);
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

// TODO(viettrungluu): More IPv6 tests needed.

std::string TestNetAddressPrivate::TestDescribeIPv6() {
  static const struct {
    uint16_t address[8];
    uint16_t port;
    uint32_t scope;
    const char* expected_without_port;
    const char* expected_with_port;
  } test_cases[] = {
    {  // Generic test case (unique longest run of zeros to collapse).
      { 0x12, 0xabcd, 0, 0x0001, 0, 0, 0, 0xcdef }, 12, 0,
      "12:abcd:0:1::cdef", "[12:abcd:0:1::cdef]:12"
    },
    {  // Non-zero scope.
      { 0x1234, 0xabcd, 0, 0x0001, 0, 0, 0, 0xcdef }, 1234, 789,
      "1234:abcd:0:1::cdef%789", "[1234:abcd:0:1::cdef%789]:1234"
    },
    {  // Ignore the first (non-longest) run of zeros.
      { 0, 0, 0, 0x0123, 0, 0, 0, 0 }, 123, 0,
      "0:0:0:123::", "[0:0:0:123::]:123"
    },
    {  // Collapse the first (equally-longest) run of zeros.
      { 0x1234, 0xabcd, 0, 0, 0xff, 0, 0, 0xcdef }, 123, 0,
      "1234:abcd::ff:0:0:cdef", "[1234:abcd::ff:0:0:cdef]:123"
    },
    {  // Don't collapse "runs" of zeros of length 1.
      { 0, 0xa, 1, 2, 3, 0, 5, 0 }, 123, 0,
      "0:a:1:2:3:0:5:0", "[0:a:1:2:3:0:5:0]:123"
    },
    {  // Collapse a run of zeros at the beginning.
      { 0, 0, 0, 2, 3, 0, 0, 0 }, 123, 0,
      "::2:3:0:0:0", "[::2:3:0:0:0]:123"
    },
    {  // Collapse a run of zeros at the end.
      { 0, 0xa, 1, 2, 3, 0, 0, 0 }, 123, 0,
      "0:a:1:2:3::", "[0:a:1:2:3::]:123"
    },
    {  // IPv4 192.168.1.2 embedded in IPv6 in the deprecated way.
      { 0, 0, 0, 0, 0, 0, 0xc0a8, 0x102 }, 123, 0,
      "::192.168.1.2", "[::192.168.1.2]:123"
    },
    {  // ... with non-zero scope.
      { 0, 0, 0, 0, 0, 0, 0xc0a8, 0x102 }, 123, 789,
      "::192.168.1.2%789", "[::192.168.1.2%789]:123"
    },
    {  // IPv4 192.168.1.2 embedded in IPv6.
      { 0, 0, 0, 0, 0, 0xffff, 0xc0a8, 0x102 }, 123, 0,
      "::ffff:192.168.1.2", "[::ffff:192.168.1.2]:123"
    },
    {  // ... with non-zero scope.
      { 0, 0, 0, 0, 0, 0xffff, 0xc0a8, 0x102 }, 123, 789,
      "::ffff:192.168.1.2%789", "[::ffff:192.168.1.2%789]:123"
    },
    {  // *Not* IPv4 embedded in IPv6.
      { 0, 0, 0, 0, 0, 0x1234, 0xc0a8, 0x102 }, 123, 0,
      "::1234:c0a8:102", "[::1234:c0a8:102]:123"
    }
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(test_cases); i++) {
    PP_NetAddress_Private addr = MakeIPv6NetAddress(test_cases[i].address,
                                                    test_cases[i].port,
                                                    test_cases[i].scope);
    ASSERT_EQ(test_cases[i].expected_without_port,
              NetAddressPrivate::Describe(addr, false));
    ASSERT_EQ(test_cases[i].expected_with_port,
              NetAddressPrivate::Describe(addr, true));
  }

  PASS();
}
