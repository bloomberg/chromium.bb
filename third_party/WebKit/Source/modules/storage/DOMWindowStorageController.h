// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DOMWindowStorageController_h
#define DOMWindowStorageController_h

#include "core/frame/DOMWindowLifecycleObserver.h"
#include "platform/Supplementable.h"
#include "platform/heap/Handle.h"

namespace blink {

class Document;
class Event;

class DOMWindowStorageController final : public NoBaseWillBeGarbageCollectedFinalized<DOMWindowStorageController>, public WillBeHeapSupplement<Document>, public DOMWindowLifecycleObserver {
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(DOMWindowStorageController);
public:
    virtual ~DOMWindowStorageController();
    DECLARE_VIRTUAL_TRACE();

    static const char* supplementName();
    static DOMWindowStorageController& from(Document&);

    // Inherited from DOMWindowLifecycleObserver
    virtual void didAddEventListener(LocalDOMWindow*, const AtomicString&) override;

protected:
    explicit DOMWindowStorageController(Document&);

private:
    Document& document() const { return *m_document; }

    RawPtrWillBeMember<Document> m_document;
};

} // namespace blink

#endif // DOMWindowStorageController_h
