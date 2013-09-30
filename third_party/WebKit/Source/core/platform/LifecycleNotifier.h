/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
 * Copyright (C) 2013 Google Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */
#ifndef LifecycleNotifier_h
#define LifecycleNotifier_h

#include "core/platform/LifecycleObserver.h"
#include "wtf/HashSet.h"
#include "wtf/PassOwnPtr.h"

namespace WebCore {

class LifecycleContext;
class LifecycleObserver;

class LifecycleNotifier {
public:
    static PassOwnPtr<LifecycleNotifier> create(LifecycleContext*);

    virtual ~LifecycleNotifier();

    virtual void addObserver(LifecycleObserver*);
    virtual void removeObserver(LifecycleObserver*);

    bool isIteratingOverObservers() const { return m_iterating != IteratingNone; }

protected:
    explicit LifecycleNotifier(LifecycleContext*);

    LifecycleContext* context();

    enum IterationType {
        IteratingNone,
        IteratingOverActiveDOMObjects,
        IteratingOverContextObservers,
        IteratingOverDocumentObservers,
        IteratingOverPageObservers,
        IteratingOverDOMWindowObservers
    };

    IterationType m_iterating;

private:
    typedef HashSet<LifecycleObserver*> ObserverSet;

    ObserverSet m_observers;
    LifecycleContext* m_context;
    bool m_inDestructor;
};

inline PassOwnPtr<LifecycleNotifier> LifecycleNotifier::create(LifecycleContext* context)
{
    return adoptPtr(new LifecycleNotifier(context));
}

} // namespace WebCore

#endif // LifecycleNotifier_h
