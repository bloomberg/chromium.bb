// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_http_utils.h"

#include <stdint.h>

#include <limits>

#include "net/third_party/quic/platform/api/quic_endian.h"
#include "net/third_party/quiche/src/spdy/core/spdy_alt_svc_wire_format.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace test {

TEST(QuicHttpUtilsTest, ConvertRequestPriorityToQuicPriority) {
  EXPECT_EQ(0u, ConvertRequestPriorityToQuicPriority(HIGHEST));
  EXPECT_EQ(1u, ConvertRequestPriorityToQuicPriority(MEDIUM));
  EXPECT_EQ(2u, ConvertRequestPriorityToQuicPriority(LOW));
  EXPECT_EQ(3u, ConvertRequestPriorityToQuicPriority(LOWEST));
  EXPECT_EQ(4u, ConvertRequestPriorityToQuicPriority(IDLE));
}

TEST(QuicHttpUtilsTest, ConvertQuicPriorityToRequestPriority) {
  EXPECT_EQ(HIGHEST, ConvertQuicPriorityToRequestPriority(0));
  EXPECT_EQ(MEDIUM, ConvertQuicPriorityToRequestPriority(1));
  EXPECT_EQ(LOW, ConvertQuicPriorityToRequestPriority(2));
  EXPECT_EQ(LOWEST, ConvertQuicPriorityToRequestPriority(3));
  EXPECT_EQ(IDLE, ConvertQuicPriorityToRequestPriority(4));
  // These are invalid values, but we should still handle them
  // gracefully. TODO(rtenneti): should we test for all possible values of
  // uint32_t?
  for (int i = 5; i < std::numeric_limits<uint8_t>::max(); ++i) {
    EXPECT_EQ(IDLE, ConvertQuicPriorityToRequestPriority(i));
  }
}

TEST(QuicHttpUtilsTest, FilterSupportedAltSvcVersions) {
  quic::QuicTransportVersionVector supported_versions = {
      quic::QUIC_VERSION_46, quic::QUIC_VERSION_39, quic::QUIC_VERSION_44};

  std::vector<uint32_t> alt_svc_versions_google = {quic::QUIC_VERSION_44,
                                                   quic::QUIC_VERSION_43};
  std::vector<uint32_t> alt_svc_versions_ietf = {
      QuicVersionToQuicVersionLabel(quic::QUIC_VERSION_44),
      QuicVersionToQuicVersionLabel(quic::QUIC_VERSION_43)};

  quic::QuicTransportVersionVector supported_alt_svc_versions = {
      quic::QUIC_VERSION_44};
  spdy::SpdyAltSvcWireFormat::AlternativeService altsvc;

  altsvc.protocol_id = "quic";
  altsvc.version = alt_svc_versions_google;
  EXPECT_EQ(supported_alt_svc_versions,
            FilterSupportedAltSvcVersions(altsvc, supported_versions, true));
  EXPECT_EQ(supported_alt_svc_versions,
            FilterSupportedAltSvcVersions(altsvc, supported_versions, false));

  altsvc.protocol_id = "hq";
  altsvc.version = alt_svc_versions_ietf;
  EXPECT_EQ(supported_alt_svc_versions,
            FilterSupportedAltSvcVersions(altsvc, supported_versions, true));
  EXPECT_EQ(quic::QuicTransportVersionVector(),
            FilterSupportedAltSvcVersions(altsvc, supported_versions, false));

  altsvc.protocol_id = "invalid_protocol";
  altsvc.version = alt_svc_versions_ietf;
  EXPECT_EQ(quic::QuicTransportVersionVector(),
            FilterSupportedAltSvcVersions(altsvc, supported_versions, true));
  EXPECT_EQ(quic::QuicTransportVersionVector(),
            FilterSupportedAltSvcVersions(altsvc, supported_versions, false));
}

}  // namespace test
}  // namespace net
