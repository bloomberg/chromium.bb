// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PermissionStatus_h
#define PermissionStatus_h

#include "core/dom/ContextLifecycleObserver.h"
#include "core/events/EventTarget.h"
#include "platform/heap/Handle.h"
#include "public/platform/modules/permissions/WebPermissionStatus.h"
#include "public/platform/modules/permissions/WebPermissionType.h"
#include "wtf/text/AtomicString.h"
#include "wtf/text/WTFString.h"

namespace blink {

class ExecutionContext;
class ScriptPromiseResolver;

class PermissionStatus final
    : public RefCountedGarbageCollectedEventTargetWithInlineData<PermissionStatus>
    , public ContextLifecycleObserver {
    DEFINE_EVENT_TARGET_REFCOUNTING_WILL_BE_REMOVED(RefCountedGarbageCollected<PermissionStatus>);
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(PermissionStatus);
    DEFINE_WRAPPERTYPEINFO();
public:
    static PermissionStatus* take(ScriptPromiseResolver*, WebPermissionStatus*, WebPermissionType);
    static void dispose(WebPermissionStatus*);

    ~PermissionStatus() override;

    // EventTarget implementation.
    const AtomicString& interfaceName() const override;
    ExecutionContext* executionContext() const override;

    DECLARE_VIRTUAL_TRACE();

    String status() const;
    // TODO: needs to be used by the IDL
    WebPermissionType type() const { return m_type; }

    DEFINE_ATTRIBUTE_EVENT_LISTENER(change);

private:
    explicit PermissionStatus(ExecutionContext*, WebPermissionType, WebPermissionStatus);

    WebPermissionType m_type;
    WebPermissionStatus m_status;
};

} // namespace blink

#endif // PermissionStatus_h
