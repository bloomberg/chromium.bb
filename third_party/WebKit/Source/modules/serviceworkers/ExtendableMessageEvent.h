// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ExtendableMessageEvent_h
#define ExtendableMessageEvent_h

#include "modules/EventModules.h"
#include "modules/ModulesExport.h"
#include "modules/serviceworkers/ExtendableEvent.h"
#include "modules/serviceworkers/ExtendableMessageEventInit.h"

namespace blink {

class MODULES_EXPORT ExtendableMessageEvent final : public ExtendableEvent {
    DEFINE_WRAPPERTYPEINFO();

public:
    static PassRefPtrWillBeRawPtr<ExtendableMessageEvent> create();
    static PassRefPtrWillBeRawPtr<ExtendableMessageEvent> create(const AtomicString& type, const ExtendableMessageEventInit& initializer);
    static PassRefPtrWillBeRawPtr<ExtendableMessageEvent> create(const AtomicString& type, const ExtendableMessageEventInit& initializer, WaitUntilObserver*);

    const AtomicString& interfaceName() const override;

    DECLARE_VIRTUAL_TRACE();

private:
    ExtendableMessageEvent();
    ExtendableMessageEvent(const AtomicString& type, const ExtendableMessageEventInit& initializer);
    ExtendableMessageEvent(const AtomicString& type, const ExtendableMessageEventInit& initializer, WaitUntilObserver*);
};

} // namespace blink

#endif // ExtendableMessageEvent_h
