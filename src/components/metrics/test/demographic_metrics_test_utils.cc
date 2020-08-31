// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/test/demographic_metrics_test_utils.h"

#include "base/strings/stringprintf.h"
#include "base/time/default_clock.h"
#include "base/time/default_tick_clock.h"
#include "components/sync/base/pref_names.h"
#include "components/sync/base/user_demographics.h"
#include "components/sync/engine_impl/loopback_server/persistent_unique_client_entity.h"
#include "components/sync/protocol/sync.pb.h"
#include "third_party/metrics_proto/chrome_user_metrics_extension.pb.h"
#include "third_party/zlib/google/compression_utils.h"

namespace metrics {
namespace test {

void AddUserBirthYearAndGenderToSyncServer(
    base::WeakPtr<fake_server::FakeServer> fake_server,
    int birth_year,
    UserDemographicsProto::Gender gender) {
  sync_pb::EntitySpecifics specifics;
  specifics.mutable_priority_preference()->mutable_preference()->set_name(
      syncer::prefs::kSyncDemographics);
  specifics.mutable_priority_preference()->mutable_preference()->set_value(
      base::StringPrintf("{\"birth_year\":%d,\"gender\":%d}", birth_year,
                         static_cast<int>(gender)));
  fake_server->InjectEntity(
      syncer::PersistentUniqueClientEntity::CreateFromSpecificsForTesting(
          /*non_unique_name=*/syncer::prefs::kSyncDemographics,
          /*client_tag=*/specifics.preference().name(), specifics,
          /*creation_time=*/0,
          /*last_modified_time=*/0));
}

void UpdateNetworkTime(const base::Time& now,
                       network_time::NetworkTimeTracker* time_tracker) {
  // Simulate the latency in the network to get the network time from the remote
  // server.
  constexpr base::TimeDelta kLatency = base::TimeDelta::FromMilliseconds(10);

  // Simulate the time taken to call UpdateNetworkTime() since the moment the
  // callback was created. When not testing with the fake sync server, the
  // callback is called when doing an HTTP request to the sync server.
  constexpr base::TimeDelta kCallbackDelay =
      base::TimeDelta::FromMilliseconds(10);

  // Simulate a network time that is a bit earlier than the now time.
  base::Time network_time = now - kCallbackDelay - kLatency;

  // Simulate the time in ticks at the moment the UpdateNetworkTime callback
  // function is created, which time should be at least 1 millisecond behind the
  // moment the callback is run to pass the DCHECK.
  base::TimeTicks post_time = base::TimeTicks::Now() - kCallbackDelay;

  time_tracker->UpdateNetworkTime(
      network_time, /*resolution=*/base::TimeDelta::FromMilliseconds(1),
      kLatency, post_time);
}

int GetMaximumEligibleBirthYear(const base::Time& now) {
  constexpr int kEligibleAge =
      syncer::kUserDemographicsMinAgeInYears +
      syncer::kUserDemographicsBirthYearNoiseOffsetRange;

  base::Time::Exploded exploded_time;
  now.UTCExplode(&exploded_time);

  // Return the maximum birth year that is eligible for reporting the user's
  // birth year and gender. The -1 year is the extra buffer that Sync uses to
  // make sure that the user is at least 20 years old because the user gives
  // only the year of their birth date. E.g., if today's date is 05 Jan 2020
  // and the user was born 05 Mar 2000, the user's age would be computed as 20
  // years old using the year resolution, but the user is in fact 19.
  return exploded_time.year - kEligibleAge - 1;
}

int GetNoisedBirthYear(const PrefService& pref_service, int raw_birth_year) {
  int birth_year_offset =
      pref_service.GetInteger(syncer::prefs::kSyncDemographicsBirthYearOffset);
  return birth_year_offset + raw_birth_year;
}

void BuildAndStoreLog(MetricsService* metrics_service) {
  metrics_service->StageCurrentLogForTest();
}

bool HasUnsentLogs(MetricsService* metrics_service) {
  return metrics_service->LogStoreForTest()->has_unsent_logs();
}

// Returns an UMA log if the MetricsService has a staged log.
std::unique_ptr<ChromeUserMetricsExtension> GetLastUmaLog(
    MetricsService* metrics_service) {
  // Decompress the staged log.
  std::string uncompressed_log;
  if (!compression::GzipUncompress(
          metrics_service->LogStoreForTest()->staged_log(),
          &uncompressed_log)) {
    return nullptr;
  }

  // Deserialize and return the log.
  std::unique_ptr<ChromeUserMetricsExtension> log =
      std::make_unique<ChromeUserMetricsExtension>();
  if (!log->ParseFromString(uncompressed_log)) {
    return nullptr;
  }
  return log;
}

}  // namespace test
}  // namespace metrics
