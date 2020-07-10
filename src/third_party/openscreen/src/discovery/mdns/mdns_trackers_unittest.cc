// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "discovery/mdns/mdns_trackers.h"

#include "discovery/mdns/mdns_random.h"
#include "discovery/mdns/mdns_record_changed_callback.h"
#include "discovery/mdns/mdns_sender.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "platform/test/fake_clock.h"
#include "platform/test/fake_task_runner.h"

namespace openscreen {
namespace discovery {

using openscreen::platform::Clock;
using openscreen::platform::FakeClock;
using openscreen::platform::FakeTaskRunner;
using openscreen::platform::NetworkInterfaceIndex;
using openscreen::platform::TaskRunner;
using openscreen::platform::UdpSocket;
using testing::_;
using testing::Args;
using testing::Invoke;
using testing::Return;
using testing::WithArgs;

ACTION_P2(VerifyMessageBytesWithoutId, expected_data, expected_size) {
  const uint8_t* actual_data = reinterpret_cast<const uint8_t*>(arg0);
  const size_t actual_size = arg1;
  EXPECT_EQ(actual_size, expected_size);
  // Start at bytes[2] to skip a generated message ID.
  for (size_t i = 2; i < actual_size; ++i) {
    EXPECT_EQ(actual_data[i], expected_data[i]);
  }
}

class MockUdpSocket : public UdpSocket {
 public:
  MOCK_METHOD(bool, IsIPv4, (), (const, override));
  MOCK_METHOD(bool, IsIPv6, (), (const, override));
  MOCK_METHOD(IPEndpoint, GetLocalEndpoint, (), (const, override));
  MOCK_METHOD(void, Bind, (), (override));
  MOCK_METHOD(void,
              SetMulticastOutboundInterface,
              (NetworkInterfaceIndex),
              (override));
  MOCK_METHOD(void,
              JoinMulticastGroup,
              (const IPAddress&, NetworkInterfaceIndex),
              (override));
  MOCK_METHOD(void,
              SendMessage,
              (const void*, size_t, const IPEndpoint&),
              (override));
  MOCK_METHOD(void, SetDscp, (DscpMode), (override));
};

class MockRecordChangedCallback : public MdnsRecordChangedCallback {
 public:
  MOCK_METHOD(void,
              OnRecordChanged,
              (const MdnsRecord&, RecordChangedEvent event),
              (override));
};

class MdnsTrackerTest : public testing::Test {
 public:
  MdnsTrackerTest()
      : clock_(Clock::now()),
        task_runner_(&clock_),
        sender_(&socket_),
        a_question_(DomainName{"testing", "local"},
                    DnsType::kANY,
                    DnsClass::kIN,
                    ResponseType::kMulticast),
        a_record_(DomainName{"testing", "local"},
                  DnsType::kA,
                  DnsClass::kIN,
                  RecordType::kShared,
                  std::chrono::seconds(120),
                  ARecordRdata(IPAddress{172, 0, 0, 1})) {}

  template <class TrackerType>
  void TrackerNoQueryAfterDestruction(TrackerType tracker) {
    tracker.reset();
    EXPECT_CALL(socket_, SendMessage(_, _, _)).Times(0);
    // Advance fake clock by a long time interval to make sure if there's a
    // scheduled task, it will run.
    clock_.Advance(std::chrono::hours(1));
  }

  std::unique_ptr<MdnsRecordTracker> CreateRecordTracker(
      const MdnsRecord& record) {
    return std::make_unique<MdnsRecordTracker>(
        record, &sender_, &task_runner_, &FakeClock::now, &random_,
        [this](const MdnsRecord& record) { expiration_called_ = true; });
  }

  std::unique_ptr<MdnsQuestionTracker> CreateQuestionTracker(
      const MdnsQuestion& question) {
    return std::make_unique<MdnsQuestionTracker>(
        question, &sender_, &task_runner_, &FakeClock::now, &random_);
  }

 protected:
  // clang-format off
  const std::vector<uint8_t> kQuestionQueryBytes = {
      0x00, 0x00,  // ID = 0
      0x00, 0x00,  // FLAGS = None
      0x00, 0x01,  // Question count
      0x00, 0x00,  // Answer count
      0x00, 0x00,  // Authority count
      0x00, 0x00,  // Additional count
      // Question
      0x07, 't', 'e', 's', 't', 'i', 'n', 'g',
      0x05, 'l', 'o', 'c', 'a', 'l',
      0x00,
      0x00, 0xFF,  // TYPE = ANY (255)
      0x00, 0x01,  // CLASS = IN (1)
  };

  const std::vector<uint8_t> kRecordQueryBytes = {
      0x00, 0x00,  // ID = 0
      0x00, 0x00,  // FLAGS = None
      0x00, 0x01,  // Question count
      0x00, 0x00,  // Answer count
      0x00, 0x00,  // Authority count
      0x00, 0x00,  // Additional count
      // Question
      0x07, 't', 'e', 's', 't', 'i', 'n', 'g',
      0x05, 'l', 'o', 'c', 'a', 'l',
      0x00,
      0x00, 0x01,  // TYPE = A (1)
      0x00, 0x01,  // CLASS = IN (1)
  };

  // clang-format on
  FakeClock clock_;
  FakeTaskRunner task_runner_;
  MockUdpSocket socket_;
  MdnsSender sender_;
  MdnsRandom random_;

  MdnsQuestion a_question_;
  MdnsRecord a_record_;

  bool expiration_called_ = false;
};

// Records are re-queried at 80%, 85%, 90% and 95% TTL as per RFC 6762
// Section 5.2 There are no subsequent queries to refresh the record after that,
// the record is expired after TTL has passed since the start of tracking.
// Random variance required is from 0% to 2%, making these times at most 82%,
// 87%, 92% and 97% TTL. Fake clock is advanced to 83%, 88%, 93% and 98% to make
// sure that task gets executed.
// https://tools.ietf.org/html/rfc6762#section-5.2

TEST_F(MdnsTrackerTest, RecordTrackerRecordAccessor) {
  std::unique_ptr<MdnsRecordTracker> tracker = CreateRecordTracker(a_record_);
  EXPECT_EQ(tracker->record(), a_record_);
}

TEST_F(MdnsTrackerTest, RecordTrackerQueryAfterDelay) {
  std::unique_ptr<MdnsRecordTracker> tracker = CreateRecordTracker(a_record_);
  // Only expect 4 queries being sent, when record reaches it's TTL it's
  // considered expired and another query is not sent
  constexpr double kTtlFractions[] = {0.83, 0.88, 0.93, 0.98, 1.00};
  EXPECT_CALL(socket_, SendMessage(_, _, _)).Times(4);

  Clock::duration time_passed{0};
  for (double fraction : kTtlFractions) {
    Clock::duration time_till_refresh =
        std::chrono::duration_cast<Clock::duration>(a_record_.ttl() * fraction);
    Clock::duration delta = time_till_refresh - time_passed;
    time_passed = time_till_refresh;
    clock_.Advance(delta);
  }
}

TEST_F(MdnsTrackerTest, RecordTrackerSendsMessage) {
  std::unique_ptr<MdnsRecordTracker> tracker = CreateRecordTracker(a_record_);

  EXPECT_CALL(socket_, SendMessage(_, _, _))
      .WillOnce(WithArgs<0, 1>(VerifyMessageBytesWithoutId(
          kRecordQueryBytes.data(), kRecordQueryBytes.size())));

  clock_.Advance(
      std::chrono::duration_cast<Clock::duration>(a_record_.ttl() * 0.83));
}

TEST_F(MdnsTrackerTest, RecordTrackerNoQueryAfterDestruction) {
  std::unique_ptr<MdnsRecordTracker> tracker = CreateRecordTracker(a_record_);
  TrackerNoQueryAfterDestruction(std::move(tracker));
}

TEST_F(MdnsTrackerTest, RecordTrackerNoQueryAfterLateTask) {
  std::unique_ptr<MdnsRecordTracker> tracker = CreateRecordTracker(a_record_);
  // If task runner was too busy and callback happened too late, there should be
  // no query and instead the record will expire.
  // Check lower bound for task being late (TTL) and an arbitrarily long time
  // interval to ensure the query is not sent a later time.
  EXPECT_CALL(socket_, SendMessage(_, _, _)).Times(0);
  clock_.Advance(a_record_.ttl());
  clock_.Advance(std::chrono::hours(1));
}

TEST_F(MdnsTrackerTest, RecordTrackerUpdateResetsTtl) {
  expiration_called_ = false;
  std::unique_ptr<MdnsRecordTracker> tracker = CreateRecordTracker(a_record_);
  // Advance time by 60% of record's TTL
  Clock::duration advance_time =
      std::chrono::duration_cast<Clock::duration>(a_record_.ttl() * 0.6);
  clock_.Advance(advance_time);
  // Now update the record, this must reset expiration time
  EXPECT_EQ(tracker->Update(a_record_).value(),
            MdnsRecordTracker::UpdateType::kTTLOnly);
  // Advance time by 60% of record's TTL again
  clock_.Advance(advance_time);
  // Check that expiration callback was not called
  EXPECT_FALSE(expiration_called_);
}

TEST_F(MdnsTrackerTest, RecordTrackerForceExpiration) {
  expiration_called_ = false;
  std::unique_ptr<MdnsRecordTracker> tracker = CreateRecordTracker(a_record_);
  tracker->ExpireSoon();
  // Expire schedules expiration after 1 second.
  clock_.Advance(std::chrono::seconds(1));
  EXPECT_TRUE(expiration_called_);
}

TEST_F(MdnsTrackerTest, RecordTrackerExpirationCallback) {
  expiration_called_ = false;
  std::unique_ptr<MdnsRecordTracker> tracker = CreateRecordTracker(a_record_);
  clock_.Advance(a_record_.ttl());
  EXPECT_TRUE(expiration_called_);
}

TEST_F(MdnsTrackerTest, RecordTrackerExpirationCallbackAfterGoodbye) {
  expiration_called_ = false;
  std::unique_ptr<MdnsRecordTracker> tracker = CreateRecordTracker(a_record_);
  MdnsRecord goodbye_record(a_record_.name(), a_record_.dns_type(),
                            a_record_.dns_class(), a_record_.record_type(),
                            std::chrono::seconds{0}, a_record_.rdata());

  // After a goodbye record is received, expiration is schedule in a second.
  EXPECT_EQ(tracker->Update(goodbye_record).value(),
            MdnsRecordTracker::UpdateType::kGoodbye);

  // No refresh queries are sent after goodbye record is received.
  EXPECT_CALL(socket_, SendMessage(_, _, _)).Times(0);

  // Advance clock to just before the expiration time of 1 second.
  clock_.Advance(std::chrono::microseconds{999999});
  EXPECT_FALSE(expiration_called_);
  // Advance clock to exactly the expiration time.
  clock_.Advance(std::chrono::microseconds{1});
  EXPECT_TRUE(expiration_called_);
}

TEST_F(MdnsTrackerTest, RecordTrackerInvalidUpdate) {
  std::unique_ptr<MdnsRecordTracker> tracker = CreateRecordTracker(a_record_);

  MdnsRecord invalid_name(DomainName{"invalid"}, a_record_.dns_type(),
                          a_record_.dns_class(), a_record_.record_type(),
                          a_record_.ttl(), a_record_.rdata());
  EXPECT_EQ(tracker->Update(invalid_name).error(),
            Error::Code::kParameterInvalid);

  MdnsRecord invalid_type(a_record_.name(), DnsType::kPTR,
                          a_record_.dns_class(), a_record_.record_type(),
                          a_record_.ttl(),
                          PtrRecordRdata{DomainName{"invalid"}});
  EXPECT_EQ(tracker->Update(invalid_type).error(),
            Error::Code::kParameterInvalid);

  MdnsRecord invalid_class(a_record_.name(), a_record_.dns_type(),
                           DnsClass::kANY, a_record_.record_type(),
                           a_record_.ttl(), a_record_.rdata());
  EXPECT_EQ(tracker->Update(invalid_class).error(),
            Error::Code::kParameterInvalid);

  // RDATA must match the old RDATA for goodbye records
  MdnsRecord invalid_rdata(a_record_.name(), a_record_.dns_type(),
                           a_record_.dns_class(), a_record_.record_type(),
                           std::chrono::seconds{0},
                           ARecordRdata(IPAddress{172, 0, 0, 2}));
  EXPECT_EQ(tracker->Update(invalid_rdata).error(),
            Error::Code::kParameterInvalid);
}

TEST_F(MdnsTrackerTest, RecordTrackerNoExpirationCallbackAfterDestruction) {
  expiration_called_ = false;
  std::unique_ptr<MdnsRecordTracker> tracker = CreateRecordTracker(a_record_);
  tracker.reset();
  clock_.Advance(a_record_.ttl());
  EXPECT_FALSE(expiration_called_);
}

// Initial query is delayed for up to 120 ms as per RFC 6762 Section 5.2
// Subsequent queries happen no sooner than a second after the initial query and
// the interval between the queries increases at least by a factor of 2 for each
// next query up until it's capped at 1 hour.
// https://tools.ietf.org/html/rfc6762#section-5.2

TEST_F(MdnsTrackerTest, QuestionTrackerQuestionAccessor) {
  std::unique_ptr<MdnsQuestionTracker> tracker =
      CreateQuestionTracker(a_question_);
  EXPECT_EQ(tracker->question(), a_question_);
}

TEST_F(MdnsTrackerTest, QuestionTrackerQueryAfterDelay) {
  std::unique_ptr<MdnsQuestionTracker> tracker =
      CreateQuestionTracker(a_question_);

  EXPECT_CALL(socket_, SendMessage(_, _, _)).Times(1);
  clock_.Advance(std::chrono::milliseconds(120));

  std::chrono::seconds interval{1};
  while (interval < std::chrono::hours(1)) {
    EXPECT_CALL(socket_, SendMessage(_, _, _)).Times(1);
    clock_.Advance(interval);
    interval *= 2;
  }
}

TEST_F(MdnsTrackerTest, QuestionTrackerSendsMessage) {
  std::unique_ptr<MdnsQuestionTracker> tracker =
      CreateQuestionTracker(a_question_);

  EXPECT_CALL(socket_, SendMessage(_, _, _))
      .WillOnce(WithArgs<0, 1>(VerifyMessageBytesWithoutId(
          kQuestionQueryBytes.data(), kQuestionQueryBytes.size())));

  clock_.Advance(std::chrono::milliseconds(120));
}

TEST_F(MdnsTrackerTest, QuestionTrackerNoQueryAfterDestruction) {
  std::unique_ptr<MdnsQuestionTracker> tracker =
      CreateQuestionTracker(a_question_);
  TrackerNoQueryAfterDestruction(std::move(tracker));
}

}  // namespace discovery
}  // namespace openscreen
