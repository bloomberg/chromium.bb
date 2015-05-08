/*
 * Copyright (c) 2014, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "core/page/NetworkStateNotifier.h"

#include "core/dom/Document.h"
#include "platform/testing/UnitTestHelpers.h"
#include "public/platform/Platform.h"
#include "public/platform/WebConnectionType.h"
#include "public/platform/WebThread.h"
#include "wtf/Functional.h"
#include <gtest/gtest.h>

namespace blink {

class StateObserver : public NetworkStateNotifier::NetworkStateObserver {
public:
    StateObserver()
        : m_observedType(ConnectionTypeNone)
        , m_callbackCount(0)
    {
    }

    virtual void connectionTypeChange(WebConnectionType type)
    {
        m_observedType = type;
        m_callbackCount += 1;

        if (m_closure)
            (*m_closure)();
    }

    WebConnectionType observedType() const
    {
        return m_observedType;
    }

    int callbackCount() const
    {
        return m_callbackCount;
    }

    void setNotificationCallback(PassOwnPtr<Closure> closure)
    {
        m_closure = closure;
    }

private:
    OwnPtr<Closure> m_closure;
    WebConnectionType m_observedType;
    int m_callbackCount;
};

class NetworkStateNotifierTest : public ::testing::Test {
public:
    NetworkStateNotifierTest()
        : m_document(Document::create())
        , m_document2(Document::create())
    {
    }

    ExecutionContext* executionContext()
    {
        return m_document.get();
    }

    ExecutionContext* executionContext2()
    {
        return m_document2.get();
    }

protected:
    void setType(WebConnectionType type)
    {
        m_notifier.setWebConnectionType(type);
        testing::runPendingTasks();
    }

    void addObserverOnNotification(StateObserver* observer, StateObserver* observerToAdd)
    {
        observer->setNotificationCallback(bind(&NetworkStateNotifier::addObserver, &m_notifier, observerToAdd, executionContext()));
    }

    void removeObserverOnNotification(StateObserver* observer, StateObserver* observerToRemove)
    {
        observer->setNotificationCallback(bind(&NetworkStateNotifier::removeObserver, &m_notifier, observerToRemove, executionContext()));
    }

    RefPtrWillBePersistent<Document> m_document;
    RefPtrWillBePersistent<Document> m_document2;
    NetworkStateNotifier m_notifier;
};

TEST_F(NetworkStateNotifierTest, AddObserver)
{
    StateObserver observer;
    m_notifier.addObserver(&observer, executionContext());
    EXPECT_EQ(observer.observedType(), ConnectionTypeNone);

    setType(ConnectionTypeBluetooth);
    EXPECT_EQ(observer.observedType(), ConnectionTypeBluetooth);
    EXPECT_EQ(observer.callbackCount(), 1);
}

TEST_F(NetworkStateNotifierTest, RemoveObserver)
{
    StateObserver observer1, observer2;
    m_notifier.addObserver(&observer1, executionContext());
    m_notifier.removeObserver(&observer1, executionContext());
    m_notifier.addObserver(&observer2, executionContext());

    setType(ConnectionTypeBluetooth);
    EXPECT_EQ(observer1.observedType(), ConnectionTypeNone);
    EXPECT_EQ(observer2.observedType(), ConnectionTypeBluetooth);
}

TEST_F(NetworkStateNotifierTest, RemoveSoleObserver)
{
    StateObserver observer1, observer2;
    m_notifier.addObserver(&observer1, executionContext());
    m_notifier.removeObserver(&observer1, executionContext());

    setType(ConnectionTypeBluetooth);
    EXPECT_EQ(observer1.observedType(), ConnectionTypeNone);
}

TEST_F(NetworkStateNotifierTest, AddObserverWhileNotifying)
{
    StateObserver observer1, observer2;
    m_notifier.addObserver(&observer1, executionContext());
    addObserverOnNotification(&observer1, &observer2);

    setType(ConnectionTypeBluetooth);
    EXPECT_EQ(observer1.observedType(), ConnectionTypeBluetooth);
    EXPECT_EQ(observer2.observedType(), ConnectionTypeBluetooth);
}

TEST_F(NetworkStateNotifierTest, RemoveSoleObserverWhileNotifying)
{
    StateObserver observer1;
    m_notifier.addObserver(&observer1, executionContext());
    removeObserverOnNotification(&observer1, &observer1);

    setType(ConnectionTypeBluetooth);
    EXPECT_EQ(observer1.observedType(), ConnectionTypeBluetooth);

    setType(ConnectionTypeEthernet);
    EXPECT_EQ(observer1.observedType(), ConnectionTypeBluetooth);
}

TEST_F(NetworkStateNotifierTest, RemoveCurrentObserverWhileNotifying)
{
    StateObserver observer1, observer2;
    m_notifier.addObserver(&observer1, executionContext());
    m_notifier.addObserver(&observer2, executionContext());
    removeObserverOnNotification(&observer1, &observer1);

    setType(ConnectionTypeBluetooth);
    EXPECT_EQ(observer1.observedType(), ConnectionTypeBluetooth);
    EXPECT_EQ(observer2.observedType(), ConnectionTypeBluetooth);

    setType(ConnectionTypeEthernet);
    EXPECT_EQ(observer1.observedType(), ConnectionTypeBluetooth);
    EXPECT_EQ(observer2.observedType(), ConnectionTypeEthernet);
}

TEST_F(NetworkStateNotifierTest, RemovePastObserverWhileNotifying)
{
    StateObserver observer1, observer2;
    m_notifier.addObserver(&observer1, executionContext());
    m_notifier.addObserver(&observer2, executionContext());
    removeObserverOnNotification(&observer2, &observer1);

    setType(ConnectionTypeBluetooth);
    EXPECT_EQ(observer1.observedType(), ConnectionTypeBluetooth);
    EXPECT_EQ(observer2.observedType(), ConnectionTypeBluetooth);

    setType(ConnectionTypeEthernet);
    EXPECT_EQ(observer1.observedType(), ConnectionTypeBluetooth);
    EXPECT_EQ(observer2.observedType(), ConnectionTypeEthernet);
}

TEST_F(NetworkStateNotifierTest, RemoveFutureObserverWhileNotifying)
{
    StateObserver observer1, observer2, observer3;
    m_notifier.addObserver(&observer1, executionContext());
    m_notifier.addObserver(&observer2, executionContext());
    m_notifier.addObserver(&observer3, executionContext());
    removeObserverOnNotification(&observer1, &observer2);

    setType(ConnectionTypeBluetooth);

    EXPECT_EQ(observer1.observedType(), ConnectionTypeBluetooth);
    EXPECT_EQ(observer2.observedType(), ConnectionTypeNone);
    EXPECT_EQ(observer3.observedType(), ConnectionTypeBluetooth);
}

TEST_F(NetworkStateNotifierTest, MultipleContextsAddObserver)
{
    StateObserver observer1, observer2;
    m_notifier.addObserver(&observer1, executionContext());
    m_notifier.addObserver(&observer2, executionContext2());

    setType(ConnectionTypeBluetooth);
    EXPECT_EQ(observer1.observedType(), ConnectionTypeBluetooth);
    EXPECT_EQ(observer2.observedType(), ConnectionTypeBluetooth);
}

TEST_F(NetworkStateNotifierTest, RemoveContext)
{
    StateObserver observer1, observer2;
    m_notifier.addObserver(&observer1, executionContext());
    m_notifier.addObserver(&observer2, executionContext2());
    m_notifier.removeObserver(&observer2, executionContext2());

    setType(ConnectionTypeBluetooth);
    EXPECT_EQ(observer1.observedType(), ConnectionTypeBluetooth);
    EXPECT_EQ(observer2.observedType(), ConnectionTypeNone);
}

TEST_F(NetworkStateNotifierTest, RemoveAllContexts)
{
    StateObserver observer1, observer2;
    m_notifier.addObserver(&observer1, executionContext());
    m_notifier.addObserver(&observer2, executionContext2());
    m_notifier.removeObserver(&observer1, executionContext());
    m_notifier.removeObserver(&observer2, executionContext2());

    setType(ConnectionTypeBluetooth);
    EXPECT_EQ(observer1.observedType(), ConnectionTypeNone);
    EXPECT_EQ(observer2.observedType(), ConnectionTypeNone);
}

} // namespace blink
