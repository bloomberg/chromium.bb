// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/inspector/ConsoleMessage.h"

#include "bindings/core/v8/SourceLocation.h"
#include "platform/wtf/CurrentTime.h"

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
      timestamp_(WTF::CurrentTimeMS()) {}

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

DEFINE_TRACE(ConsoleMessage) {}

}  // namespace blink
