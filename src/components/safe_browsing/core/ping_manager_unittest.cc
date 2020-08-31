// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "components/safe_browsing/core/ping_manager.h"
#include "base/base64.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/safe_browsing/core/db/v4_test_util.h"
#include "google_apis/google_api_keys.h"
#include "net/base/escape.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::Time;
using base::TimeDelta;
using safe_browsing::HitReport;
using safe_browsing::ThreatSource;

namespace safe_browsing {

class PingManagerTest : public testing::Test {
 public:
  PingManagerTest() {}

 protected:
  void SetUp() override {
    std::string key = google_apis::GetAPIKey();
    if (!key.empty()) {
      key_param_ = base::StringPrintf(
          "&key=%s", net::EscapeQueryParamValue(key, true).c_str());
    }

    ping_manager_.reset(
        new PingManager(nullptr, safe_browsing::GetTestV4ProtocolConfig()));
  }

  PingManager* ping_manager() { return ping_manager_.get(); }

  std::string key_param_;
  std::unique_ptr<PingManager> ping_manager_;
};

TEST_F(PingManagerTest, TestSafeBrowsingHitUrl) {
  HitReport base_hp;
  base_hp.malicious_url = GURL("http://malicious.url.com");
  base_hp.page_url = GURL("http://page.url.com");
  base_hp.referrer_url = GURL("http://referrer.url.com");

  {
    HitReport hp(base_hp);
    hp.threat_type = SB_THREAT_TYPE_URL_MALWARE;
    hp.threat_source = ThreatSource::LOCAL_PVER3;
    hp.is_subresource = true;
    hp.extended_reporting_level = SBER_LEVEL_LEGACY;
    hp.is_metrics_reporting_active = true;
    hp.is_enhanced_protection = true;

    EXPECT_EQ(
        "https://safebrowsing.google.com/safebrowsing/report?client=unittest&"
        "appver=1.0&pver=4.0" +
            key_param_ +
            "&ext=1&enh=1&evts=malblhit&evtd=http%3A%2F%2Fmalicious.url.com%2F&"
            "evtr=http%3A%2F%2Fpage.url.com%2F&evhr=http%3A%2F%2Freferrer."
            "url.com%2F&evtb=1&src=l3&m=1",
        ping_manager()->SafeBrowsingHitUrl(hp).spec());
  }

  {
    HitReport hp(base_hp);
    hp.threat_type = SB_THREAT_TYPE_URL_PHISHING;
    hp.threat_source = ThreatSource::DATA_SAVER;
    hp.is_subresource = false;
    hp.extended_reporting_level = SBER_LEVEL_LEGACY;
    hp.is_metrics_reporting_active = true;
    hp.is_enhanced_protection = false;
    EXPECT_EQ(
        "https://safebrowsing.google.com/safebrowsing/report?client=unittest&"
        "appver=1.0&pver=4.0" +
            key_param_ +
            "&ext=1&evts=phishblhit&"
            "evtd=http%3A%2F%2Fmalicious.url.com%2F&"
            "evtr=http%3A%2F%2Fpage.url.com%2F&evhr=http%3A%2F%2Freferrer."
            "url.com%2F&evtb=0&src=ds&m=1",
        ping_manager()->SafeBrowsingHitUrl(hp).spec());
  }

  {
    HitReport hp(base_hp);
    hp.threat_type = SB_THREAT_TYPE_URL_PHISHING;
    hp.threat_source = ThreatSource::DATA_SAVER;
    hp.is_subresource = false;
    hp.extended_reporting_level = SBER_LEVEL_SCOUT;
    hp.is_metrics_reporting_active = true;
    hp.is_enhanced_protection = true;
    EXPECT_EQ(
        "https://safebrowsing.google.com/safebrowsing/report?client=unittest&"
        "appver=1.0&pver=4.0" +
            key_param_ +
            "&ext=2&enh=1&evts=phishblhit&"
            "evtd=http%3A%2F%2Fmalicious.url.com%2F&"
            "evtr=http%3A%2F%2Fpage.url.com%2F&evhr=http%3A%2F%2Freferrer."
            "url.com%2F&evtb=0&src=ds&m=1",
        ping_manager()->SafeBrowsingHitUrl(hp).spec());
  }

  {
    HitReport hp(base_hp);
    hp.threat_type = SB_THREAT_TYPE_URL_BINARY_MALWARE;
    hp.threat_source = ThreatSource::REMOTE;
    hp.extended_reporting_level = SBER_LEVEL_OFF;
    hp.is_metrics_reporting_active = true;
    hp.is_subresource = false;
    hp.is_enhanced_protection = true;
    EXPECT_EQ(
        "https://safebrowsing.google.com/safebrowsing/report?client=unittest&"
        "appver=1.0&pver=4.0" +
            key_param_ +
            "&ext=0&enh=1&evts=binurlhit&"
            "evtd=http%3A%2F%2Fmalicious.url.com%2F&"
            "evtr=http%3A%2F%2Fpage.url.com%2F&evhr=http%3A%2F%2Freferrer."
            "url.com%2F&evtb=0&src=rem&m=1",
        ping_manager()->SafeBrowsingHitUrl(hp).spec());
  }

  {
    HitReport hp(base_hp);
    hp.threat_type = SB_THREAT_TYPE_URL_CLIENT_SIDE_PHISHING;
    hp.threat_source = ThreatSource::LOCAL_PVER4;
    hp.extended_reporting_level = SBER_LEVEL_OFF;
    hp.is_metrics_reporting_active = false;
    hp.is_subresource = false;
    hp.is_enhanced_protection = true;
    EXPECT_EQ(
        "https://safebrowsing.google.com/safebrowsing/report?client=unittest&"
        "appver=1.0&pver=4.0" +
            key_param_ +
            "&ext=0&enh=1&evts=phishcsdhit&"
            "evtd=http%3A%2F%2Fmalicious.url.com%2F&"
            "evtr=http%3A%2F%2Fpage.url.com%2F&evhr=http%3A%2F%2Freferrer."
            "url.com%2F&evtb=0&src=l4&m=0",
        ping_manager()->SafeBrowsingHitUrl(hp).spec());
  }

  {
    HitReport hp(base_hp);
    hp.threat_type = SB_THREAT_TYPE_URL_CLIENT_SIDE_MALWARE;
    hp.threat_source = ThreatSource::LOCAL_PVER4;
    hp.extended_reporting_level = SBER_LEVEL_OFF;
    hp.is_metrics_reporting_active = false;
    hp.is_subresource = true;
    hp.is_enhanced_protection = true;
    EXPECT_EQ(
        "https://safebrowsing.google.com/safebrowsing/report?client=unittest&"
        "appver=1.0&pver=4.0" +
            key_param_ +
            "&ext=0&enh=1&evts=malcsdhit&"
            "evtd=http%3A%2F%2Fmalicious.url.com%2F&"
            "evtr=http%3A%2F%2Fpage.url.com%2F&evhr=http%3A%2F%2Freferrer."
            "url.com%2F&evtb=1&src=l4&m=0",
        ping_manager()->SafeBrowsingHitUrl(hp).spec());
  }

  // Same as above, but add population_id
  {
    HitReport hp(base_hp);
    hp.threat_type = SB_THREAT_TYPE_URL_CLIENT_SIDE_MALWARE;
    hp.threat_source = ThreatSource::LOCAL_PVER4;
    hp.extended_reporting_level = SBER_LEVEL_OFF;
    hp.is_metrics_reporting_active = false;
    hp.is_subresource = true;
    hp.population_id = "foo bar";
    hp.is_enhanced_protection = true;
    EXPECT_EQ(
        "https://safebrowsing.google.com/safebrowsing/report?client=unittest&"
        "appver=1.0&pver=4.0" +
            key_param_ +
            "&ext=0&enh=1&evts=malcsdhit&"
            "evtd=http%3A%2F%2Fmalicious.url.com%2F&"
            "evtr=http%3A%2F%2Fpage.url.com%2F&evhr=http%3A%2F%2Freferrer."
            "url.com%2F&evtb=1&src=l4&m=0&up=foo+bar",
        ping_manager()->SafeBrowsingHitUrl(hp).spec());
  }
}

TEST_F(PingManagerTest, TestThreatDetailsUrl) {
  EXPECT_EQ(
      "https://safebrowsing.google.com/safebrowsing/clientreport/malware?"
      "client=unittest&appver=1.0&pver=4.0" +
          key_param_,
      ping_manager()->ThreatDetailsUrl().spec());
}

}  // namespace safe_browsing
