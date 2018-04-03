// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/mediasession/NavigatorMediaSession.h"

#include "core/execution_context/ExecutionContext.h"
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

const char NavigatorMediaSession::kSupplementName[] = "NavigatorMediaSession";

NavigatorMediaSession& NavigatorMediaSession::From(Navigator& navigator) {
  NavigatorMediaSession* supplement =
      Supplement<Navigator>::From<NavigatorMediaSession>(navigator);
  if (!supplement) {
    supplement = new NavigatorMediaSession(navigator);
    ProvideTo(navigator, supplement);
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
