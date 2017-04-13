// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/presentation/PresentationReceiver.h"

#include <memory>

#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "bindings/core/v8/V8BindingForTesting.h"
#include "core/frame/LocalFrame.h"
#include "core/testing/DummyPageHolder.h"
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

class MockWebPresentationClient : public WebPresentationClient {
  void StartPresentation(
      const WebVector<WebURL>& presentation_urls,
      std::unique_ptr<WebPresentationConnectionCallbacks> callbacks) override {
    return startPresentation_(presentation_urls, callbacks);
  }
  void ReconnectPresentation(
      const WebVector<WebURL>& presentation_urls,
      const WebString& presentation_id,
      std::unique_ptr<WebPresentationConnectionCallbacks> callbacks) override {
    return reconnectPresentation_(presentation_urls, presentation_id,
                                  callbacks);
  }

  void GetAvailability(const WebVector<WebURL>& availability_urls,
                       std::unique_ptr<WebPresentationAvailabilityCallbacks>
                           callbacks) override {
    return getAvailability_(availability_urls, callbacks);
  }

 public:
  MOCK_METHOD1(SetController, void(WebPresentationController*));

  MOCK_METHOD1(SetReceiver, void(WebPresentationReceiver*));

  MOCK_METHOD2(startPresentation_,
               void(const WebVector<WebURL>& presentationUrls,
                    std::unique_ptr<WebPresentationConnectionCallbacks>&));

  MOCK_METHOD3(reconnectPresentation_,
               void(const WebVector<WebURL>& presentationUrls,
                    const WebString& presentationId,
                    std::unique_ptr<WebPresentationConnectionCallbacks>&));

  MOCK_METHOD4(SendString,
               void(const WebURL& presentationUrl,
                    const WebString& presentationId,
                    const WebString& message,
                    const WebPresentationConnectionProxy* proxy));

  MOCK_METHOD5(SendArrayBuffer,
               void(const WebURL& presentationUrl,
                    const WebString& presentationId,
                    const uint8_t* data,
                    size_t length,
                    const WebPresentationConnectionProxy* proxy));

  MOCK_METHOD5(SendBlobData,
               void(const WebURL& presentationUrl,
                    const WebString& presentationId,
                    const uint8_t* data,
                    size_t length,
                    const WebPresentationConnectionProxy* proxy));

  MOCK_METHOD3(CloseConnection,
               void(const WebURL& presentationUrl,
                    const WebString& presentationId,
                    const WebPresentationConnectionProxy*));

  MOCK_METHOD2(TerminatePresentation,
               void(const WebURL& presentationUrl,
                    const WebString& presentationId));

  MOCK_METHOD2(getAvailability_,
               void(const WebVector<WebURL>& availabilityUrls,
                    std::unique_ptr<WebPresentationAvailabilityCallbacks>&));

  MOCK_METHOD1(StartListening, void(WebPresentationAvailabilityObserver*));

  MOCK_METHOD1(StopListening, void(WebPresentationAvailabilityObserver*));

  MOCK_METHOD1(SetDefaultPresentationUrls, void(const WebVector<WebURL>&));
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
  EXPECT_CALL(*event_handler, handleEvent(testing::_, testing::_)).Times(0);

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
  EXPECT_CALL(*event_handler, handleEvent(testing::_, testing::_)).Times(0);

  receiver->connectionList(scope.GetScriptState());

  // Receive first connection.
  receiver->OnReceiverConnectionAvailable(
      WebPresentationInfo(KURL(KURL(), "http://example.com"), "id"));

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
  EXPECT_CALL(*event_handler, handleEvent(testing::_, testing::_)).Times(1);

  receiver->connectionList(scope.GetScriptState());

  WebPresentationInfo presentation_info(KURL(KURL(), "http://example.com"),
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
  EXPECT_CALL(*event_handler, handleEvent(testing::_, testing::_)).Times(0);

  WebPresentationInfo presentation_info(KURL(KURL(), "http://example.com"),
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
  EXPECT_CALL(client, SetReceiver(testing::_));

  V8TestingScope scope;
  new PresentationReceiver(&scope.GetFrame(), &client);
}

}  // namespace blink
