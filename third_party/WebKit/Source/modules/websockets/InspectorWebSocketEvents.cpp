// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/websockets/InspectorWebSocketEvents.h"

#include <memory>
#include "core/dom/Document.h"
#include "core/dom/ExecutionContext.h"
#include "core/frame/LocalFrame.h"
#include "core/inspector/IdentifiersFactory.h"
#include "platform/weborigin/KURL.h"

namespace blink {

std::unique_ptr<TracedValue> InspectorWebSocketCreateEvent::Data(
    ExecutionContext* execution_context,
    unsigned long identifier,
    const KURL& url,
    const String& protocol) {
  std::unique_ptr<TracedValue> value = TracedValue::Create();
  value->SetInteger("identifier", identifier);
  value->SetString("url", url.GetString());
  if (execution_context->IsDocument()) {
    value->SetString("frame", IdentifiersFactory::FrameId(
                                  ToDocument(execution_context)->GetFrame()));
  } else {
    // TODO(nhiroki): Support WorkerGlobalScope (https://crbug.com/825740).
    NOTREACHED();
  }
  if (!protocol.IsNull())
    value->SetString("webSocketProtocol", protocol);
  SetCallStack(value.get());
  return value;
}

std::unique_ptr<TracedValue> InspectorWebSocketEvent::Data(
    ExecutionContext* execution_context,
    unsigned long identifier) {
  std::unique_ptr<TracedValue> value = TracedValue::Create();
  value->SetInteger("identifier", identifier);
  if (execution_context->IsDocument()) {
    value->SetString("frame", IdentifiersFactory::FrameId(
                                  ToDocument(execution_context)->GetFrame()));
  } else {
    // TODO(nhiroki): Support WorkerGlobalScope (https://crbug.com/825740).
    NOTREACHED();
  }
  SetCallStack(value.get());
  return value;
}

}  // namespace blink
