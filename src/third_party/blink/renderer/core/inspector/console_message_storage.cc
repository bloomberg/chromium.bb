// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/inspector/console_message_storage.h"

#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"

namespace blink {

static const unsigned kMaxConsoleMessageCount = 1000;

namespace {

const char* MessageSourceToString(MessageSource source) {
  switch (source) {
    case kXMLMessageSource:
      return "XML";
    case kJSMessageSource:
      return "JS";
    case kNetworkMessageSource:
      return "Network";
    case kConsoleAPIMessageSource:
      return "ConsoleAPI";
    case kStorageMessageSource:
      return "Storage";
    case kAppCacheMessageSource:
      return "AppCache";
    case kRenderingMessageSource:
      return "Rendering";
    case kSecurityMessageSource:
      return "Security";
    case kOtherMessageSource:
      return "Other";
    case kDeprecationMessageSource:
      return "Deprecation";
    case kWorkerMessageSource:
      return "Worker";
    case kViolationMessageSource:
      return "Violation";
    case kInterventionMessageSource:
      return "Intervention";
    case kRecommendationMessageSource:
      return "Recommendation";
  }
  LOG(FATAL) << "Unreachable code.";
  return nullptr;
}

void TraceConsoleMessageEvent(ConsoleMessage* message) {
  // Change in this function requires adjustment of Catapult/Telemetry metric
  // tracing/tracing/metrics/console_error_metric.html.
  // See https://crbug.com/880432
  if (message->Level() == kErrorMessageLevel) {
    TRACE_EVENT_INSTANT1("blink.console", "ConsoleMessage::Error",
                         TRACE_EVENT_SCOPE_THREAD, "source",
                         MessageSourceToString(message->Source()));
  }
}
}  // anonymous namespace

ConsoleMessageStorage::ConsoleMessageStorage() : expired_count_(0) {}

void ConsoleMessageStorage::AddConsoleMessage(ExecutionContext* context,
                                              ConsoleMessage* message) {
  TraceConsoleMessageEvent(message);
  probe::consoleMessageAdded(context, message);
  DCHECK(messages_.size() <= kMaxConsoleMessageCount);
  if (messages_.size() == kMaxConsoleMessageCount) {
    ++expired_count_;
    messages_.pop_front();
  }
  messages_.push_back(message);
}

void ConsoleMessageStorage::Clear() {
  messages_.clear();
  expired_count_ = 0;
}

wtf_size_t ConsoleMessageStorage::size() const {
  return messages_.size();
}

ConsoleMessage* ConsoleMessageStorage::at(wtf_size_t index) const {
  return messages_[index].Get();
}

int ConsoleMessageStorage::ExpiredCount() const {
  return expired_count_;
}

void ConsoleMessageStorage::Trace(blink::Visitor* visitor) {
  visitor->Trace(messages_);
}

}  // namespace blink
