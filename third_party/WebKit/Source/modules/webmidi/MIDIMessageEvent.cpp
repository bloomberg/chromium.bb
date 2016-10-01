// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/webmidi/MIDIMessageEvent.h"

#include "core/dom/ExecutionContext.h"
#include "core/frame/Deprecation.h"
#include "modules/webmidi/MIDIMessageEventInit.h"

namespace blink {

MIDIMessageEvent::MIDIMessageEvent(ExecutionContext* context,
                                   const AtomicString& type,
                                   const MIDIMessageEventInit& initializer)
    : Event(type, initializer), m_receivedTime(0.0) {
  if (initializer.hasReceivedTime()) {
    Deprecation::countDeprecation(context,
                                  UseCounter::MIDIMessageEventReceivedTime);
    m_receivedTime = initializer.receivedTime();
  }
  if (initializer.hasData())
    m_data = initializer.data();
}

}  // namespace blink
