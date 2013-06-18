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

#include "core/dom/ContextDestructionObserver.h"
#include "wtf/Assertions.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/Vector.h"

namespace WebCore {

class Document;

class DocumentLifecycleObserver : public ContextDestructionObserver {
public:
    explicit DocumentLifecycleObserver(Document*);
    virtual ~DocumentLifecycleObserver() { }
    virtual void documentWasDetached() { }
    virtual void documentWasDisposed() { }
};

class DocumentLifecycleNotifier {
public:
    static PassOwnPtr<DocumentLifecycleNotifier> create();

    void notifyDocumentWasDetached();
    void notifyDocumentWasDisposed();

    void addObserver(DocumentLifecycleObserver*);

private:
#if ASSERT_DISABLED
    DocumentLifecycleNotifier() { }
    void startIteration() { }
    void endIteration() { }
#else
    DocumentLifecycleNotifier() : m_iterating(false) { }
    void startIteration() { m_iterating = true; }
    void endIteration() { m_iterating = false; }
    bool m_iterating;
#endif

    Vector<DocumentLifecycleObserver*> m_observers; // Use Vector instead of HashSet for faster iteration
};

inline void DocumentLifecycleNotifier::notifyDocumentWasDetached()
{
    startIteration();
    for (size_t i = 0; i < m_observers.size(); ++i)
        m_observers[i]->documentWasDetached();
    endIteration();
}

inline void DocumentLifecycleNotifier::notifyDocumentWasDisposed()
{
    startIteration();
    for (size_t i = 0; i < m_observers.size(); ++i)
        m_observers[i]->documentWasDisposed();
    endIteration();
}

} // namespace WebCore

#endif // DocumentLifecycleObserver_h
