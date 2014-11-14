// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PushEvent_h
#define PushEvent_h

#include "modules/EventModules.h"
#include "modules/serviceworkers/ExtendableEvent.h"
#include "platform/heap/Handle.h"
#include "wtf/text/AtomicString.h"
#include "wtf/text/WTFString.h"

namespace blink {

struct PushEventInit : public ExtendableEventInit {
    PushEventInit();

    String data;
};

class PushEvent final : public ExtendableEvent {
    DEFINE_WRAPPERTYPEINFO();
public:
    static PassRefPtrWillBeRawPtr<PushEvent> create()
    {
        return adoptRefWillBeNoop(new PushEvent);
    }
    static PassRefPtrWillBeRawPtr<PushEvent> create(const AtomicString& type, const String& data, WaitUntilObserver* observer)
    {
        return adoptRefWillBeNoop(new PushEvent(type, data, observer));
    }
    static PassRefPtrWillBeRawPtr<PushEvent> create(const AtomicString& type, const PushEventInit& initializer)
    {
        return adoptRefWillBeNoop(new PushEvent(type, initializer));
    }

    virtual ~PushEvent();

    virtual const AtomicString& interfaceName() const override;

    String data() const { return m_data; }

private:
    PushEvent();
    PushEvent(const AtomicString& type, const String& data, WaitUntilObserver*);
    PushEvent(const AtomicString& type, const PushEventInit&);
    String m_data;
};

} // namespace blink

#endif // PushEvent_h
