// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/presentation/PresentationReceiver.h"

#include <memory>

#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "bindings/core/v8/V8BindingForTesting.h"
#include "core/frame/LocalFrame.h"
#include "core/testing/DummyPageHolder.h"
#include "modules/presentation/MockWebPresentationClient.h"
#include "modules/presentation/PresentationConnection.h"
#include "modules/presentation/PresentationConnectionList.h"
#include "platform/testing/URLTestHelpers.h"
#include "public/platform/modules/presentation/WebPresentationClient.h"
#include "public/platform/modules/presentation/WebPresentationConnectionCallbacks.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "v8/include/v8.h"

namespace blink {

class MockEventListenerForPresentationReceiver : public EventListener {
 public:
  MockEventListenerForPresentationReceiver()
      : EventListener(kCPPEventListenerType) {}

  bool operator==(const EventListener& other) const final {
    return this == &other;
  }

  MOCK_METHOD2(handleEvent, void(ExecutionContext* executionContext, Event*));
};

class PresentationReceiverTest : public ::testing::Test {
 public:
  PresentationReceiverTest()
      : connection_info_(KURL(NullURL(), "http://example.com"), "id") {}
  void AddConnectionavailableEventListener(EventListener*,
                                           const PresentationReceiver*);
  void VerifyConnectionListPropertyState(ScriptPromisePropertyBase::State,
                                         const PresentationReceiver*);
  void VerifyConnectionListSize(size_t expected_size,
                                const PresentationReceiver*);

 protected:
  void SetUp() override {
    controller_connection_request_ = mojo::MakeRequest(&controller_connection_);
    receiver_connection_request_ = mojo::MakeRequest(&receiver_connection_);
  }

  mojom::blink::PresentationInfo connection_info_;
  mojom::blink::PresentationConnectionRequest controller_connection_request_;
  mojom::blink::PresentationConnectionPtr controller_connection_;
  mojom::blink::PresentationConnectionRequest receiver_connection_request_;
  mojom::blink::PresentationConnectionPtr receiver_connection_;
};

void PresentationReceiverTest::AddConnectionavailableEventListener(
    EventListener* event_handler,
    const PresentationReceiver* receiver) {
  receiver->connection_list_->addEventListener(
      EventTypeNames::connectionavailable, event_handler);
}

void PresentationReceiverTest::VerifyConnectionListPropertyState(
    ScriptPromisePropertyBase::State expected_state,
    const PresentationReceiver* receiver) {
  EXPECT_EQ(expected_state, receiver->connection_list_property_->GetState());
}

void PresentationReceiverTest::VerifyConnectionListSize(
    size_t expected_size,
    const PresentationReceiver* receiver) {
  EXPECT_EQ(expected_size, receiver->connection_list_->connections_.size());
}

using ::testing::StrictMock;

TEST_F(PresentationReceiverTest, NoConnectionUnresolvedConnectionList) {
  V8TestingScope scope;
  auto receiver = new PresentationReceiver(&scope.GetFrame(), nullptr);

  auto event_handler =
      new StrictMock<MockEventListenerForPresentationReceiver>();
  AddConnectionavailableEventListener(event_handler, receiver);
  EXPECT_CALL(*event_handler, handleEvent(::testing::_, ::testing::_)).Times(0);

  receiver->connectionList(scope.GetScriptState());

  VerifyConnectionListPropertyState(ScriptPromisePropertyBase::kPending,
                                    receiver);
  VerifyConnectionListSize(0, receiver);
}

TEST_F(PresentationReceiverTest, OneConnectionResolvedConnectionListNoEvent) {
  V8TestingScope scope;
  auto receiver = new PresentationReceiver(&scope.GetFrame(), nullptr);

  auto event_handler =
      new StrictMock<MockEventListenerForPresentationReceiver>();
  AddConnectionavailableEventListener(event_handler, receiver);
  EXPECT_CALL(*event_handler, handleEvent(::testing::_, ::testing::_)).Times(0);

  receiver->connectionList(scope.GetScriptState());

  // Receive first connection.
  receiver->OnReceiverConnectionAvailable(
      connection_info_.Clone(), std::move(controller_connection_),
      std::move(receiver_connection_request_));

  VerifyConnectionListPropertyState(ScriptPromisePropertyBase::kResolved,
                                    receiver);
  VerifyConnectionListSize(1, receiver);
}

TEST_F(PresentationReceiverTest, TwoConnectionsFireOnconnectionavailableEvent) {
  V8TestingScope scope;
  auto receiver = new PresentationReceiver(&scope.GetFrame(), nullptr);

  StrictMock<MockEventListenerForPresentationReceiver>* event_handler =
      new StrictMock<MockEventListenerForPresentationReceiver>();
  AddConnectionavailableEventListener(event_handler, receiver);
  EXPECT_CALL(*event_handler, handleEvent(::testing::_, ::testing::_)).Times(1);

  receiver->connectionList(scope.GetScriptState());

  // Receive first connection.
  receiver->OnReceiverConnectionAvailable(
      connection_info_.Clone(), std::move(controller_connection_),
      std::move(receiver_connection_request_));

  mojom::blink::PresentationConnectionPtr controller_connection_2;
  mojom::blink::PresentationConnectionPtr receiver_connection_2;
  mojom::blink::PresentationConnectionRequest controller_connection_request_2 =
      mojo::MakeRequest(&controller_connection_2);
  mojom::blink::PresentationConnectionRequest receiver_connection_request_2 =
      mojo::MakeRequest(&receiver_connection_2);

  // Receive second connection.
  receiver->OnReceiverConnectionAvailable(
      connection_info_.Clone(), std::move(controller_connection_2),
      std::move(receiver_connection_request_2));

  VerifyConnectionListSize(2, receiver);
}

TEST_F(PresentationReceiverTest, TwoConnectionsNoEvent) {
  V8TestingScope scope;
  auto receiver = new PresentationReceiver(&scope.GetFrame(), nullptr);

  StrictMock<MockEventListenerForPresentationReceiver>* event_handler =
      new StrictMock<MockEventListenerForPresentationReceiver>();
  AddConnectionavailableEventListener(event_handler, receiver);
  EXPECT_CALL(*event_handler, handleEvent(::testing::_, ::testing::_)).Times(0);

  // Receive first connection.
  receiver->OnReceiverConnectionAvailable(
      connection_info_.Clone(), std::move(controller_connection_),
      std::move(receiver_connection_request_));

  mojom::blink::PresentationConnectionPtr controller_connection_2;
  mojom::blink::PresentationConnectionPtr receiver_connection_2;
  mojom::blink::PresentationConnectionRequest controller_connection_request_2 =
      mojo::MakeRequest(&controller_connection_2);
  mojom::blink::PresentationConnectionRequest receiver_connection_request_2 =
      mojo::MakeRequest(&receiver_connection_2);

  // Receive second connection.
  receiver->OnReceiverConnectionAvailable(
      connection_info_.Clone(), std::move(controller_connection_2),
      std::move(receiver_connection_request_2));

  receiver->connectionList(scope.GetScriptState());
  VerifyConnectionListPropertyState(ScriptPromisePropertyBase::kResolved,
                                    receiver);
  VerifyConnectionListSize(2, receiver);
}

TEST_F(PresentationReceiverTest, CreateReceiver) {
  MockWebPresentationClient client;
  EXPECT_CALL(client, SetReceiver(::testing::NotNull()));

  V8TestingScope scope;
  new PresentationReceiver(&scope.GetFrame(), &client);
  EXPECT_TRUE(::testing::Mock::VerifyAndClearExpectations(&client));

  EXPECT_CALL(client, SetReceiver(::testing::IsNull()));
}

}  // namespace blink
