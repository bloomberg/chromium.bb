/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "pc/sctp_transport.h"

#include <utility>
#include <vector>

#include "absl/memory/memory.h"
#include "p2p/base/fake_dtls_transport.h"
#include "pc/dtls_transport.h"
#include "rtc_base/gunit.h"
#include "test/gmock.h"
#include "test/gtest.h"

constexpr int kDefaultTimeout = 1000;  // milliseconds

using cricket::FakeDtlsTransport;
using ::testing::ElementsAre;

namespace webrtc {

namespace {

class FakeCricketSctpTransport : public cricket::SctpTransportInternal {
 public:
  void SetDtlsTransport(rtc::PacketTransportInternal* transport) override {}
  bool Start(int local_port, int remote_port) override { return true; }
  bool OpenStream(int sid) override { return true; }
  bool ResetStream(int sid) override { return true; }
  bool SendData(const cricket::SendDataParams& params,
                const rtc::CopyOnWriteBuffer& payload,
                cricket::SendDataResult* result = nullptr) override {
    return true;
  }
  bool ReadyToSendData() override { return true; }
  void set_debug_name_for_testing(const char* debug_name) override {}
  // Methods exposed for testing
  void SendSignalReadyToSendData() { SignalReadyToSendData(); }

  void SendSignalClosingProcedureStartedRemotely() {
    SignalClosingProcedureStartedRemotely(1);
  }

  void SendSignalClosingProcedureComplete() {
    SignalClosingProcedureComplete(1);
  }
};

}  // namespace

class TestSctpTransportObserver : public SctpTransportObserverInterface {
 public:
  void OnStateChange(SctpTransportInformation info) override {
    states_.push_back(info.state());
  }

  SctpTransportState State() {
    if (states_.size() > 0) {
      return states_[states_.size() - 1];
    } else {
      return SctpTransportState::kNew;
    }
  }

  const std::vector<SctpTransportState>& States() { return states_; }

 private:
  std::vector<SctpTransportState> states_;
};

class SctpTransportTest : public testing::Test {
 public:
  SctpTransport* transport() { return transport_.get(); }
  SctpTransportObserverInterface* observer() { return &observer_; }

  void CreateTransport() {
    auto cricket_sctp_transport =
        absl::WrapUnique(new FakeCricketSctpTransport());
    transport_ = new rtc::RefCountedObject<SctpTransport>(
        std::move(cricket_sctp_transport));
  }

  void AddDtlsTransport() {
    std::unique_ptr<cricket::DtlsTransportInternal> cricket_transport =
        absl::make_unique<FakeDtlsTransport>(
            "audio", cricket::ICE_CANDIDATE_COMPONENT_RTP);
    dtls_transport_ =
        new rtc::RefCountedObject<DtlsTransport>(std::move(cricket_transport));
    transport_->SetDtlsTransport(dtls_transport_);
  }

  void CompleteSctpHandshake() {
    CricketSctpTransport()->SendSignalReadyToSendData();
  }

  FakeCricketSctpTransport* CricketSctpTransport() {
    return static_cast<FakeCricketSctpTransport*>(transport_->internal());
  }

  rtc::scoped_refptr<SctpTransport> transport_;
  rtc::scoped_refptr<DtlsTransport> dtls_transport_;
  TestSctpTransportObserver observer_;
};

TEST(SctpTransportSimpleTest, CreateClearDelete) {
  std::unique_ptr<cricket::SctpTransportInternal> fake_cricket_sctp_transport =
      absl::WrapUnique(new FakeCricketSctpTransport());
  rtc::scoped_refptr<SctpTransport> sctp_transport =
      new rtc::RefCountedObject<SctpTransport>(
          std::move(fake_cricket_sctp_transport));
  ASSERT_TRUE(sctp_transport->internal());
  ASSERT_EQ(SctpTransportState::kNew, sctp_transport->Information().state());
  sctp_transport->Clear();
  ASSERT_FALSE(sctp_transport->internal());
  ASSERT_EQ(SctpTransportState::kClosed, sctp_transport->Information().state());
}

TEST_F(SctpTransportTest, EventsObservedWhenConnecting) {
  CreateTransport();
  transport()->RegisterObserver(observer());
  AddDtlsTransport();
  CompleteSctpHandshake();
  ASSERT_EQ_WAIT(SctpTransportState::kConnected, observer_.State(),
                 kDefaultTimeout);
  EXPECT_THAT(observer_.States(), ElementsAre(SctpTransportState::kConnecting,
                                              SctpTransportState::kConnected));
}

TEST_F(SctpTransportTest, CloseWhenClearing) {
  CreateTransport();
  transport()->RegisterObserver(observer());
  AddDtlsTransport();
  CompleteSctpHandshake();
  ASSERT_EQ_WAIT(SctpTransportState::kConnected, observer_.State(),
                 kDefaultTimeout);
  transport()->Clear();
  ASSERT_EQ_WAIT(SctpTransportState::kClosed, observer_.State(),
                 kDefaultTimeout);
}

}  // namespace webrtc
