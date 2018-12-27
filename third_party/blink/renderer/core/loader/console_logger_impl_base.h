// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_CONSOLE_LOGGER_IMPL_BASE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_CONSOLE_LOGGER_IMPL_BASE_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/inspector/console_types.h"
#include "third_party/blink/renderer/platform/loader/fetch/console_logger.h"

namespace blink {

class ConsoleMessage;

// A bridge class which translates an Add(Info|Warning|Error)Message call to
// an AddMessage call.
class CORE_EXPORT ConsoleLoggerImplBase : public ConsoleLogger {
 public:
  ConsoleLoggerImplBase();
  ~ConsoleLoggerImplBase() override;

  // ConsoleLogger implementation.
  void AddInfoMessage(Source source, const String& message) override;
  void AddWarningMessage(Source source, const String& message) override;
  void AddErrorMessage(Source source, const String& message) override;

  virtual void AddConsoleMessage(ConsoleMessage*) = 0;

 private:
  static MessageSource GetMessageSourceFromSource(Source);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_CONSOLE_LOGGER_IMPL_BASE_H_
