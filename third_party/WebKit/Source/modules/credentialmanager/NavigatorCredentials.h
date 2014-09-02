// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NavigatorCredentials_h
#define NavigatorCredentials_h

#include "core/frame/DOMWindowProperty.h"
#include "core/frame/Navigator.h"
#include "platform/Supplementable.h"
#include "platform/heap/Handle.h"

namespace blink {

class CredentialsContainer;
class Navigator;

class NavigatorCredentials FINAL : public NoBaseWillBeGarbageCollectedFinalized<NavigatorCredentials>, public WillBeHeapSupplement<Navigator>, public DOMWindowProperty {
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(NavigatorCredentials);
public:
    virtual ~NavigatorCredentials();
    static NavigatorCredentials& from(Navigator&);
    static const char* supplementName();

    // NavigatorCredentials.idl
    static CredentialsContainer* credentials(Navigator&);

    void trace(Visitor*);

private:
    explicit NavigatorCredentials(Navigator&);
    CredentialsContainer* credentials();

    PersistentWillBeMember<CredentialsContainer> m_credentialsContainer;
};

} // namespace blink

#endif // NavigatorCredentials_h
