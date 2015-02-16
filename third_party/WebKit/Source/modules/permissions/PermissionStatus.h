// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PermissionStatus_h
#define PermissionStatus_h

#include "core/dom/ContextLifecycleObserver.h"
#include "core/events/EventTarget.h"
#include "platform/heap/Handle.h"
#include "wtf/text/AtomicString.h"
#include "wtf/text/WTFString.h"

namespace blink {

class ExecutionContext;

class PermissionStatus final
    : public RefCountedGarbageCollectedEventTargetWithInlineData<PermissionStatus>
    , public ContextLifecycleObserver {
    DEFINE_EVENT_TARGET_REFCOUNTING_WILL_BE_REMOVED(RefCountedGarbageCollected<PermissionStatus>);
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(PermissionStatus);
    DEFINE_WRAPPERTYPEINFO();
public:
    static PermissionStatus* create(ExecutionContext*);
    ~PermissionStatus() override;

    // EventTarget implementation.
    const AtomicString& interfaceName() const override;
    ExecutionContext* executionContext() const override;

    DECLARE_VIRTUAL_TRACE();

    String status() const;

    DEFINE_ATTRIBUTE_EVENT_LISTENER(change);

private:
    explicit PermissionStatus(ExecutionContext*);
};

} // namespace blink

#endif // PermissionStatus_h
