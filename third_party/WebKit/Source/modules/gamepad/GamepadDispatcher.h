// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GamepadDispatcher_h
#define GamepadDispatcher_h

#include "core/frame/PlatformEventDispatcher.h"
#include "platform/heap/Handle.h"
#include "public/platform/WebGamepad.h"
#include "public/platform/WebGamepadListener.h"

namespace blink {

class WebGamepads;

class GamepadDispatcher final
    : public GarbageCollectedFinalized<GamepadDispatcher>,
      public PlatformEventDispatcher,
      public WebGamepadListener {
  USING_GARBAGE_COLLECTED_MIXIN(GamepadDispatcher);

 public:
  static GamepadDispatcher& Instance();
  ~GamepadDispatcher() override;

  void SampleGamepads(WebGamepads&);

  struct ConnectionChange {
    DISALLOW_NEW();
    WebGamepad pad;
    unsigned index;
  };

  const ConnectionChange& LatestConnectionChange() const {
    return latest_change_;
  }

  DECLARE_VIRTUAL_TRACE();

 private:
  GamepadDispatcher();

  // WebGamepadListener
  void DidConnectGamepad(unsigned index, const WebGamepad&) override;
  void DidDisconnectGamepad(unsigned index, const WebGamepad&) override;

  // PlatformEventDispatcher
  void StartListening() override;
  void StopListening() override;

  void DispatchDidConnectOrDisconnectGamepad(unsigned index,
                                             const WebGamepad&,
                                             bool connected);

  ConnectionChange latest_change_;
};

}  // namespace blink

#endif
