// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/inspector/ConsoleMessage.h"

#include "bindings/core/v8/SourceLocation.h"
#include "core/dom/Node.h"
#include "core/frame/LocalFrame.h"
#include "platform/wtf/Assertions.h"
#include "platform/wtf/CurrentTime.h"
#include "public/web/WebConsoleMessage.h"

namespace blink {

// static
ConsoleMessage* ConsoleMessage::CreateForRequest(
    MessageSource source,
    MessageLevel level,
    const String& message,
    const String& url,
    unsigned long request_identifier) {
  ConsoleMessage* console_message = ConsoleMessage::Create(
      source, level, message, SourceLocation::Capture(url, 0, 0));
  console_message->request_identifier_ = request_identifier;
  return console_message;
}

// static
ConsoleMessage* ConsoleMessage::Create(
    MessageSource source,
    MessageLevel level,
    const String& message,
    std::unique_ptr<SourceLocation> location) {
  return new ConsoleMessage(source, level, message, std::move(location));
}

// static
ConsoleMessage* ConsoleMessage::Create(MessageSource source,
                                       MessageLevel level,
                                       const String& message) {
  return ConsoleMessage::Create(source, level, message,
                                SourceLocation::Capture());
}

// static
ConsoleMessage* ConsoleMessage::CreateFromWorker(
    MessageLevel level,
    const String& message,
    std::unique_ptr<SourceLocation> location,
    const String& worker_id) {
  ConsoleMessage* console_message = ConsoleMessage::Create(
      kWorkerMessageSource, level, message, std::move(location));
  console_message->worker_id_ = worker_id;
  return console_message;
}

ConsoleMessage::ConsoleMessage(MessageSource source,
                               MessageLevel level,
                               const String& message,
                               std::unique_ptr<SourceLocation> location)
    : source_(source),
      level_(level),
      message_(message),
      location_(std::move(location)),
      request_identifier_(0),
      timestamp_(WTF::CurrentTimeMS()),
      frame_(nullptr) {}

ConsoleMessage::~ConsoleMessage() {}

SourceLocation* ConsoleMessage::Location() const {
  return location_.get();
}

unsigned long ConsoleMessage::RequestIdentifier() const {
  return request_identifier_;
}

double ConsoleMessage::Timestamp() const {
  return timestamp_;
}

MessageSource ConsoleMessage::Source() const {
  return source_;
}

MessageLevel ConsoleMessage::Level() const {
  return level_;
}

const String& ConsoleMessage::Message() const {
  return message_;
}

const String& ConsoleMessage::WorkerId() const {
  return worker_id_;
}

LocalFrame* ConsoleMessage::Frame() const {
  return frame_;
}

Vector<DOMNodeId>& ConsoleMessage::Nodes() {
  return nodes_;
}

void ConsoleMessage::SetNodes(LocalFrame* frame, Vector<DOMNodeId> nodes) {
  frame_ = frame;
  nodes_ = std::move(nodes);
}

void ConsoleMessage::Trace(blink::Visitor* visitor) {
  visitor->Trace(frame_);
}

STATIC_ASSERT_ENUM(WebConsoleMessage::kLevelVerbose, kVerboseMessageLevel);
STATIC_ASSERT_ENUM(WebConsoleMessage::kLevelInfo, kInfoMessageLevel);
STATIC_ASSERT_ENUM(WebConsoleMessage::kLevelWarning, kWarningMessageLevel);
STATIC_ASSERT_ENUM(WebConsoleMessage::kLevelError, kErrorMessageLevel);

}  // namespace blink
