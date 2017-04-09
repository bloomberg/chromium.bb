// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/websockets/InspectorWebSocketEvents.h"

#include "core/dom/Document.h"
#include "platform/weborigin/KURL.h"
#include <memory>

namespace blink {

std::unique_ptr<TracedValue> InspectorWebSocketCreateEvent::Data(
    Document* document,
    unsigned long identifier,
    const KURL& url,
    const String& protocol) {
  std::unique_ptr<TracedValue> value = TracedValue::Create();
  value->SetInteger("identifier", identifier);
  value->SetString("url", url.GetString());
  value->SetString("frame", ToHexString(document->GetFrame()));
  if (!protocol.IsNull())
    value->SetString("webSocketProtocol", protocol);
  SetCallStack(value.get());
  return value;
}

std::unique_ptr<TracedValue> InspectorWebSocketEvent::Data(
    Document* document,
    unsigned long identifier) {
  std::unique_ptr<TracedValue> value = TracedValue::Create();
  value->SetInteger("identifier", identifier);
  value->SetString("frame", ToHexString(document->GetFrame()));
  SetCallStack(value.get());
  return value;
}

}  // namespace blink
