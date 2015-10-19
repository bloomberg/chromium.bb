// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef InstallEvent_h
#define InstallEvent_h

#include "modules/EventModules.h"
#include "modules/ModulesExport.h"
#include "modules/serviceworkers/ExtendableEvent.h"

namespace blink {

class MODULES_EXPORT InstallEvent : public ExtendableEvent {
    DEFINE_WRAPPERTYPEINFO();

public:
    static PassRefPtrWillBeRawPtr<InstallEvent> create();
    static PassRefPtrWillBeRawPtr<InstallEvent> create(const AtomicString& type, const ExtendableEventInit&);
    static PassRefPtrWillBeRawPtr<InstallEvent> create(const AtomicString& type, const ExtendableEventInit&, WaitUntilObserver*);

    ~InstallEvent() override;

    void registerForeignFetchScopes(ExecutionContext*, const Vector<String>& subScopes, ExceptionState&);

    const AtomicString& interfaceName() const override;

protected:
    InstallEvent();
    InstallEvent(const AtomicString& type, const ExtendableEventInit&);
    InstallEvent(const AtomicString& type, const ExtendableEventInit&, WaitUntilObserver*);
};

} // namespace blink

#endif // InstallEvent_h
