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

#include "core/dom/ExecutionContext.h"
#include "modules/indexeddb/IDBTransaction.h"

namespace blink {

DEFINE_EMPTY_DESTRUCTOR_WILL_BE_REMOVED(IDBPendingTransactionMonitor)

const char* IDBPendingTransactionMonitor::supplementName()
{
    return "IDBPendingTransactionMonitor";
}

inline IDBPendingTransactionMonitor::IDBPendingTransactionMonitor()
{
}

IDBPendingTransactionMonitor& IDBPendingTransactionMonitor::from(WillBeHeapSupplementable<ExecutionContext>& context)
{
    IDBPendingTransactionMonitor* supplement = static_cast<IDBPendingTransactionMonitor*>(WillBeHeapSupplement<ExecutionContext>::from(context, supplementName()));
    if (!supplement) {
        supplement = new IDBPendingTransactionMonitor();
        provideTo(context, supplementName(), adoptPtrWillBeNoop(supplement));
    }
    return *supplement;
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

void IDBPendingTransactionMonitor::trace(Visitor* visitor)
{
#if ENABLE(OILPAN)
    visitor->trace(m_transactions);
#endif
    WillBeHeapSupplement<ExecutionContext>::trace(visitor);
}

} // namespace blink
