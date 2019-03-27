// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_CONSOLE_LOGGER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_CONSOLE_LOGGER_H_

#include "third_party/blink/public/mojom/devtools/console_message.mojom-shared.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"

namespace blink {

// A pure virtual interface for console logging.
class PLATFORM_EXPORT ConsoleLogger : public GarbageCollectedMixin {
 public:
  ConsoleLogger() = default;
  virtual ~ConsoleLogger() = default;

  virtual void AddConsoleMessage(mojom::ConsoleMessageSource,
                                 mojom::ConsoleMessageLevel,
                                 const String& message) = 0;
};

class PLATFORM_EXPORT NullConsoleLogger final
    : public GarbageCollected<NullConsoleLogger>,
      public ConsoleLogger {
  USING_GARBAGE_COLLECTED_MIXIN(NullConsoleLogger);

 public:
  NullConsoleLogger() = default;
  ~NullConsoleLogger() override = default;

  void AddConsoleMessage(mojom::ConsoleMessageSource,
                         mojom::ConsoleMessageLevel,
                         const String&) override {}
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_CONSOLE_LOGGER_H_
