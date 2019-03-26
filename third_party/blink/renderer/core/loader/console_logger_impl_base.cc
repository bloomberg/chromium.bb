// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/console_logger_impl_base.h"

#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"

namespace blink {

ConsoleLoggerImplBase::ConsoleLoggerImplBase() = default;
ConsoleLoggerImplBase::~ConsoleLoggerImplBase() = default;

void ConsoleLoggerImplBase::AddInfoMessage(Source source,
                                           const String& message) {
  AddConsoleMessage(ConsoleMessage::Create(GetMessageSourceFromSource(source),
                                           mojom::ConsoleMessageLevel::kInfo,
                                           message));
}

void ConsoleLoggerImplBase::AddWarningMessage(Source source,
                                              const String& message) {
  AddConsoleMessage(ConsoleMessage::Create(GetMessageSourceFromSource(source),
                                           mojom::ConsoleMessageLevel::kWarning,
                                           message));
}

void ConsoleLoggerImplBase::AddErrorMessage(Source source,
                                            const String& message) {
  AddConsoleMessage(ConsoleMessage::Create(GetMessageSourceFromSource(source),
                                           mojom::ConsoleMessageLevel::kError,
                                           message));
}

mojom::ConsoleMessageSource ConsoleLoggerImplBase::GetMessageSourceFromSource(
    Source source) {
  switch (source) {
    case Source::kScript:
      return mojom::ConsoleMessageSource::kJavaScript;
    case Source::kSecurity:
      return mojom::ConsoleMessageSource::kSecurity;
    case Source::kOther:
      return mojom::ConsoleMessageSource::kOther;
  }
  NOTREACHED();
  return mojom::ConsoleMessageSource::kOther;
}

}  // namespace blink
