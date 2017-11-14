// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EventModulesFactory_h
#define EventModulesFactory_h

#include <memory>
#include "core/events/EventFactory.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/PtrUtil.h"
#include "platform/wtf/RefPtr.h"
#include "platform/wtf/text/AtomicString.h"

namespace blink {

class Event;

class EventModulesFactory final : public EventFactoryBase {
 public:
  static std::unique_ptr<EventModulesFactory> Create() {
    return std::make_unique<EventModulesFactory>();
  }

  Event* Create(ExecutionContext*, const String& event_type) override;
};

}  // namespace blink

#endif
