// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BeforeInstallPromptEvent_h
#define BeforeInstallPromptEvent_h

#include "modules/EventModules.h"
#include "platform/heap/Handle.h"

namespace blink {

class BeforeInstallPromptEventInit;

class BeforeInstallPromptEvent final : public Event {
    DEFINE_WRAPPERTYPEINFO();
public:
    virtual ~BeforeInstallPromptEvent();

    // For EventModules.cpp
    static PassRefPtrWillBeRawPtr<BeforeInstallPromptEvent> create()
    {
        return adoptRefWillBeNoop(new BeforeInstallPromptEvent());
    }

    static PassRefPtrWillBeRawPtr<BeforeInstallPromptEvent> create(const AtomicString& name, const String& platform)
    {
        return adoptRefWillBeNoop(new BeforeInstallPromptEvent(name, platform));
    }

    static PassRefPtrWillBeRawPtr<BeforeInstallPromptEvent> create(const AtomicString& name, const BeforeInstallPromptEventInit& init)
    {
        return adoptRefWillBeNoop(new BeforeInstallPromptEvent(name, init));
    }

    String platform() const;
    ScriptPromise userChoice(ScriptState*) const;

    virtual const AtomicString& interfaceName() const override;

private:
    BeforeInstallPromptEvent();
    BeforeInstallPromptEvent(const AtomicString& name, const String& platform);
    BeforeInstallPromptEvent(const AtomicString& name, const BeforeInstallPromptEventInit&);

    String m_platform;
};

DEFINE_TYPE_CASTS(BeforeInstallPromptEvent, Event, event, event->interfaceName() == EventNames::BeforeInstallPromptEvent, event.interfaceName() == EventNames::BeforeInstallPromptEvent);

} // namespace blink

#endif // BeforeInstallPromptEvent_h
