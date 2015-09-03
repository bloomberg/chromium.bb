// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MediaRecorderErrorEvent_h
#define MediaRecorderErrorEvent_h

#include "modules/EventModules.h"
#include "modules/ModulesExport.h"
#include "wtf/text/AtomicString.h"
#include "wtf/text/WTFString.h"

namespace blink {

// TODO(mcasas): Remove completely MediaRecorderErrorEvent, see https://github.com/w3c/mediacapture-record/issues/14.
class MODULES_EXPORT MediaRecorderErrorEvent final : public Event {
    DEFINE_WRAPPERTYPEINFO();
public:
    virtual ~MediaRecorderErrorEvent() {}

    static PassRefPtrWillBeRawPtr<MediaRecorderErrorEvent> create();
    static PassRefPtrWillBeRawPtr<MediaRecorderErrorEvent> create(const AtomicString& type, bool canBubble, bool cancelable, const String& name, const String& message);

    const String& name() const { return m_name; }
    const String& message() const { return m_message; }

    // Event
    virtual const AtomicString& interfaceName() const override;

    DEFINE_INLINE_VIRTUAL_TRACE() { Event::trace(visitor); }

private:
    MediaRecorderErrorEvent() {}
    MediaRecorderErrorEvent(const AtomicString& type, bool canBubble, bool cancelable, const String& name, const String& message);

    String m_name;
    String m_message;
};

} // namespace blink

#endif // MediaRecorderErrorEvent_h
