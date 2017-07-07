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
#include "public/platform/modules/presentation/WebPresentationConnectionProxy.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "v8/include/v8.h"

namespace blink {

class MockEventListener : public EventListener {
 public:
  MockEventListener() : EventListener(kCPPEventListenerType) {}

  bool operator==(const EventListener& other) const final {
    return this == &other;
  }

  MOCK_METHOD2(handleEvent, void(ExecutionContext* executionContext, Event*));
};

class PresentationReceiverTest : public ::testing::Test {
 public:
  void AddConnectionavailableEventListener(EventListener*,
                                           const PresentationReceiver*);
  void VerifyConnectionListPropertyState(ScriptPromisePropertyBase::State,
                                         const PresentationReceiver*);
  void VerifyConnectionListSize(size_t expected_size,
                                const PresentationReceiver*);
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

  auto event_handler = new StrictMock<MockEventListener>();
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

  auto event_handler = new StrictMock<MockEventListener>();
  AddConnectionavailableEventListener(event_handler, receiver);
  EXPECT_CALL(*event_handler, handleEvent(::testing::_, ::testing::_)).Times(0);

  receiver->connectionList(scope.GetScriptState());

  // Receive first connection.
  receiver->OnReceiverConnectionAvailable(
      WebPresentationInfo(KURL(NullURL(), "http://example.com"), "id"));

  VerifyConnectionListPropertyState(ScriptPromisePropertyBase::kResolved,
                                    receiver);
  VerifyConnectionListSize(1, receiver);
}

TEST_F(PresentationReceiverTest, TwoConnectionsFireOnconnectionavailableEvent) {
  V8TestingScope scope;
  auto receiver = new PresentationReceiver(&scope.GetFrame(), nullptr);

  StrictMock<MockEventListener>* event_handler =
      new StrictMock<MockEventListener>();
  AddConnectionavailableEventListener(event_handler, receiver);
  EXPECT_CALL(*event_handler, handleEvent(::testing::_, ::testing::_)).Times(1);

  receiver->connectionList(scope.GetScriptState());

  WebPresentationInfo presentation_info(KURL(NullURL(), "http://example.com"),
                                        "id");
  // Receive first connection.
  receiver->OnReceiverConnectionAvailable(presentation_info);
  // Receive second connection.
  receiver->OnReceiverConnectionAvailable(presentation_info);

  VerifyConnectionListSize(2, receiver);
}

TEST_F(PresentationReceiverTest, TwoConnectionsNoEvent) {
  V8TestingScope scope;
  auto receiver = new PresentationReceiver(&scope.GetFrame(), nullptr);

  StrictMock<MockEventListener>* event_handler =
      new StrictMock<MockEventListener>();
  AddConnectionavailableEventListener(event_handler, receiver);
  EXPECT_CALL(*event_handler, handleEvent(::testing::_, ::testing::_)).Times(0);

  WebPresentationInfo presentation_info(KURL(NullURL(), "http://example.com"),
                                        "id");
  // Receive first connection.
  auto* connection1 =
      receiver->OnReceiverConnectionAvailable(presentation_info);
  EXPECT_TRUE(connection1);

  // Receive second connection.
  auto* connection2 =
      receiver->OnReceiverConnectionAvailable(presentation_info);
  EXPECT_TRUE(connection2);

  receiver->connectionList(scope.GetScriptState());
  VerifyConnectionListPropertyState(ScriptPromisePropertyBase::kResolved,
                                    receiver);
  VerifyConnectionListSize(2, receiver);
}

TEST_F(PresentationReceiverTest, CreateReceiver) {
  MockWebPresentationClient client;
  EXPECT_CALL(client, SetReceiver(::testing::_));

  V8TestingScope scope;
  new PresentationReceiver(&scope.GetFrame(), &client);
}

TEST_F(PresentationReceiverTest, TestRemoveConnection) {
  V8TestingScope scope;
  auto receiver = new PresentationReceiver(&scope.GetFrame(), nullptr);

  // Receive first connection.
  WebPresentationInfo presentation_info1(KURL(NullURL(), "http://example1.com"),
                                         "id1");
  auto* connection1 =
      receiver->OnReceiverConnectionAvailable(presentation_info1);
  EXPECT_TRUE(connection1);

  // Receive second connection.
  WebPresentationInfo presentation_info2(KURL(NullURL(), "http://example2.com"),
                                         "id2");
  auto* connection2 =
      receiver->OnReceiverConnectionAvailable(presentation_info2);
  EXPECT_TRUE(connection2);

  receiver->connectionList(scope.GetScriptState());
  VerifyConnectionListSize(2, receiver);

  receiver->RemoveConnection(connection1);
  VerifyConnectionListSize(1, receiver);
}

}  // namespace blink
