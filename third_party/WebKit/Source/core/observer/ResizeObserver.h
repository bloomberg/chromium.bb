// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ResizeObserver_h
#define ResizeObserver_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "platform/heap/Handle.h"

namespace blink {

class Document;
class Element;
class ResizeObserverCallback;
class ResizeObserverController;
class ResizeObservation;

// ResizeObserver represents ResizeObserver javascript api:
// https://github.com/WICG/ResizeObserver/
class ResizeObserver final : public GarbageCollectedFinalized<ResizeObserver>, public ScriptWrappable {
    DEFINE_WRAPPERTYPEINFO();
public:

    static ResizeObserver* create(Document&, ResizeObserverCallback*);

    virtual ~ResizeObserver() {};

    // API methods
    void observe(Element*);
    void unobserve(Element*);
    void disconnect();

    DECLARE_TRACE();

private:

    using ObservationList = HeapLinkedHashSet<WeakMember<ResizeObservation>>;

    explicit ResizeObserver(ResizeObserverCallback*, Document&);

    Member<ResizeObserverCallback> m_callback;

    // List of elements we are observing
    ObservationList m_observations;

    WeakMember<ResizeObserverController> m_controller;
};


} // namespace blink

#endif // ResizeObserver_h
