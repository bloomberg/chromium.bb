/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "modules/indexeddb/IDBPendingTransactionMonitor.h"

#include "modules/indexeddb/IDBCursor.h"
#include "modules/indexeddb/IDBRequest.h"
#include "modules/indexeddb/IDBTransaction.h"

namespace blink {

IDBPendingTransactionMonitor::IDBPendingTransactionMonitor()
{
}

IDBPendingTransactionMonitor::~IDBPendingTransactionMonitor()
{
}

void IDBPendingTransactionMonitor::addNewTransaction(IDBTransaction& transaction)
{
    m_transactions.append(&transaction);
}

void IDBPendingTransactionMonitor::deactivateNewTransactions()
{
    for (size_t i = 0; i < m_transactions.size(); ++i)
        m_transactions[i]->setActive(false);
    // FIXME: Exercise this call to clear() in a layout test.
    m_transactions.clear();
}

// IDBDisposerDispatcher should be RefCounted because it should outlive all of
// target objects.
class IDBDisposerDispatcher: public RefCounted<IDBDisposerDispatcher> {
public:
    static PassRefPtr<IDBDisposerDispatcher> create() { return adoptRef(new IDBDisposerDispatcher()); }

private:
    IDBDisposerDispatcher() { }

    template<typename Owner, typename Target>
    class Disposer {
    public:
        static PassOwnPtr<Disposer> create(Owner& owner, Target& target) { return adoptPtr(new Disposer(owner, target)); }
        ~Disposer()
        {
            if (!m_isDisabled)
                m_target.dispose();
        }
        void setDisabled() { m_isDisabled = true; }

    private:
        Disposer(Owner& owner, Target& target)
            : m_owner(owner)
            , m_target(target)
            , m_isDisabled(false)
        {
        }

        RefPtr<Owner> m_owner;
        Target& m_target;
        bool m_isDisabled;
    };

    template<typename Target>
    class DisposerMap {
        DISALLOW_ALLOCATION();
    public:
        void registerTarget(IDBDisposerDispatcher& dispatcher, Target& target)
        {
            ASSERT(!m_disposerMap.contains(&target));
            m_disposerMap.add(&target, Disposer<IDBDisposerDispatcher, Target>::create(dispatcher, target));
        }

        void unregisterTarget(IDBDisposerDispatcher& dispatcher, Target& target)
        {
            // Skip this function if this is called in Target::dispose().
            if (ThreadState::current()->isSweepInProgress())
                return;
            auto it = m_disposerMap.find(&target);
            ASSERT(it != m_disposerMap.end());
            if (it == m_disposerMap.end())
                return;
            // m_disposerMap.remove() will trigger ~Disposer. We should not call
            // Target::dispose() in ~Disposer in this case.
            it->value->setDisabled();
            m_disposerMap.remove(it);
        }

    private:
        PersistentHeapHashMap<WeakMember<Target>, OwnPtr<Disposer<IDBDisposerDispatcher, Target>>> m_disposerMap;
    };

    DisposerMap<IDBRequest> m_requests;
    DisposerMap<IDBCursor> m_cursors;
    friend class IDBPendingTransactionMonitor;
};

void IDBPendingTransactionMonitor::registerRequest(IDBRequest& request)
{
    if (!m_dispatcher)
        m_dispatcher = IDBDisposerDispatcher::create();
    m_dispatcher->m_requests.registerTarget(*m_dispatcher, request);
}

void IDBPendingTransactionMonitor::unregisterRequest(IDBRequest& request)
{
    // We should not unregister without registeration.
    ASSERT(m_dispatcher);
    m_dispatcher->m_requests.unregisterTarget(*m_dispatcher, request);
}

void IDBPendingTransactionMonitor::registerCursor(IDBCursor& cursor)
{
    if (!m_dispatcher)
        m_dispatcher = IDBDisposerDispatcher::create();
    m_dispatcher->m_cursors.registerTarget(*m_dispatcher, cursor);
}

void IDBPendingTransactionMonitor::unregisterCursor(IDBCursor& cursor)
{
    // We should not unregister without registeration.
    ASSERT(m_dispatcher);
    m_dispatcher->m_cursors.unregisterTarget(*m_dispatcher, cursor);
}

} // namespace blink
