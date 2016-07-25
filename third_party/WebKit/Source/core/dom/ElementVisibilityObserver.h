// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ElementVisibilityObserver_h
#define ElementVisibilityObserver_h

#include "core/CoreExport.h"
#include "core/dom/IntersectionObserverCallback.h"
#include "platform/heap/Handle.h"

namespace blink {

class Element;
class IntersectionObserver;
class IntersectionObserverEntry;

// ElementVisibilityObserver is a helper class to be used to track the
// visibility of an Element in the viewport; it is implemented on top of
// IntersectionObserver.
//
// When creating an ElementVisibilityObserver instance, alongside the element
// reference, the caller will have to supply an object reference implementing
// the |Client| interface and its |onVisibilityChanged| method. The callback
// method will be invoked when the element changes visibility state,
// the boolean argument indicating which.
class ElementVisibilityObserver final : public IntersectionObserverCallback {
    WTF_MAKE_NONCOPYABLE(ElementVisibilityObserver);
public:
    class CORE_EXPORT Client : public GarbageCollectedMixin {
    public:
        virtual void onVisibilityChanged(bool isVisible) = 0;
        virtual ExecutionContext* getElementVisibilityExecutionContext() const = 0;
    };

    static ElementVisibilityObserver* create(Element*, Client*);
    ~ElementVisibilityObserver();
    DECLARE_VIRTUAL_TRACE();

    void stop();

    // IntersectionObserverCallback implementation:
    void handleEvent(const HeapVector<Member<IntersectionObserverEntry>>&, IntersectionObserver&) override;
    ExecutionContext* getExecutionContext() const override;

private:
    explicit ElementVisibilityObserver(Client*);

    void start(Element*);

    Member<Client> m_client;
    Member<IntersectionObserver> m_intersectionObserver;
};

} // namespace blink

#endif // ElementVisibilityObserver_h
