// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/push_messaging/PushController.h"

#include "platform/wtf/Assertions.h"
#include "public/platform/modules/push_messaging/WebPushClient.h"

namespace blink {

PushController::PushController(LocalFrame& frame, WebPushClient* client)
    : Supplement<LocalFrame>(frame), client_(client) {}

WebPushClient& PushController::ClientFrom(LocalFrame* frame) {
  PushController* controller = PushController::From(frame);
  DCHECK(controller);
  WebPushClient* client = controller->Client();
  DCHECK(client);
  return *client;
}

const char* PushController::SupplementName() {
  return "PushController";
}

void ProvidePushControllerTo(LocalFrame& frame, WebPushClient* client) {
  PushController::ProvideTo(frame, PushController::SupplementName(),
                            new PushController(frame, client));
}

}  // namespace blink
