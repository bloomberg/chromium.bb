// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PromiseRejectionEvent_h
#define PromiseRejectionEvent_h

#include "bindings/core/v8/ScopedPersistent.h"
#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptValue.h"
#include "core/CoreExport.h"
#include "core/events/Event.h"
#include "core/events/PromiseRejectionEventInit.h"

namespace blink {

class CORE_EXPORT PromiseRejectionEvent final : public Event {
    DEFINE_WRAPPERTYPEINFO();
public:
    static PassRefPtrWillBeRawPtr<PromiseRejectionEvent> create()
    {
        return adoptRefWillBeNoop(new PromiseRejectionEvent);
    }
    static PassRefPtrWillBeRawPtr<PromiseRejectionEvent> create(const AtomicString& type, const PromiseRejectionEventInit& initializer)
    {
        return adoptRefWillBeNoop(new PromiseRejectionEvent(type, initializer));
    }

    ScriptValue reason(ScriptState*) const;
    ScriptPromise promise(ScriptState*) const;

    virtual const AtomicString& interfaceName() const override;

    DECLARE_VIRTUAL_TRACE();

private:
    PromiseRejectionEvent();
    PromiseRejectionEvent(const AtomicString& type, ScriptPromise, ScriptValue reason);
    PromiseRejectionEvent(const AtomicString&, const PromiseRejectionEventInit&);
    ~PromiseRejectionEvent() override;

    static void didCollectPromise(const v8::WeakCallbackInfo<PromiseRejectionEvent>&);
    static void didCollectReason(const v8::WeakCallbackInfo<PromiseRejectionEvent>&);

    ScopedPersistent<v8::Value> m_promise;
    ScopedPersistent<v8::Value> m_reason;
};

} // namespace blink

#endif // PromiseRejectionEvent_h
