// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/mediasession/NavigatorMediaSession.h"

#include "core/dom/ExecutionContext.h"
#include "modules/mediasession/MediaSession.h"
#include "platform/Supplementable.h"
#include "platform/bindings/ScriptState.h"

namespace blink {

NavigatorMediaSession::NavigatorMediaSession(Navigator& navigator)
    : Supplement<Navigator>(navigator) {}

void NavigatorMediaSession::Trace(blink::Visitor* visitor) {
  visitor->Trace(session_);
  Supplement<Navigator>::Trace(visitor);
}

const char* NavigatorMediaSession::SupplementName() {
  return "NavigatorMediaSession";
}

NavigatorMediaSession& NavigatorMediaSession::From(Navigator& navigator) {
  NavigatorMediaSession* supplement = static_cast<NavigatorMediaSession*>(
      Supplement<Navigator>::From(navigator, SupplementName()));
  if (!supplement) {
    supplement = new NavigatorMediaSession(navigator);
    ProvideTo(navigator, SupplementName(), supplement);
  }
  return *supplement;
}

MediaSession* NavigatorMediaSession::mediaSession(ScriptState* script_state,
                                                  Navigator& navigator) {
  NavigatorMediaSession& self = NavigatorMediaSession::From(navigator);
  if (!self.session_)
    self.session_ = MediaSession::Create(ExecutionContext::From(script_state));
  return self.session_.Get();
}

}  // namespace blink
