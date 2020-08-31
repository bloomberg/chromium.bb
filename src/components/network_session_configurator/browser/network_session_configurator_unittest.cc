// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/network_session_configurator/browser/network_session_configurator.h"

#include <map>
#include <memory>
#include <string>

#include "base/command_line.h"
#include "base/metrics/field_trial.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "components/network_session_configurator/common/network_features.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "components/variations/variations_associated_data.h"
#include "net/base/host_mapping_rules.h"
#include "net/base/host_port_pair.h"
#include "net/http/http_stream_factory.h"
#include "net/third_party/quiche/src/quic/core/crypto/crypto_protocol.h"
#include "net/third_party/quiche/src/quic/core/quic_packets.h"
#include "net/third_party/quiche/src/spdy/core/spdy_protocol.h"
#include "net/url_request/url_request_context_builder.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_MACOSX) && !defined(OS_IOS)
#include "base/mac/mac_util.h"
#endif

namespace network_session_configurator {

class NetworkSessionConfiguratorTest : public testing::Test {
 public:
  NetworkSessionConfiguratorTest()
      : quic_user_agent_id_("Chrome/52.0.2709.0 Linux x86_64") {
    scoped_feature_list_.Init();
    variations::testing::ClearAllVariationParams();
  }

  void ParseCommandLineAndFieldTrials(const base::CommandLine& command_line) {
    network_session_configurator::ParseCommandLineAndFieldTrials(
        command_line,
        /*is_quic_force_disabled=*/false, quic_user_agent_id_, &params_,
        &quic_params_);
  }

  void ParseFieldTrials() {
    ParseCommandLineAndFieldTrials(
        base::CommandLine(base::CommandLine::NO_PROGRAM));
  }

  std::string quic_user_agent_id_;
  base::test::ScopedFeatureList scoped_feature_list_;
  net::HttpNetworkSession::Params params_;
  net::QuicParams quic_params_;
};

TEST_F(NetworkSessionConfiguratorTest, Defaults) {
  ParseFieldTrials();

  EXPECT_FALSE(params_.ignore_certificate_errors);
  EXPECT_EQ(0u, params_.testing_fixed_http_port);
  EXPECT_EQ(0u, params_.testing_fixed_https_port);
  EXPECT_FALSE(params_.enable_user_alternate_protocol_ports);

  EXPECT_TRUE(params_.enable_http2);
  EXPECT_TRUE(params_.http2_settings.empty());
  EXPECT_FALSE(params_.greased_http2_frame);
  EXPECT_FALSE(params_.enable_websocket_over_http2);

  EXPECT_TRUE(params_.enable_quic);
  EXPECT_TRUE(quic_params_.retry_without_alt_svc_on_quic_errors);
  EXPECT_EQ(1350u, quic_params_.max_packet_length);
  EXPECT_EQ(quic::QuicTagVector(), quic_params_.connection_options);
  EXPECT_EQ(quic::QuicTagVector(), quic_params_.client_connection_options);
  EXPECT_FALSE(params_.enable_server_push_cancellation);
  EXPECT_FALSE(quic_params_.close_sessions_on_ip_change);
  EXPECT_FALSE(quic_params_.goaway_sessions_on_ip_change);
  EXPECT_EQ(net::kIdleConnectionTimeout, quic_params_.idle_connection_timeout);
  EXPECT_EQ(base::TimeDelta::FromSeconds(quic::kPingTimeoutSecs),
            quic_params_.reduced_ping_timeout);
  EXPECT_EQ(base::TimeDelta::FromSeconds(quic::kMaxTimeForCryptoHandshakeSecs),
            quic_params_.max_time_before_crypto_handshake);
  EXPECT_EQ(base::TimeDelta::FromSeconds(quic::kInitialIdleTimeoutSecs),
            quic_params_.max_idle_time_before_crypto_handshake);
  EXPECT_FALSE(quic_params_.estimate_initial_rtt);
  EXPECT_FALSE(quic_params_.migrate_sessions_on_network_change_v2);
  EXPECT_FALSE(quic_params_.migrate_sessions_early_v2);
  EXPECT_FALSE(quic_params_.retry_on_alternate_network_before_handshake);
  EXPECT_FALSE(quic_params_.migrate_idle_sessions);
  EXPECT_FALSE(quic_params_.go_away_on_path_degrading);
  EXPECT_TRUE(quic_params_.initial_rtt_for_handshake.is_zero());
  EXPECT_FALSE(quic_params_.allow_server_migration);
  EXPECT_TRUE(params_.quic_host_allowlist.empty());
  EXPECT_TRUE(quic_params_.retransmittable_on_wire_timeout.is_zero());

  EXPECT_EQ(net::DefaultSupportedQuicVersions(),
            quic_params_.supported_versions);
  EXPECT_FALSE(params_.enable_quic_proxies_for_https_urls);
  EXPECT_EQ("Chrome/52.0.2709.0 Linux x86_64", quic_params_.user_agent_id);
  EXPECT_EQ(0u, quic_params_.origins_to_force_quic_on.size());
}

TEST_F(NetworkSessionConfiguratorTest, Http2FieldTrialGroupNameDoesNotMatter) {
  base::FieldTrialList::CreateFieldTrial("HTTP2", "Disable");

  ParseFieldTrials();

  EXPECT_TRUE(params_.enable_http2);
}

TEST_F(NetworkSessionConfiguratorTest, Http2FieldTrialDisable) {
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["http2_enabled"] = "false";
  variations::AssociateVariationParams("HTTP2", "Experiment",
                                       field_trial_params);
  base::FieldTrialList::CreateFieldTrial("HTTP2", "Experiment");

  ParseFieldTrials();

  EXPECT_FALSE(params_.enable_http2);
}

TEST_F(NetworkSessionConfiguratorTest, DisableQuicFromFieldTrialGroup) {
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["enable_quic"] = "false";
  variations::AssociateVariationParams("QUIC", "Disabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("QUIC", "Disabled");

  ParseFieldTrials();

  EXPECT_FALSE(params_.enable_quic);
}

TEST_F(NetworkSessionConfiguratorTest, EnableQuicFromParams) {
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["enable_quic"] = "true";
  variations::AssociateVariationParams("QUIC", "UseQuic", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("QUIC", "UseQuic");

  ParseFieldTrials();

  EXPECT_TRUE(params_.enable_quic);
}

TEST_F(NetworkSessionConfiguratorTest, EnableQuicForDataReductionProxy) {
  base::FieldTrialList::CreateFieldTrial("QUIC", "Enabled");
  base::FieldTrialList::CreateFieldTrial("DataReductionProxyUseQuic",
                                         "Enabled");

  ParseFieldTrials();

  EXPECT_TRUE(params_.enable_quic);
}

TEST_F(NetworkSessionConfiguratorTest, EnableQuicProxiesForHttpsUrls) {
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["enable_quic_proxies_for_https_urls"] = "true";
  variations::AssociateVariationParams("QUIC", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("QUIC", "Enabled");

  ParseFieldTrials();

  EXPECT_TRUE(params_.enable_quic_proxies_for_https_urls);
}

TEST_F(NetworkSessionConfiguratorTest, DisableRetryWithoutAltSvcOnQuicErrors) {
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["retry_without_alt_svc_on_quic_errors"] = "false";
  variations::AssociateVariationParams("QUIC", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("QUIC", "Enabled");

  ParseFieldTrials();

  EXPECT_FALSE(quic_params_.retry_without_alt_svc_on_quic_errors);
}

TEST_F(NetworkSessionConfiguratorTest,
       QuicCloseSessionsOnIpChangeFromFieldTrialParams) {
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["close_sessions_on_ip_change"] = "true";
  variations::AssociateVariationParams("QUIC", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("QUIC", "Enabled");

  ParseFieldTrials();

  EXPECT_TRUE(quic_params_.close_sessions_on_ip_change);
}

TEST_F(NetworkSessionConfiguratorTest,
       QuicGoAwaySessionsOnIpChangeFromFieldTrialParams) {
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["goaway_sessions_on_ip_change"] = "true";
  variations::AssociateVariationParams("QUIC", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("QUIC", "Enabled");

  ParseFieldTrials();

  EXPECT_TRUE(quic_params_.goaway_sessions_on_ip_change);
}

TEST_F(NetworkSessionConfiguratorTest,
       QuicRetransmittableOnWireTimeoutMillisecondsFieldTrialParams) {
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["retransmittable_on_wire_timeout_milliseconds"] = "1000";
  variations::AssociateVariationParams("QUIC", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("QUIC", "Enabled");

  ParseFieldTrials();

  EXPECT_EQ(base::TimeDelta::FromMilliseconds(1000),
            quic_params_.retransmittable_on_wire_timeout);
}

TEST_F(NetworkSessionConfiguratorTest,
       QuicIdleConnectionTimeoutSecondsFieldTrialParams) {
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["idle_connection_timeout_seconds"] = "300";
  variations::AssociateVariationParams("QUIC", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("QUIC", "Enabled");

  ParseFieldTrials();

  EXPECT_EQ(base::TimeDelta::FromSeconds(300),
            quic_params_.idle_connection_timeout);
}

TEST_F(NetworkSessionConfiguratorTest,
       NegativeQuicReducedPingTimeoutSecondsFieldTrialParams) {
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["reduced_ping_timeout_seconds"] = "-5";
  variations::AssociateVariationParams("QUIC", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("QUIC", "Enabled");
  ParseFieldTrials();
  EXPECT_EQ(base::TimeDelta::FromSeconds(quic::kPingTimeoutSecs),
            quic_params_.reduced_ping_timeout);
}

TEST_F(NetworkSessionConfiguratorTest,
       LargeQuicReducedPingTimeoutSecondsFieldTrialParams) {
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["reduced_ping_timeout_seconds"] = "50";
  variations::AssociateVariationParams("QUIC", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("QUIC", "Enabled");
  ParseFieldTrials();
  EXPECT_EQ(base::TimeDelta::FromSeconds(quic::kPingTimeoutSecs),
            quic_params_.reduced_ping_timeout);
}

TEST_F(NetworkSessionConfiguratorTest,
       QuicReducedPingTimeoutSecondsFieldTrialParams) {
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["reduced_ping_timeout_seconds"] = "10";
  variations::AssociateVariationParams("QUIC", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("QUIC", "Enabled");
  ParseFieldTrials();
  EXPECT_EQ(base::TimeDelta::FromSeconds(10),
            quic_params_.reduced_ping_timeout);
}

TEST_F(NetworkSessionConfiguratorTest,
       QuicMaxTimeBeforeCryptoHandshakeSeconds) {
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["max_time_before_crypto_handshake_seconds"] = "7";
  variations::AssociateVariationParams("QUIC", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("QUIC", "Enabled");
  ParseFieldTrials();
  EXPECT_EQ(base::TimeDelta::FromSeconds(7),
            quic_params_.max_time_before_crypto_handshake);
}

TEST_F(NetworkSessionConfiguratorTest,
       NegativeQuicMaxTimeBeforeCryptoHandshakeSeconds) {
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["max_time_before_crypto_handshake_seconds"] = "-1";
  variations::AssociateVariationParams("QUIC", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("QUIC", "Enabled");
  ParseFieldTrials();
  EXPECT_EQ(base::TimeDelta::FromSeconds(quic::kMaxTimeForCryptoHandshakeSecs),
            quic_params_.max_time_before_crypto_handshake);
}

TEST_F(NetworkSessionConfiguratorTest,
       QuicMaxIdleTimeBeforeCryptoHandshakeSeconds) {
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["max_idle_time_before_crypto_handshake_seconds"] = "11";
  variations::AssociateVariationParams("QUIC", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("QUIC", "Enabled");
  ParseFieldTrials();
  EXPECT_EQ(base::TimeDelta::FromSeconds(11),
            quic_params_.max_idle_time_before_crypto_handshake);
}

TEST_F(NetworkSessionConfiguratorTest,
       NegativeQuicMaxIdleTimeBeforeCryptoHandshakeSeconds) {
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["max_idle_time_before_crypto_handshake_seconds"] = "-1";
  variations::AssociateVariationParams("QUIC", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("QUIC", "Enabled");
  ParseFieldTrials();
  EXPECT_EQ(base::TimeDelta::FromSeconds(quic::kInitialIdleTimeoutSecs),
            quic_params_.max_idle_time_before_crypto_handshake);
}

TEST_F(NetworkSessionConfiguratorTest, EnableServerPushCancellation) {
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["enable_server_push_cancellation"] = "true";
  variations::AssociateVariationParams("QUIC", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("QUIC", "Enabled");

  ParseFieldTrials();

  EXPECT_TRUE(params_.enable_server_push_cancellation);
}

TEST_F(NetworkSessionConfiguratorTest, QuicEstimateInitialRtt) {
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["estimate_initial_rtt"] = "true";
  variations::AssociateVariationParams("QUIC", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("QUIC", "Enabled");

  ParseFieldTrials();

  EXPECT_TRUE(quic_params_.estimate_initial_rtt);
}

TEST_F(NetworkSessionConfiguratorTest,
       QuicMigrateSessionsOnNetworkChangeV2FromFieldTrialParams) {
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["migrate_sessions_on_network_change_v2"] = "true";
  variations::AssociateVariationParams("QUIC", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("QUIC", "Enabled");

  ParseFieldTrials();

  EXPECT_TRUE(quic_params_.migrate_sessions_on_network_change_v2);
}

TEST_F(NetworkSessionConfiguratorTest,
       QuicMigrateSessionsEarlyV2FromFieldTrialParams) {
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["migrate_sessions_early_v2"] = "true";
  variations::AssociateVariationParams("QUIC", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("QUIC", "Enabled");

  ParseFieldTrials();

  EXPECT_TRUE(quic_params_.migrate_sessions_early_v2);
}

TEST_F(NetworkSessionConfiguratorTest,
       QuicRetryOnAlternateNetworkBeforeHandshakeFromFieldTrialParams) {
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["retry_on_alternate_network_before_handshake"] = "true";
  variations::AssociateVariationParams("QUIC", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("QUIC", "Enabled");

  ParseFieldTrials();

  EXPECT_TRUE(quic_params_.retry_on_alternate_network_before_handshake);
}

TEST_F(NetworkSessionConfiguratorTest,
       QuicGoawayOnPathDegradingFromFieldTrialParams) {
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["go_away_on_path_degrading"] = "true";
  variations::AssociateVariationParams("QUIC", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("QUIC", "Enabled");

  ParseFieldTrials();

  EXPECT_TRUE(quic_params_.go_away_on_path_degrading);
}

TEST_F(NetworkSessionConfiguratorTest,
       QuicIdleSessionMigrationPeriodFromFieldTrialParams) {
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["migrate_idle_sessions"] = "true";
  field_trial_params["idle_session_migration_period_seconds"] = "15";
  variations::AssociateVariationParams("QUIC", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("QUIC", "Enabled");

  ParseFieldTrials();

  EXPECT_TRUE(quic_params_.migrate_idle_sessions);
  EXPECT_EQ(base::TimeDelta::FromSeconds(15),
            quic_params_.idle_session_migration_period);
}

TEST_F(NetworkSessionConfiguratorTest,
       QuicMaxTimeOnNonDefaultNetworkFromFieldTrialParams) {
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["max_time_on_non_default_network_seconds"] = "10";
  variations::AssociateVariationParams("QUIC", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("QUIC", "Enabled");

  ParseFieldTrials();

  EXPECT_EQ(base::TimeDelta::FromSeconds(10),
            quic_params_.max_time_on_non_default_network);
}

TEST_F(
    NetworkSessionConfiguratorTest,
    QuicMaxNumMigrationsToNonDefaultNetworkOnWriteErrorFromFieldTrialParams) {
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["max_migrations_to_non_default_network_on_write_error"] =
      "3";
  variations::AssociateVariationParams("QUIC", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("QUIC", "Enabled");

  ParseFieldTrials();

  EXPECT_EQ(3,
            quic_params_.max_migrations_to_non_default_network_on_write_error);
}

TEST_F(
    NetworkSessionConfiguratorTest,
    QuicMaxNumMigrationsToNonDefaultNetworkOnPathDegradingFromFieldTrialParams) {
  std::map<std::string, std::string> field_trial_params;
  field_trial_params
      ["max_migrations_to_non_default_network_on_path_degrading"] = "4";
  variations::AssociateVariationParams("QUIC", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("QUIC", "Enabled");

  ParseFieldTrials();

  EXPECT_EQ(
      4, quic_params_.max_migrations_to_non_default_network_on_path_degrading);
}

TEST_F(NetworkSessionConfiguratorTest,
       QuicAllowPortMigrationFromFieldTrialParams) {
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["allow_port_migration"] = "true";
  variations::AssociateVariationParams("QUIC", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("QUIC", "Enabled");

  ParseFieldTrials();

  EXPECT_TRUE(quic_params_.allow_port_migration);
}

TEST_F(NetworkSessionConfiguratorTest, PacketLengthFromFieldTrialParams) {
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["max_packet_length"] = "1450";
  variations::AssociateVariationParams("QUIC", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("QUIC", "Enabled");

  ParseFieldTrials();

  EXPECT_EQ(1450u, quic_params_.max_packet_length);
}

TEST_F(NetworkSessionConfiguratorTest, QuicVersionFromFieldTrialParams) {
  // Find first version with QUIC_CRYPTO.
  quic::ParsedQuicVersionVector all_supported_versions =
      quic::AllSupportedVersions();
  auto it = all_supported_versions.begin();
  while (it->handshake_protocol != quic::PROTOCOL_QUIC_CRYPTO) {
    ++it;
    ASSERT_NE(it, all_supported_versions.end());
  }
  quic::ParsedQuicVersion version = *it;

  std::map<std::string, std::string> field_trial_params;
  field_trial_params["quic_version"] =
      quic::QuicVersionToString(version.transport_version);
  variations::AssociateVariationParams("QUIC", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("QUIC", "Enabled");

  ParseFieldTrials();

  quic::ParsedQuicVersionVector supported_versions = {version};
  EXPECT_EQ(supported_versions, quic_params_.supported_versions);
}

TEST_F(NetworkSessionConfiguratorTest, QuicVersionFromFieldTrialParamsAlpn) {
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["quic_version"] = "h3-T050";
  variations::AssociateVariationParams("QUIC", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("QUIC", "Enabled");

  ParseFieldTrials();

  quic::ParsedQuicVersionVector supported_versions = {
      {quic::PROTOCOL_TLS1_3, quic::QUIC_VERSION_50}};
  EXPECT_EQ(supported_versions, quic_params_.supported_versions);
}

TEST_F(NetworkSessionConfiguratorTest,
       MultipleQuicVersionFromFieldTrialParams) {
  quic::ParsedQuicVersionVector versions_with_quic_crypto;
  for (const auto& version : quic::AllSupportedVersions()) {
    if (version.handshake_protocol == quic::PROTOCOL_QUIC_CRYPTO) {
      versions_with_quic_crypto.push_back(version);
    }
  }
  ASSERT_LE(2u, versions_with_quic_crypto.size());

  quic::ParsedQuicVersion version1 = versions_with_quic_crypto.front();
  quic::ParsedQuicVersion version2 = versions_with_quic_crypto.back();
  std::string quic_versions =
      quic::QuicVersionToString(version1.transport_version) + "," +
      quic::QuicVersionToString(version2.transport_version);

  std::map<std::string, std::string> field_trial_params;
  field_trial_params["quic_version"] = quic_versions;
  variations::AssociateVariationParams("QUIC", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("QUIC", "Enabled");

  ParseFieldTrials();

  quic::ParsedQuicVersionVector supported_versions = {version1, version2};
  EXPECT_EQ(supported_versions, quic_params_.supported_versions);
}

TEST_F(NetworkSessionConfiguratorTest,
       MultipleQuicVersionFromFieldTrialParamsAlpn) {
  quic::ParsedQuicVersion version1 = quic::AllSupportedVersions().front();
  quic::ParsedQuicVersion version2 = quic::AllSupportedVersions().back();
  std::string quic_versions =
      quic::AlpnForVersion(version1) + "," + quic::AlpnForVersion(version2);

  std::map<std::string, std::string> field_trial_params;
  field_trial_params["quic_version"] = quic_versions;
  variations::AssociateVariationParams("QUIC", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("QUIC", "Enabled");

  ParseFieldTrials();

  quic::ParsedQuicVersionVector supported_versions = {version1, version2};
  EXPECT_EQ(supported_versions, quic_params_.supported_versions);
}

TEST_F(NetworkSessionConfiguratorTest,
       QuicConnectionOptionsFromFieldTrialParams) {
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["connection_options"] = "TIME,TBBR,REJ";
  variations::AssociateVariationParams("QUIC", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("QUIC", "Enabled");

  ParseFieldTrials();

  quic::QuicTagVector options;
  options.push_back(quic::kTIME);
  options.push_back(quic::kTBBR);
  options.push_back(quic::kREJ);
  EXPECT_EQ(options, quic_params_.connection_options);
}

TEST_F(NetworkSessionConfiguratorTest,
       QuicClientConnectionOptionsFromFieldTrialParams) {
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["client_connection_options"] = "TBBR,1RTT";
  variations::AssociateVariationParams("QUIC", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("QUIC", "Enabled");

  ParseFieldTrials();

  quic::QuicTagVector options;
  options.push_back(quic::kTBBR);
  options.push_back(quic::k1RTT);
  EXPECT_EQ(options, quic_params_.client_connection_options);
}

TEST_F(NetworkSessionConfiguratorTest, QuicHostAllowlist) {
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["host_whitelist"] = "www.example.org, www.example.com";
  variations::AssociateVariationParams("QUIC", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("QUIC", "Enabled");

  ParseFieldTrials();

  EXPECT_EQ(2u, params_.quic_host_allowlist.size());
  EXPECT_TRUE(base::Contains(params_.quic_host_allowlist, "www.example.com"));
  EXPECT_TRUE(base::Contains(params_.quic_host_allowlist, "www.example.org"));
}

TEST_F(NetworkSessionConfiguratorTest, QuicHostAllowlistEmpty) {
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["host_whitelist"] = "";
  variations::AssociateVariationParams("QUIC", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("QUIC", "Enabled");

  ParseFieldTrials();

  EXPECT_TRUE(params_.quic_host_allowlist.empty());
}

TEST_F(NetworkSessionConfiguratorTest, QuicFlags) {
  FLAGS_quic_reloadable_flag_quic_enable_version_t050_v2 = false;
  FLAGS_quic_reloadable_flag_quic_enable_version_draft_27 = false;
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["set_quic_flags"] =
      "FLAGS_quic_reloadable_flag_quic_enable_version_t050_v2=true,"
      "FLAGS_quic_reloadable_flag_quic_enable_version_draft_27=true";
  variations::AssociateVariationParams("QUIC", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("QUIC", "Enabled");

  ParseFieldTrials();

  EXPECT_TRUE(FLAGS_quic_reloadable_flag_quic_enable_version_t050_v2);
  EXPECT_TRUE(FLAGS_quic_reloadable_flag_quic_enable_version_draft_27);
}

TEST_F(NetworkSessionConfiguratorTest, Http2SettingsFromFieldTrialParams) {
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["http2_settings"] = "7:1234,25:5678";
  variations::AssociateVariationParams("HTTP2", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("HTTP2", "Enabled");

  ParseFieldTrials();

  spdy::SettingsMap expected_settings;
  expected_settings[static_cast<spdy::SpdyKnownSettingsId>(7)] = 1234;
  expected_settings[static_cast<spdy::SpdyKnownSettingsId>(25)] = 5678;
  EXPECT_EQ(expected_settings, params_.http2_settings);
}

TEST_F(NetworkSessionConfiguratorTest, ForceQuic) {
  struct {
    bool force_enabled;
    bool force_disabled;
    bool expect_quic_enabled;
  } kTests[] = {
      {true /* force_enabled */, false /* force_disabled */,
       true /* expect_quic_enabled */},
      {false /* force_enabled */, true /* force_disabled */,
       false /* expect_quic_enabled */},
      {true /* force_enabled */, true /* force_disabled */,
       false /* expect_quic_enabled */},
  };

  for (const auto& test : kTests) {
    base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
    if (test.force_enabled)
      command_line.AppendSwitch(switches::kEnableQuic);
    if (test.force_disabled)
      command_line.AppendSwitch(switches::kDisableQuic);
    ParseCommandLineAndFieldTrials(command_line);
    EXPECT_EQ(test.expect_quic_enabled, params_.enable_quic);
  }
}

TEST_F(NetworkSessionConfiguratorTest, DisableHttp2) {
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitch(switches::kDisableHttp2);
  ParseCommandLineAndFieldTrials(command_line);
  EXPECT_FALSE(params_.enable_http2);
}

TEST_F(NetworkSessionConfiguratorTest, QuicConnectionOptions) {
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitch(switches::kEnableQuic);
  command_line.AppendSwitchASCII(switches::kQuicConnectionOptions,
                                 "TIMER,TBBR,REJ");
  ParseCommandLineAndFieldTrials(command_line);

  quic::QuicTagVector expected_options;
  expected_options.push_back(quic::kTIME);
  expected_options.push_back(quic::kTBBR);
  expected_options.push_back(quic::kREJ);
  EXPECT_EQ(expected_options, quic_params_.connection_options);
}

TEST_F(NetworkSessionConfiguratorTest, QuicMaxPacketLength) {
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitch(switches::kEnableQuic);
  command_line.AppendSwitchASCII(switches::kQuicMaxPacketLength, "42");
  ParseCommandLineAndFieldTrials(command_line);
  EXPECT_EQ(42u, quic_params_.max_packet_length);
}

TEST_F(NetworkSessionConfiguratorTest, OriginToForceQuicOn) {
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitch(switches::kEnableQuic);
  command_line.AppendSwitchASCII(switches::kOriginToForceQuicOn, "*");
  ParseCommandLineAndFieldTrials(command_line);
  EXPECT_EQ(1u, quic_params_.origins_to_force_quic_on.size());
  EXPECT_EQ(1u,
            quic_params_.origins_to_force_quic_on.count(net::HostPortPair()));
}

TEST_F(NetworkSessionConfiguratorTest, OriginToForceQuicOn2) {
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitch(switches::kEnableQuic);
  command_line.AppendSwitchASCII(switches::kOriginToForceQuicOn, "foo:1234");
  ParseCommandLineAndFieldTrials(command_line);
  EXPECT_EQ(1u, quic_params_.origins_to_force_quic_on.size());
  EXPECT_EQ(1u, quic_params_.origins_to_force_quic_on.count(
                    net::HostPortPair("foo", 1234)));
}

TEST_F(NetworkSessionConfiguratorTest, OriginToForceQuicOn3) {
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitch(switches::kEnableQuic);
  command_line.AppendSwitchASCII(switches::kOriginToForceQuicOn, "foo:1,bar:2");
  ParseCommandLineAndFieldTrials(command_line);
  EXPECT_EQ(2u, quic_params_.origins_to_force_quic_on.size());
  EXPECT_EQ(1u, quic_params_.origins_to_force_quic_on.count(
                    net::HostPortPair("foo", 1)));
  EXPECT_EQ(1u, quic_params_.origins_to_force_quic_on.count(
                    net::HostPortPair("bar", 2)));
}

TEST_F(NetworkSessionConfiguratorTest, EnableUserAlternateProtocolPorts) {
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitch(switches::kEnableUserAlternateProtocolPorts);
  ParseCommandLineAndFieldTrials(command_line);
  EXPECT_TRUE(params_.enable_user_alternate_protocol_ports);
}

TEST_F(NetworkSessionConfiguratorTest, TestingFixedPorts) {
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitchASCII(switches::kTestingFixedHttpPort, "800");
  command_line.AppendSwitchASCII(switches::kTestingFixedHttpsPort, "801");
  ParseCommandLineAndFieldTrials(command_line);
  EXPECT_EQ(800, params_.testing_fixed_http_port);
  EXPECT_EQ(801, params_.testing_fixed_https_port);
}

TEST_F(NetworkSessionConfiguratorTest, IgnoreCertificateErrors) {
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitch(switches::kIgnoreCertificateErrors);
  ParseCommandLineAndFieldTrials(command_line);
  EXPECT_TRUE(params_.ignore_certificate_errors);
}

TEST_F(NetworkSessionConfiguratorTest, HostRules) {
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitchASCII(switches::kHostRules, "map *.com foo");
  ParseCommandLineAndFieldTrials(command_line);

  net::HostPortPair host_port_pair("spam.net", 80);
  EXPECT_FALSE(params_.host_mapping_rules.RewriteHost(&host_port_pair));
  EXPECT_EQ("spam.net", host_port_pair.host());

  host_port_pair = net::HostPortPair("spam.com", 80);
  EXPECT_TRUE(params_.host_mapping_rules.RewriteHost(&host_port_pair));
  EXPECT_EQ("foo", host_port_pair.host());
}

TEST_F(NetworkSessionConfiguratorTest, DefaultCacheBackend) {
#if defined(OS_ANDROID) || defined(OS_LINUX) || defined(OS_CHROMEOS)
  EXPECT_EQ(net::URLRequestContextBuilder::HttpCacheParams::DISK_SIMPLE,
            ChooseCacheType());
#elif defined(OS_MACOSX) && !defined(OS_IOS)
  EXPECT_EQ(
      base::mac::IsAtLeastOS10_14()
          ? net::URLRequestContextBuilder::HttpCacheParams::DISK_SIMPLE
          : net::URLRequestContextBuilder::HttpCacheParams::DISK_BLOCKFILE,
      ChooseCacheType());
#else
  EXPECT_EQ(net::URLRequestContextBuilder::HttpCacheParams::DISK_BLOCKFILE,
            ChooseCacheType());
#endif
}

TEST_F(NetworkSessionConfiguratorTest, SimpleCacheTrialExperimentYes) {
  base::FieldTrialList::CreateFieldTrial("SimpleCacheTrial", "ExperimentYes");
  EXPECT_EQ(net::URLRequestContextBuilder::HttpCacheParams::DISK_SIMPLE,
            ChooseCacheType());
}

TEST_F(NetworkSessionConfiguratorTest, SimpleCacheTrialDisable) {
  base::FieldTrialList::CreateFieldTrial("SimpleCacheTrial", "Disable");
#if !defined(OS_ANDROID)
  EXPECT_EQ(net::URLRequestContextBuilder::HttpCacheParams::DISK_BLOCKFILE,
            ChooseCacheType());
#else  // defined(OS_ANDROID)
  // Android always uses the simple cache.
  EXPECT_EQ(net::URLRequestContextBuilder::HttpCacheParams::DISK_SIMPLE,
            ChooseCacheType());
#endif
}

TEST_F(NetworkSessionConfiguratorTest, QuicHeadersIncludeH2StreamDependency) {
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["headers_include_h2_stream_dependency"] = "true";
  variations::AssociateVariationParams("QUIC", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("QUIC", "Enabled");

  ParseFieldTrials();

  EXPECT_TRUE(quic_params_.headers_include_h2_stream_dependency);
}

TEST_F(NetworkSessionConfiguratorTest, Http2GreaseSettingsFromCommandLine) {
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitch(switches::kHttp2GreaseSettings);

  ParseCommandLineAndFieldTrials(command_line);

  bool greased_setting_found = false;
  for (const auto& setting : params_.http2_settings) {
    if ((setting.first & 0x0f0f) == 0x0a0a) {
      greased_setting_found = true;
      break;
    }
  }
  EXPECT_TRUE(greased_setting_found);
}

TEST_F(NetworkSessionConfiguratorTest, Http2GreaseSettingsFromFieldTrial) {
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["http2_grease_settings"] = "true";
  variations::AssociateVariationParams("HTTP2", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("HTTP2", "Enabled");

  ParseFieldTrials();

  bool greased_setting_found = false;
  for (const auto& setting : params_.http2_settings) {
    if ((setting.first & 0x0f0f) == 0x0a0a) {
      greased_setting_found = true;
      break;
    }
  }
  EXPECT_TRUE(greased_setting_found);
}

TEST_F(NetworkSessionConfiguratorTest, Http2GreaseFrameTypeFromCommandLine) {
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitch(switches::kHttp2GreaseFrameType);

  ParseCommandLineAndFieldTrials(command_line);

  ASSERT_TRUE(params_.greased_http2_frame);
  const uint8_t frame_type = params_.greased_http2_frame.value().type;
  EXPECT_EQ(0x0b, frame_type % 0x1f);
}

TEST_F(NetworkSessionConfiguratorTest, Http2GreaseFrameTypeFromFieldTrial) {
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["http2_grease_frame_type"] = "true";
  variations::AssociateVariationParams("HTTP2", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("HTTP2", "Enabled");

  ParseFieldTrials();

  ASSERT_TRUE(params_.greased_http2_frame);
  const uint8_t frame_type = params_.greased_http2_frame.value().type;
  EXPECT_EQ(0x0b, frame_type % 0x1f);
}

TEST_F(NetworkSessionConfiguratorTest,
       WebsocketOverHttp2EnabledFromCommandLine) {
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitch(switches::kEnableWebsocketOverHttp2);

  ParseCommandLineAndFieldTrials(command_line);

  EXPECT_TRUE(params_.enable_websocket_over_http2);
}

TEST_F(NetworkSessionConfiguratorTest,
       WebsocketOverHttp2EnabledFromFieldTrial) {
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["websocket_over_http2"] = "true";
  variations::AssociateVariationParams("HTTP2", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("HTTP2", "Enabled");

  ParseFieldTrials();

  EXPECT_TRUE(params_.enable_websocket_over_http2);
}

TEST_F(NetworkSessionConfiguratorTest,
       QuicInitialRttForHandshakeFromFieldTrailParams) {
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["initial_rtt_for_handshake_milliseconds"] = "500";
  variations::AssociateVariationParams("QUIC", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("QUIC", "Enabled");

  ParseFieldTrials();

  EXPECT_EQ(base::TimeDelta::FromMilliseconds(500),
            quic_params_.initial_rtt_for_handshake);
}

class NetworkSessionConfiguratorWithQuicVersionTest
    : public NetworkSessionConfiguratorTest,
      public ::testing::WithParamInterface<quic::ParsedQuicVersion> {
 public:
  NetworkSessionConfiguratorWithQuicVersionTest() : version_(GetParam()) {}
  ~NetworkSessionConfiguratorWithQuicVersionTest() override = default;

  const quic::ParsedQuicVersion version_;
};

INSTANTIATE_TEST_SUITE_P(QuicVersion,
                         NetworkSessionConfiguratorWithQuicVersionTest,
                         ::testing::ValuesIn(quic::AllSupportedVersions()));

TEST_P(NetworkSessionConfiguratorWithQuicVersionTest, QuicVersion) {
  if (version_.handshake_protocol != quic::PROTOCOL_QUIC_CRYPTO) {
    return;
  }

  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitch(switches::kEnableQuic);
  command_line.AppendSwitchASCII(
      switches::kQuicVersion,
      quic::QuicVersionToString(version_.transport_version));
  ParseCommandLineAndFieldTrials(command_line);

  ASSERT_EQ(1u, quic_params_.supported_versions.size());
  EXPECT_EQ(version_, quic_params_.supported_versions[0]);
}

TEST_P(NetworkSessionConfiguratorWithQuicVersionTest, QuicVersionAlpn) {
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitch(switches::kEnableQuic);
  command_line.AppendSwitchASCII(switches::kQuicVersion,
                                 quic::AlpnForVersion(version_));
  ParseCommandLineAndFieldTrials(command_line);

  ASSERT_EQ(1u, quic_params_.supported_versions.size());
  EXPECT_EQ(version_, quic_params_.supported_versions[0]);
}

TEST_P(NetworkSessionConfiguratorWithQuicVersionTest,
       SameQuicVersionsFromFieldTrialParams) {
  if (version_.handshake_protocol != quic::PROTOCOL_QUIC_CRYPTO) {
    return;
  }

  std::string quic_versions =
      quic::QuicVersionToString(version_.transport_version) + "," +
      quic::QuicVersionToString(version_.transport_version);

  std::map<std::string, std::string> field_trial_params;
  field_trial_params["quic_version"] = quic_versions;
  variations::AssociateVariationParams("QUIC", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("QUIC", "Enabled");

  ParseFieldTrials();

  ASSERT_EQ(1u, quic_params_.supported_versions.size());
  EXPECT_EQ(version_, quic_params_.supported_versions[0]);
}

TEST_P(NetworkSessionConfiguratorWithQuicVersionTest,
       SameQuicVersionsFromFieldTrialParamsAlpn) {
  std::string quic_versions =
      quic::AlpnForVersion(version_) + "," + quic::AlpnForVersion(version_);

  std::map<std::string, std::string> field_trial_params;
  field_trial_params["quic_version"] = quic_versions;
  variations::AssociateVariationParams("QUIC", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("QUIC", "Enabled");

  ParseFieldTrials();

  ASSERT_EQ(1u, quic_params_.supported_versions.size());
  EXPECT_EQ(version_, quic_params_.supported_versions[0]);
}

}  // namespace network_session_configurator
