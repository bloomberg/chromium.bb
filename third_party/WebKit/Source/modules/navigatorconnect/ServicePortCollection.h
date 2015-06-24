// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ServicePortCollection_h
#define ServicePortCollection_h

#include "bindings/core/v8/ScriptPromise.h"
#include "core/dom/ContextLifecycleObserver.h"
#include "core/events/EventTarget.h"
#include "modules/ModulesExport.h"
#include "wtf/RefCounted.h"

namespace blink {

class ServicePortConnectOptions;
class ServicePortMatchOptions;

class MODULES_EXPORT ServicePortCollection final
    : public EventTargetWithInlineData
    , public RefCountedWillBeNoBase<ServicePortCollection>
    , public ContextLifecycleObserver {
    DEFINE_WRAPPERTYPEINFO();
    REFCOUNTED_EVENT_TARGET(ServicePortCollection);
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(ServicePortCollection);
    WTF_MAKE_NONCOPYABLE(ServicePortCollection);
public:
    static PassRefPtrWillBeRawPtr<ServicePortCollection> create(ExecutionContext*);
    ~ServicePortCollection() override;

    // ServicePortCollection.idl
    ScriptPromise connect(ScriptState*, const String& url, const ServicePortConnectOptions&);
    ScriptPromise match(ScriptState*, const ServicePortMatchOptions&);
    ScriptPromise matchAll(ScriptState*, const ServicePortMatchOptions&);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(connect);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(message);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(close);

    // EventTarget overrides.
    const AtomicString& interfaceName() const override;
    ExecutionContext* executionContext() const override;

    DECLARE_VIRTUAL_TRACE();

private:
    explicit ServicePortCollection(ExecutionContext*);
};

} // namespace blink

#endif // ServicePortCollection_h
