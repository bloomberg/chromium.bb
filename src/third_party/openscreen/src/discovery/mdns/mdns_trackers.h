// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DISCOVERY_MDNS_MDNS_TRACKERS_H_
#define DISCOVERY_MDNS_MDNS_TRACKERS_H_

#include <unordered_map>

#include "absl/hash/hash.h"
#include "discovery/mdns/mdns_records.h"
#include "platform/api/task_runner.h"
#include "platform/base/error.h"
#include "util/alarm.h"

namespace openscreen {
namespace discovery {

class MdnsRandom;
class MdnsRecord;
class MdnsRecordChangedCallback;
class MdnsSender;

// MdnsTracker is a base class for MdnsRecordTracker and MdnsQuestionTracker for
// the purposes of common code sharing only
class MdnsTracker {
 public:
  using ClockNowFunctionPtr = openscreen::platform::ClockNowFunctionPtr;
  using TaskRunner = openscreen::platform::TaskRunner;

  // MdnsTracker does not own |sender|, |task_runner| and |random_delay|
  // and expects that the lifetime of these objects exceeds the lifetime of
  // MdnsTracker.
  MdnsTracker(MdnsSender* sender,
              TaskRunner* task_runner,
              ClockNowFunctionPtr now_function,
              MdnsRandom* random_delay);
  MdnsTracker(const MdnsTracker& other) = delete;
  MdnsTracker(MdnsTracker&& other) noexcept = delete;
  MdnsTracker& operator=(const MdnsTracker& other) = delete;
  MdnsTracker& operator=(MdnsTracker&& other) noexcept = delete;
  ~MdnsTracker() = default;

 protected:
  MdnsSender* const sender_;
  TaskRunner* const task_runner_;
  const ClockNowFunctionPtr now_function_;
  Alarm send_alarm_;  // TODO(yakimakha): Use cancelable task when available
  MdnsRandom* const random_delay_;
};

// MdnsRecordTracker manages automatic resending of mDNS queries for
// refreshing records as they reach their expiration time.
class MdnsRecordTracker : public MdnsTracker {
 public:
  using Clock = openscreen::platform::Clock;

  MdnsRecordTracker(
      MdnsRecord record,
      MdnsSender* sender,
      TaskRunner* task_runner,
      ClockNowFunctionPtr now_function,
      MdnsRandom* random_delay,
      std::function<void(const MdnsRecord&)> record_expired_callback);

  // Possible outcomes from updating a tracked record.
  enum class UpdateType {
    kGoodbye,  // The record has a TTL of 0 and will expire.
    kTTLOnly,  // The record updated its TTL only.
    kRdata     // The record updated its RDATA.
  };

  // Updates record tracker with the new record:
  // 1. Resets TTL to the value specified in |new_record|.
  // 2. Schedules expiration in case of a goodbye record.
  // Returns Error::Code::kParameterInvalid if new_record is not a valid update
  // for the current tracked record.
  ErrorOr<UpdateType> Update(const MdnsRecord& new_record);

  // Sets record to expire after 1 seconds as per RFC 6762
  void ExpireSoon();

  // Returns a reference to the tracked record.
  const MdnsRecord& record() const { return record_; }

 private:
  void SendQuery();
  Clock::time_point GetNextSendTime();

  // Stores MdnsRecord provided to Start method call.
  MdnsRecord record_;
  // A point in time when the record was received and the tracking has started.
  Clock::time_point start_time_;
  // Number of times record refresh has been attempted.
  size_t attempt_count_ = 0;
  std::function<void(const MdnsRecord&)> record_expired_callback_;
};

// MdnsQuestionTracker manages automatic resending of mDNS queries for
// continuous monitoring with exponential back-off as described in RFC 6762
class MdnsQuestionTracker : public MdnsTracker {
 public:
  using Clock = openscreen::platform::Clock;

  MdnsQuestionTracker(MdnsQuestion question,
                      MdnsSender* sender,
                      TaskRunner* task_runner,
                      ClockNowFunctionPtr now_function,
                      MdnsRandom* random_delay);

  // Returns a reference to the tracked question.
  const MdnsQuestion& question() const { return question_; }

 private:
  using RecordKey = std::tuple<DomainName, DnsType, DnsClass>;

  // Sends a query message via MdnsSender and schedules the next resend.
  void SendQuery();

  // Stores MdnsQuestion provided to Start method call.
  MdnsQuestion question_;

  // A delay between the currently scheduled and the next queries.
  Clock::duration send_delay_;

  // Active record trackers, uniquely identified by domain name, DNS record type
  // and DNS record class. MdnsRecordTracker instances are stored as unique_ptr
  // so they are not moved around in memory when the collection is modified.
  // This allows passing a pointer to MdnsRecordTracker to a task running on the
  // TaskRunner.
  std::unordered_map<RecordKey,
                     std::unique_ptr<MdnsRecordTracker>,
                     absl::Hash<RecordKey>>
      record_trackers_;
};

}  // namespace discovery
}  // namespace openscreen

#endif  // DISCOVERY_MDNS_MDNS_TRACKERS_H_
