// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "discovery/mdns/mdns_trackers.h"

#include <array>

#include "discovery/mdns/mdns_random.h"
#include "discovery/mdns/mdns_record_changed_callback.h"
#include "discovery/mdns/mdns_sender.h"
#include "util/std_util.h"

namespace openscreen {
namespace discovery {

namespace {

// RFC 6762 Section 5.2
// https://tools.ietf.org/html/rfc6762#section-5.2

// Attempt to refresh a record should be performed at 80%, 85%, 90% and 95% TTL.
constexpr double kTtlFractions[] = {0.80, 0.85, 0.90, 0.95, 1.00};

// Intervals between successive queries must increase by at least a factor of 2.
constexpr int kIntervalIncreaseFactor = 2;

// The interval between the first two queries must be at least one second.
constexpr std::chrono::seconds kMinimumQueryInterval{1};

// The querier may cap the question refresh interval to a maximum of 60 minutes.
constexpr std::chrono::minutes kMaximumQueryInterval{60};

// RFC 6762 Section 10.1
// https://tools.ietf.org/html/rfc6762#section-10.1

// A goodbye record is a record with TTL of 0.
bool IsGoodbyeRecord(const MdnsRecord& record) {
  return record.ttl() == std::chrono::seconds{0};
}

// RFC 6762 Section 10.1
// https://tools.ietf.org/html/rfc6762#section-10.1
// In case of a goodbye record, the querier should set TTL to 1 second
constexpr std::chrono::seconds kGoodbyeRecordTtl{1};

}  // namespace

MdnsTracker::MdnsTracker(MdnsSender* sender,
                         TaskRunner* task_runner,
                         ClockNowFunctionPtr now_function,
                         MdnsRandom* random_delay)
    : sender_(sender),
      task_runner_(task_runner),
      now_function_(now_function),
      send_alarm_(now_function, task_runner),
      random_delay_(random_delay) {
  OSP_DCHECK(task_runner);
  OSP_DCHECK(now_function);
  OSP_DCHECK(random_delay);
  OSP_DCHECK(sender);
}

MdnsRecordTracker::MdnsRecordTracker(
    MdnsRecord record,
    MdnsSender* sender,
    TaskRunner* task_runner,
    ClockNowFunctionPtr now_function,
    MdnsRandom* random_delay,
    std::function<void(const MdnsRecord&)> record_expired_callback)
    : MdnsTracker(sender, task_runner, now_function, random_delay),
      record_(std::move(record)),
      start_time_(now_function_()),
      record_expired_callback_(record_expired_callback) {
  OSP_DCHECK(record_expired_callback);

  send_alarm_.Schedule([this] { SendQuery(); }, GetNextSendTime());
}

ErrorOr<MdnsRecordTracker::UpdateType> MdnsRecordTracker::Update(
    const MdnsRecord& new_record) {
  OSP_DCHECK(task_runner_->IsRunningOnTaskRunner());
  bool has_same_rdata = record_.rdata() == new_record.rdata();

  // Goodbye records must have the same RDATA but TTL of 0.
  // RFC 6762 Section 10.1
  // https://tools.ietf.org/html/rfc6762#section-10.1
  if ((record_.dns_type() != new_record.dns_type()) ||
      (record_.dns_class() != new_record.dns_class()) ||
      (record_.name() != new_record.name()) ||
      (IsGoodbyeRecord(new_record) && !has_same_rdata)) {
    // The new record has been passed to a wrong tracker.
    return Error::Code::kParameterInvalid;
  }

  UpdateType result = UpdateType::kGoodbye;
  if (IsGoodbyeRecord(new_record)) {
    record_ = MdnsRecord(new_record.name(), new_record.dns_type(),
                         new_record.dns_class(), new_record.record_type(),
                         kGoodbyeRecordTtl, new_record.rdata());

    // Goodbye records do not need to be requeried, set the attempt count to the
    // last item, which is 100% of TTL, i.e. record expiration.
    attempt_count_ = openscreen::countof(kTtlFractions) - 1;
  } else {
    record_ = new_record;
    attempt_count_ = 0;
    result = has_same_rdata ? UpdateType::kTTLOnly : UpdateType::kRdata;
  }

  start_time_ = now_function_();
  send_alarm_.Schedule([this] { SendQuery(); }, GetNextSendTime());

  return result;
}

void MdnsRecordTracker::ExpireSoon() {
  OSP_DCHECK(task_runner_->IsRunningOnTaskRunner());

  record_ =
      MdnsRecord(record_.name(), record_.dns_type(), record_.dns_class(),
                 record_.record_type(), kGoodbyeRecordTtl, record_.rdata());

  // Set the attempt count to the last item, which is 100% of TTL, i.e. record
  // expiration, to prevent any requeries
  attempt_count_ = openscreen::countof(kTtlFractions) - 1;
  start_time_ = now_function_();
  send_alarm_.Schedule([this] { SendQuery(); }, GetNextSendTime());
}

void MdnsRecordTracker::SendQuery() {
  const Clock::time_point expiration_time = start_time_ + record_.ttl();
  const bool is_expired = (now_function_() >= expiration_time);
  if (!is_expired) {
    MdnsQuestion question(record_.name(), record_.dns_type(),
                          record_.dns_class(), ResponseType::kMulticast);
    MdnsMessage message(CreateMessageId(), MessageType::Query);
    message.AddQuestion(std::move(question));
    sender_->SendMulticast(message);
    send_alarm_.Schedule([this] { MdnsRecordTracker::SendQuery(); },
                         GetNextSendTime());
  } else {
    record_expired_callback_(record_);
  }
}

openscreen::platform::Clock::time_point MdnsRecordTracker::GetNextSendTime() {
  OSP_DCHECK(attempt_count_ < openscreen::countof(kTtlFractions));

  double ttl_fraction = kTtlFractions[attempt_count_++];

  // Do not add random variation to the expiration time (last fraction of TTL)
  if (attempt_count_ != openscreen::countof(kTtlFractions)) {
    ttl_fraction += random_delay_->GetRecordTtlVariation();
  }

  const Clock::duration delay =
      std::chrono::duration_cast<Clock::duration>(record_.ttl() * ttl_fraction);
  return start_time_ + delay;
}

MdnsQuestionTracker::MdnsQuestionTracker(MdnsQuestion question,
                                         MdnsSender* sender,
                                         TaskRunner* task_runner,
                                         ClockNowFunctionPtr now_function,
                                         MdnsRandom* random_delay)
    : MdnsTracker(sender, task_runner, now_function, random_delay),
      question_(std::move(question)),
      send_delay_(kMinimumQueryInterval) {
  // The initial query has to be sent after a random delay of 20-120
  // milliseconds.
  const Clock::duration delay = random_delay_->GetInitialQueryDelay();
  send_alarm_.Schedule([this] { MdnsQuestionTracker::SendQuery(); },
                       now_function_() + delay);
}

void MdnsQuestionTracker::SendQuery() {
  MdnsMessage message(CreateMessageId(), MessageType::Query);
  message.AddQuestion(question_);
  // TODO(yakimakha): Implement known-answer suppression by adding known
  // answers to the question
  sender_->SendMulticast(message);
  send_alarm_.Schedule([this] { MdnsQuestionTracker::SendQuery(); },
                       now_function_() + send_delay_);
  send_delay_ = send_delay_ * kIntervalIncreaseFactor;
  if (send_delay_ > kMaximumQueryInterval) {
    send_delay_ = kMaximumQueryInterval;
  }
}

}  // namespace discovery
}  // namespace openscreen
