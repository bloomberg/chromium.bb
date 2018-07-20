#ifndef NET_THIRD_PARTY_QUIC_PLATFORM_API_QUIC_EXPECT_BUG_H_
#define NET_THIRD_PARTY_QUIC_PLATFORM_API_QUIC_EXPECT_BUG_H_

#include "net/third_party/quic/platform/impl/quic_expect_bug_impl.h"

#define EXPECT_QUIC_BUG EXPECT_QUIC_BUG_IMPL
#define EXPECT_QUIC_PEER_BUG(statement, regex) \
  EXPECT_QUIC_PEER_BUG_IMPL(statement, regex)

#endif  // NET_THIRD_PARTY_QUIC_PLATFORM_API_QUIC_EXPECT_BUG_H_
