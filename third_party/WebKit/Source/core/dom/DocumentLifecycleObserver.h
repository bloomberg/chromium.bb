/*
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef DocumentLifecycleObserver_h
#define DocumentLifecycleObserver_h

#include "core/dom/ContextLifecycleNotifier.h"
#include "core/dom/ContextLifecycleObserver.h"
#include "wtf/Assertions.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/TemporaryChange.h"

namespace WebCore {

class Document;

class DocumentLifecycleObserver : public ContextLifecycleObserver {
public:
    explicit DocumentLifecycleObserver(Document*);
    virtual ~DocumentLifecycleObserver();
    virtual void documentWasDetached() { }
    virtual void documentWasDisposed() { }
};

class DocumentLifecycleNotifier : public ContextLifecycleNotifier {
public:
    static PassOwnPtr<DocumentLifecycleNotifier> create(ScriptExecutionContext*);

    void notifyDocumentWasDetached();
    void notifyDocumentWasDisposed();

    virtual void addObserver(LifecycleObserver*) OVERRIDE;
    virtual void removeObserver(LifecycleObserver*) OVERRIDE;

private:
    explicit DocumentLifecycleNotifier(ScriptExecutionContext*);

    typedef HashSet<DocumentLifecycleObserver*> DocumentObserverSet;
    DocumentObserverSet m_documentObservers;
};

inline PassOwnPtr<DocumentLifecycleNotifier> DocumentLifecycleNotifier::create(ScriptExecutionContext* context)
{
    return adoptPtr(new DocumentLifecycleNotifier(context));
}

inline void DocumentLifecycleNotifier::notifyDocumentWasDetached()
{
    TemporaryChange<IterationType> scope(this->m_iterating, IteratingOverDocumentObservers);
    for (DocumentObserverSet::iterator i = m_documentObservers.begin(); i != m_documentObservers.end(); ++i)
        (*i)->documentWasDetached();
}

inline void DocumentLifecycleNotifier::notifyDocumentWasDisposed()
{
    TemporaryChange<IterationType> scope(this->m_iterating, IteratingOverDocumentObservers);
    for (DocumentObserverSet::iterator i = m_documentObservers.begin(); i != m_documentObservers.end(); ++i)
        (*i)->documentWasDisposed();
}

} // namespace WebCore

#endif // DocumentLifecycleObserver_h
