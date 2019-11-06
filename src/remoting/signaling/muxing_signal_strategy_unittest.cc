// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/signaling/muxing_signal_strategy.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/memory/ptr_util.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_task_environment.h"
#include "base/time/time.h"
#include "remoting/signaling/fake_signal_strategy.h"
#include "remoting/signaling/signaling_address.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libjingle_xmpp/xmllite/xmlelement.h"
#include "third_party/libjingle_xmpp/xmpp/constants.h"

namespace remoting {

namespace {

using testing::_;
using testing::Return;

constexpr char kLocalFtlId[] = "local_user@domain.com/chromoting_ftl_abc123";
constexpr char kRemoteFtlId[] = "remote_user@domain.com/chromoting_ftl_def456";
constexpr char kLocalJabberId[] = "local_user@domain.com/chromotingABC123";
constexpr char kRemoteJabberId[] = "remote_user@domain.com/chromotingDEF456";

constexpr base::TimeDelta kWaitForAllStrategiesConnectedTimeout =
    base::TimeDelta::FromSecondsD(5.5);

MATCHER_P(StanzaMatchesString, expected_str, "") {
  return arg->Str() == expected_str;
}

std::unique_ptr<jingle_xmpp::XmlElement> CreateXmlStanza(
    const std::string& from,
    const std::string& to) {
  static constexpr char kStanzaTemplate[] =
      "<iq xmlns=\"jabber:client\" type=\"set\">"
      "<bind xmlns=\"urn:ietf:params:xml:ns:xmpp-bind\">"
      "<resource>chromoting</resource>"
      "</bind>"
      "</iq>";
  auto stanza = base::WrapUnique<jingle_xmpp::XmlElement>(
      jingle_xmpp::XmlElement::ForStr(kStanzaTemplate));
  stanza->SetAttr(jingle_xmpp::QN_FROM, from);
  stanza->SetAttr(jingle_xmpp::QN_TO, to);
  return stanza;
}

}  // namespace

class MuxingSignalStrategyTest : public testing::Test,
                                 public SignalStrategy::Listener {
 public:
  MuxingSignalStrategyTest() {
    auto ftl_signal_strategy =
        std::make_unique<FakeSignalStrategy>(SignalingAddress(kLocalFtlId));
    auto xmpp_signal_strategy =
        std::make_unique<FakeSignalStrategy>(SignalingAddress(kLocalJabberId));
    ftl_signal_strategy_ = ftl_signal_strategy.get();
    xmpp_signal_strategy_ = xmpp_signal_strategy.get();

    // Start in disconnected state.
    ftl_signal_strategy_->Disconnect();
    xmpp_signal_strategy_->Disconnect();

    ftl_signal_strategy_->SetPeerCallback(mock_ftl_peer_callback_.Get());
    xmpp_signal_strategy_->SetPeerCallback(mock_xmpp_peer_callback_.Get());

    muxing_signal_strategy_ = std::make_unique<MuxingSignalStrategy>(
        std::move(ftl_signal_strategy), std::move(xmpp_signal_strategy));
    muxing_signal_strategy_->AddListener(this);
  }

  ~MuxingSignalStrategyTest() override {
    scoped_task_environment_.FastForwardUntilNoTasksRemain();
  }

 protected:
  base::test::ScopedTaskEnvironment scoped_task_environment_{
      base::test::ScopedTaskEnvironment::MainThreadType::MOCK_TIME};

  std::unique_ptr<MuxingSignalStrategy> muxing_signal_strategy_;

  FakeSignalStrategy* ftl_signal_strategy_;
  FakeSignalStrategy* xmpp_signal_strategy_;

  base::MockCallback<FakeSignalStrategy::PeerCallback> mock_ftl_peer_callback_;
  base::MockCallback<FakeSignalStrategy::PeerCallback> mock_xmpp_peer_callback_;

  std::vector<SignalStrategy::State> state_history_;
  std::vector<std::unique_ptr<jingle_xmpp::XmlElement>> received_messages_;
  std::vector<SignalingAddress> received_stanza_local_addresses_;

 private:
  // SignalStrategy::Listener overrides.
  void OnSignalStrategyStateChange(SignalStrategy::State state) override {
    state_history_.push_back(state);
  }

  bool OnSignalStrategyIncomingStanza(
      const jingle_xmpp::XmlElement* stanza) override {
    received_messages_.push_back(
        std::make_unique<jingle_xmpp::XmlElement>(*stanza));
    received_stanza_local_addresses_.push_back(
        muxing_signal_strategy_->GetLocalAddress());
    return true;
  }
};

TEST_F(MuxingSignalStrategyTest, StateTransition_NothingIsConnected) {
  scoped_task_environment_.FastForwardBy(kWaitForAllStrategiesConnectedTimeout);

  ASSERT_EQ(0u, state_history_.size());
}

TEST_F(MuxingSignalStrategyTest, StateTransition_OnlyXmppIsConnected) {
  xmpp_signal_strategy_->SetState(SignalStrategy::CONNECTING);
  xmpp_signal_strategy_->Connect();
  scoped_task_environment_.FastForwardBy(kWaitForAllStrategiesConnectedTimeout);

  ASSERT_EQ(3u, state_history_.size());

  // SOME_CONNECTING
  ASSERT_EQ(SignalStrategy::CONNECTING, state_history_[0]);

  // ONLY_ONE_CONNECTED_BEFORE_TIMEOUT
  ASSERT_EQ(SignalStrategy::CONNECTING, state_history_[1]);

  // ONLY_ONE_CONNECTED_AFTER_TIMEOUT
  ASSERT_EQ(SignalStrategy::CONNECTED, state_history_[2]);
}

TEST_F(MuxingSignalStrategyTest, StateTransition_OnlyFtlIsConnected) {
  ftl_signal_strategy_->SetState(SignalStrategy::CONNECTING);
  ftl_signal_strategy_->Connect();
  scoped_task_environment_.FastForwardBy(kWaitForAllStrategiesConnectedTimeout);

  ASSERT_EQ(3u, state_history_.size());

  // SOME_CONNECTING
  ASSERT_EQ(SignalStrategy::CONNECTING, state_history_[0]);

  // ONLY_ONE_CONNECTED_BEFORE_TIMEOUT
  ASSERT_EQ(SignalStrategy::CONNECTING, state_history_[1]);

  // ONLY_ONE_CONNECTED_AFTER_TIMEOUT
  ASSERT_EQ(SignalStrategy::CONNECTED, state_history_[2]);
}

TEST_F(MuxingSignalStrategyTest,
       StateTransition_BothAreConnectingThenConnected) {
  xmpp_signal_strategy_->SetState(SignalStrategy::CONNECTING);
  ftl_signal_strategy_->SetState(SignalStrategy::CONNECTING);
  xmpp_signal_strategy_->Connect();
  ftl_signal_strategy_->Connect();
  scoped_task_environment_.FastForwardBy(kWaitForAllStrategiesConnectedTimeout);

  ASSERT_EQ(3u, state_history_.size());

  // SOME_CONNECTING
  ASSERT_EQ(SignalStrategy::CONNECTING, state_history_[0]);

  // ONLY_ONE_CONNECTED_BEFORE_TIMEOUT
  ASSERT_EQ(SignalStrategy::CONNECTING, state_history_[1]);

  // ALL_CONNECTED
  ASSERT_EQ(SignalStrategy::CONNECTED, state_history_[2]);
}

TEST_F(MuxingSignalStrategyTest,
       StateTransition_ConnectingThenConnectedOneAfterAnother) {
  xmpp_signal_strategy_->SetState(SignalStrategy::CONNECTING);
  xmpp_signal_strategy_->Connect();
  ftl_signal_strategy_->SetState(SignalStrategy::CONNECTING);
  ftl_signal_strategy_->Connect();
  scoped_task_environment_.FastForwardBy(kWaitForAllStrategiesConnectedTimeout);

  ASSERT_EQ(3u, state_history_.size());

  // SOME_CONNECTING
  ASSERT_EQ(SignalStrategy::CONNECTING, state_history_[0]);

  // ONLY_ONE_CONNECTED_BEFORE_TIMEOUT
  ASSERT_EQ(SignalStrategy::CONNECTING, state_history_[1]);

  // ALL_CONNECTED
  ASSERT_EQ(SignalStrategy::CONNECTED, state_history_[2]);
}

TEST_F(
    MuxingSignalStrategyTest,
    StateTransition_StartedConnectionBeforeTimeoutAndTheOtherStartedConnectionAfterTimeout) {
  xmpp_signal_strategy_->SetState(SignalStrategy::CONNECTING);
  xmpp_signal_strategy_->Connect();
  scoped_task_environment_.FastForwardBy(kWaitForAllStrategiesConnectedTimeout);
  ftl_signal_strategy_->SetState(SignalStrategy::CONNECTING);
  ftl_signal_strategy_->Connect();

  ASSERT_EQ(4u, state_history_.size());

  // SOME_CONNECTING
  ASSERT_EQ(SignalStrategy::CONNECTING, state_history_[0]);

  // ONLY_ONE_CONNECTED_BEFORE_TIMEOUT
  ASSERT_EQ(SignalStrategy::CONNECTING, state_history_[1]);

  // ONLY_ONE_CONNECTED_AFTER_TIMEOUT
  ASSERT_EQ(SignalStrategy::CONNECTED, state_history_[2]);

  // ALL_CONNECTED
  ASSERT_EQ(SignalStrategy::CONNECTED, state_history_[3]);
}

TEST_F(
    MuxingSignalStrategyTest,
    StateTransition_OneConnectedBeforeTimeoutAndTheOtherConnectedAfterTimeout) {
  xmpp_signal_strategy_->SetState(SignalStrategy::CONNECTING);
  ftl_signal_strategy_->SetState(SignalStrategy::CONNECTING);
  xmpp_signal_strategy_->Connect();
  scoped_task_environment_.FastForwardBy(kWaitForAllStrategiesConnectedTimeout);
  ftl_signal_strategy_->Connect();

  ASSERT_EQ(4u, state_history_.size());

  // SOME_CONNECTING
  ASSERT_EQ(SignalStrategy::CONNECTING, state_history_[0]);

  // ONLY_ONE_CONNECTED_BEFORE_TIMEOUT
  ASSERT_EQ(SignalStrategy::CONNECTING, state_history_[1]);

  // ONLY_ONE_CONNECTED_AFTER_TIMEOUT
  ASSERT_EQ(SignalStrategy::CONNECTED, state_history_[2]);

  // ALL_CONNECTED
  ASSERT_EQ(SignalStrategy::CONNECTED, state_history_[3]);
}

TEST_F(MuxingSignalStrategyTest, StateTransition_OneConnectedThenDisconnected) {
  xmpp_signal_strategy_->SetState(SignalStrategy::CONNECTING);
  xmpp_signal_strategy_->Connect();
  scoped_task_environment_.FastForwardBy(kWaitForAllStrategiesConnectedTimeout);
  xmpp_signal_strategy_->Disconnect();

  ASSERT_EQ(4u, state_history_.size());

  // SOME_CONNECTING
  ASSERT_EQ(SignalStrategy::CONNECTING, state_history_[0]);

  // ONLY_ONE_CONNECTED_BEFORE_TIMEOUT
  ASSERT_EQ(SignalStrategy::CONNECTING, state_history_[1]);

  // ONLY_ONE_CONNECTED_AFTER_TIMEOUT
  ASSERT_EQ(SignalStrategy::CONNECTED, state_history_[2]);

  // ALL_DISCONNECTED
  ASSERT_EQ(SignalStrategy::DISCONNECTED, state_history_[3]);
}

TEST_F(MuxingSignalStrategyTest,
       StateTransition_BothConnectedThenDisconnectedOneByOne) {
  xmpp_signal_strategy_->SetState(SignalStrategy::CONNECTING);
  ftl_signal_strategy_->SetState(SignalStrategy::CONNECTING);
  xmpp_signal_strategy_->Connect();
  ftl_signal_strategy_->Connect();
  scoped_task_environment_.FastForwardBy(kWaitForAllStrategiesConnectedTimeout);
  xmpp_signal_strategy_->Disconnect();
  ftl_signal_strategy_->Disconnect();

  ASSERT_EQ(5u, state_history_.size());

  // SOME_CONNECTING
  ASSERT_EQ(SignalStrategy::CONNECTING, state_history_[0]);

  // ONLY_ONE_CONNECTED_BEFORE_TIMEOUT
  ASSERT_EQ(SignalStrategy::CONNECTING, state_history_[1]);

  // ALL_CONNECTED
  ASSERT_EQ(SignalStrategy::CONNECTED, state_history_[2]);

  // ONLY_ONE_CONNECTED_AFTER_TIMEOUT
  ASSERT_EQ(SignalStrategy::CONNECTED, state_history_[3]);

  // ALL_DISCONNECTED
  ASSERT_EQ(SignalStrategy::DISCONNECTED, state_history_[4]);
}

TEST_F(MuxingSignalStrategyTest, SendStanza_MessageRoutedToFtlSignalStrategy) {
  xmpp_signal_strategy_->Connect();
  ftl_signal_strategy_->Connect();

  auto stanza = CreateXmlStanza(kLocalFtlId, kRemoteFtlId);
  std::string stanza_string = stanza->Str();
  EXPECT_CALL(mock_ftl_peer_callback_, Run(StanzaMatchesString(stanza_string)))
      .WillOnce(Return());
  muxing_signal_strategy_->SendStanza(std::move(stanza));
}

TEST_F(MuxingSignalStrategyTest, SendStanza_MessageRoutedToXmppSignalStrategy) {
  xmpp_signal_strategy_->Connect();
  ftl_signal_strategy_->Connect();

  auto stanza = CreateXmlStanza(kLocalJabberId, kRemoteJabberId);
  std::string stanza_string = stanza->Str();
  EXPECT_CALL(mock_xmpp_peer_callback_, Run(StanzaMatchesString(stanza_string)))
      .WillOnce(Return());
  muxing_signal_strategy_->SendStanza(std::move(stanza));
}

TEST_F(MuxingSignalStrategyTest,
       ReceiveStanza_MessagesFromBothStrategiesAreReceived) {
  xmpp_signal_strategy_->Connect();
  ftl_signal_strategy_->Connect();

  xmpp_signal_strategy_->OnIncomingMessage(
      CreateXmlStanza(kRemoteJabberId, kLocalJabberId));
  ftl_signal_strategy_->OnIncomingMessage(
      CreateXmlStanza(kRemoteFtlId, kLocalFtlId));

  ASSERT_EQ(2u, received_messages_.size());
  ASSERT_EQ(kRemoteJabberId, received_messages_[0]->Attr(jingle_xmpp::QN_FROM));
  ASSERT_EQ(kLocalJabberId, received_messages_[0]->Attr(jingle_xmpp::QN_TO));
  ASSERT_EQ(kRemoteFtlId, received_messages_[1]->Attr(jingle_xmpp::QN_FROM));
  ASSERT_EQ(kLocalFtlId, received_messages_[1]->Attr(jingle_xmpp::QN_TO));

  ASSERT_EQ(2u, received_stanza_local_addresses_.size());
  ASSERT_EQ(kLocalJabberId, received_stanza_local_addresses_[0].jid());
  ASSERT_EQ(kLocalFtlId, received_stanza_local_addresses_[1].jid());
}

}  // namespace remoting
