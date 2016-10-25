// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/remoteplayback/RemotePlayback.h"

#include "bindings/core/v8/ExceptionStatePlaceholder.h"
#include "bindings/core/v8/V8BindingForTesting.h"
#include "bindings/modules/v8/RemotePlaybackAvailabilityCallback.h"
#include "core/dom/DocumentUserGestureToken.h"
#include "core/html/HTMLMediaElement.h"
#include "core/html/HTMLVideoElement.h"
#include "core/testing/DummyPageHolder.h"
#include "modules/remoteplayback/HTMLMediaElementRemotePlayback.h"
#include "platform/UserGestureIndicator.h"
#include "platform/testing/UnitTestHelpers.h"
#include "public/platform/modules/remoteplayback/WebRemotePlaybackState.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class MockFunction : public ScriptFunction {
 public:
  static MockFunction* create(ScriptState* scriptState) {
    return new MockFunction(scriptState);
  }

  v8::Local<v8::Function> bind() { return bindToV8Function(); }

  MOCK_METHOD1(call, ScriptValue(ScriptValue));

 private:
  explicit MockFunction(ScriptState* scriptState)
      : ScriptFunction(scriptState) {}
};

class MockEventListener : public EventListener {
 public:
  MockEventListener() : EventListener(CPPEventListenerType) {}

  bool operator==(const EventListener& other) const final {
    return this == &other;
  }

  MOCK_METHOD2(handleEvent, void(ExecutionContext* executionContext, Event*));
};

class RemotePlaybackTest : public ::testing::Test {
 protected:
  void cancelPrompt(RemotePlayback* remotePlayback) {
    remotePlayback->promptCancelled();
  }

  void setState(RemotePlayback* remotePlayback, WebRemotePlaybackState state) {
    remotePlayback->stateChanged(state);
  }

  void setAvailability(RemotePlayback* remotePlayback, bool available) {
    remotePlayback->availabilityChanged(available);
  }
};

TEST_F(RemotePlaybackTest, PromptCancelledRejectsWithNotAllowedError) {
  V8TestingScope scope;

  auto pageHolder = DummyPageHolder::create();

  HTMLMediaElement* element = HTMLVideoElement::create(pageHolder->document());
  RemotePlayback* remotePlayback =
      HTMLMediaElementRemotePlayback::remote(scope.getScriptState(), *element);

  MockFunction* resolve = MockFunction::create(scope.getScriptState());
  MockFunction* reject = MockFunction::create(scope.getScriptState());

  EXPECT_CALL(*resolve, call(::testing::_)).Times(0);
  EXPECT_CALL(*reject, call(::testing::_)).Times(1);

  UserGestureIndicator indicator(DocumentUserGestureToken::create(
      &pageHolder->document(), UserGestureToken::NewGesture));
  remotePlayback->prompt().then(resolve->bind(), reject->bind());
  cancelPrompt(remotePlayback);
}

TEST_F(RemotePlaybackTest, StateChangeEvents) {
  V8TestingScope scope;

  auto pageHolder = DummyPageHolder::create();

  HTMLMediaElement* element = HTMLVideoElement::create(pageHolder->document());
  RemotePlayback* remotePlayback =
      HTMLMediaElementRemotePlayback::remote(scope.getScriptState(), *element);

  auto connectingHandler = new ::testing::StrictMock<MockEventListener>();
  auto connectHandler = new ::testing::StrictMock<MockEventListener>();
  auto disconnectHandler = new ::testing::StrictMock<MockEventListener>();

  remotePlayback->addEventListener(EventTypeNames::connecting,
                                   connectingHandler);
  remotePlayback->addEventListener(EventTypeNames::connect, connectHandler);
  remotePlayback->addEventListener(EventTypeNames::disconnect,
                                   disconnectHandler);

  EXPECT_CALL(*connectingHandler, handleEvent(::testing::_, ::testing::_))
      .Times(1);
  EXPECT_CALL(*connectHandler, handleEvent(::testing::_, ::testing::_))
      .Times(1);
  EXPECT_CALL(*disconnectHandler, handleEvent(::testing::_, ::testing::_))
      .Times(1);

  setState(remotePlayback, WebRemotePlaybackState::Connecting);
  setState(remotePlayback, WebRemotePlaybackState::Connecting);
  setState(remotePlayback, WebRemotePlaybackState::Connected);
  setState(remotePlayback, WebRemotePlaybackState::Connected);
  setState(remotePlayback, WebRemotePlaybackState::Disconnected);
  setState(remotePlayback, WebRemotePlaybackState::Disconnected);
}

TEST_F(RemotePlaybackTest,
       DisableRemotePlaybackRejectsPromptWithInvalidStateError) {
  V8TestingScope scope;

  auto pageHolder = DummyPageHolder::create();

  HTMLMediaElement* element = HTMLVideoElement::create(pageHolder->document());
  RemotePlayback* remotePlayback =
      HTMLMediaElementRemotePlayback::remote(scope.getScriptState(), *element);

  MockFunction* resolve = MockFunction::create(scope.getScriptState());
  MockFunction* reject = MockFunction::create(scope.getScriptState());

  EXPECT_CALL(*resolve, call(::testing::_)).Times(0);
  EXPECT_CALL(*reject, call(::testing::_)).Times(1);

  UserGestureIndicator indicator(DocumentUserGestureToken::create(
      &pageHolder->document(), UserGestureToken::NewGesture));
  remotePlayback->prompt().then(resolve->bind(), reject->bind());
  HTMLMediaElementRemotePlayback::setBooleanAttribute(
      HTMLNames::disableremoteplaybackAttr, *element, true);
}

TEST_F(RemotePlaybackTest, DisableRemotePlaybackCancelsAvailabilityCallbacks) {
  V8TestingScope scope;

  auto pageHolder = DummyPageHolder::create();

  HTMLMediaElement* element = HTMLVideoElement::create(pageHolder->document());
  RemotePlayback* remotePlayback =
      HTMLMediaElementRemotePlayback::remote(scope.getScriptState(), *element);

  MockFunction* callbackFunction = MockFunction::create(scope.getScriptState());
  RemotePlaybackAvailabilityCallback* availabilityCallback =
      RemotePlaybackAvailabilityCallback::create(scope.isolate(),
                                                 callbackFunction->bind());

  // The initial call upon registering will not happen as it's posted on the
  // message loop.
  EXPECT_CALL(*callbackFunction, call(::testing::_)).Times(0);

  MockFunction* watchResolve = MockFunction::create(scope.getScriptState());
  MockFunction* watchReject = MockFunction::create(scope.getScriptState());

  EXPECT_CALL(*watchResolve, call(::testing::_)).Times(1);
  EXPECT_CALL(*watchReject, call(::testing::_)).Times(0);

  remotePlayback->watchAvailability(availabilityCallback)
      .then(watchResolve->bind(), watchReject->bind());

  HTMLMediaElementRemotePlayback::setBooleanAttribute(
      HTMLNames::disableremoteplaybackAttr, *element, true);
  setAvailability(remotePlayback, true);
}

}  // namespace blink
