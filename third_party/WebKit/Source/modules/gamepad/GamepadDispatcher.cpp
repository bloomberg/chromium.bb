// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/gamepad/GamepadDispatcher.h"

#include "modules/gamepad/NavigatorGamepad.h"
#include "public/platform/Platform.h"

namespace blink {

GamepadDispatcher& GamepadDispatcher::Instance() {
  DEFINE_STATIC_LOCAL(GamepadDispatcher, gamepad_dispatcher,
                      (new GamepadDispatcher));
  return gamepad_dispatcher;
}

void GamepadDispatcher::SampleGamepads(WebGamepads& gamepads) {
  Platform::Current()->SampleGamepads(gamepads);
}

GamepadDispatcher::GamepadDispatcher() {}

GamepadDispatcher::~GamepadDispatcher() {}

DEFINE_TRACE(GamepadDispatcher) {
  PlatformEventDispatcher::Trace(visitor);
}

void GamepadDispatcher::DidConnectGamepad(unsigned index,
                                          const WebGamepad& gamepad) {
  DispatchDidConnectOrDisconnectGamepad(index, gamepad, true);
}

void GamepadDispatcher::DidDisconnectGamepad(unsigned index,
                                             const WebGamepad& gamepad) {
  DispatchDidConnectOrDisconnectGamepad(index, gamepad, false);
}

void GamepadDispatcher::DispatchDidConnectOrDisconnectGamepad(
    unsigned index,
    const WebGamepad& gamepad,
    bool connected) {
  ASSERT(index < WebGamepads::kItemsLengthCap);
  ASSERT(connected == gamepad.connected);

  latest_change_.pad = gamepad;
  latest_change_.index = index;
  NotifyControllers();
}

void GamepadDispatcher::StartListening() {
  Platform::Current()->StartListening(kWebPlatformEventTypeGamepad, this);
}

void GamepadDispatcher::StopListening() {
  Platform::Current()->StopListening(kWebPlatformEventTypeGamepad);
}

}  // namespace blink
