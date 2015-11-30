// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DragEvent_h
#define DragEvent_h

#include "core/CoreExport.h"
#include "core/events/DragEventInit.h"
#include "core/events/MouseEvent.h"

namespace blink {

class DataTransfer;

class CORE_EXPORT DragEvent final : public MouseEvent {
    DEFINE_WRAPPERTYPEINFO();

public:
    static PassRefPtrWillBeRawPtr<DragEvent> create()
    {
        return adoptRefWillBeNoop(new DragEvent);
    }

    static PassRefPtrWillBeRawPtr<DragEvent> create(DataTransfer* dataTransfer)
    {
        return adoptRefWillBeNoop(new DragEvent(dataTransfer));
    }

    static PassRefPtrWillBeRawPtr<DragEvent> create(const AtomicString& type, bool canBubble, bool cancelable, PassRefPtrWillBeRawPtr<AbstractView>,
        int detail, int screenX, int screenY, int windowX, int windowY,
        int movementX, int movementY,
        PlatformEvent::Modifiers, short button, unsigned short buttons,
        PassRefPtrWillBeRawPtr<EventTarget> relatedTarget,
        double platformTimeStamp, DataTransfer*,
        PlatformMouseEvent::SyntheticEventType = PlatformMouseEvent::RealOrIndistinguishable);

    static PassRefPtrWillBeRawPtr<DragEvent> create(const AtomicString& type, const DragEventInit& initializer)
    {
        return adoptRefWillBeNoop(new DragEvent(type, initializer));
    }

    DataTransfer* dataTransfer() const override { return isDragEvent() ? m_dataTransfer.get() : nullptr; }

    bool isDragEvent() const override;
    bool isMouseEvent() const override;

    PassRefPtrWillBeRawPtr<EventDispatchMediator> createMediator() override;

    DECLARE_VIRTUAL_TRACE();

private:
    DragEvent();
    DragEvent(DataTransfer*);
    DragEvent(const AtomicString& type, bool canBubble, bool cancelable, PassRefPtrWillBeRawPtr<AbstractView>,
        int detail, int screenX, int screenY, int windowX, int windowY,
        int movementX, int movementY,
        PlatformEvent::Modifiers, short button, unsigned short buttons,
        PassRefPtrWillBeRawPtr<EventTarget> relatedTarget,
        double platformTimeStamp, DataTransfer*,
        PlatformMouseEvent::SyntheticEventType);

    DragEvent(const AtomicString& type, const DragEventInit&);

    PersistentWillBeMember<DataTransfer> m_dataTransfer;
};

class DragEventDispatchMediator final : public EventDispatchMediator {
public:
    static PassRefPtrWillBeRawPtr<DragEventDispatchMediator> create(PassRefPtrWillBeRawPtr<DragEvent>);

private:
    explicit DragEventDispatchMediator(PassRefPtrWillBeRawPtr<DragEvent>);
    DragEvent& event() const;
    bool dispatchEvent(EventDispatcher&) const override;
};

DEFINE_EVENT_TYPE_CASTS(DragEvent);

} // namespace blink

#endif // DragEvent_h
