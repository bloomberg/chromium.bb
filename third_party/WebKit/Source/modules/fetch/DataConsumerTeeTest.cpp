// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/fetch/DataConsumerTee.h"

#include "core/testing/DummyPageHolder.h"
#include "core/testing/NullExecutionContext.h"
#include "modules/fetch/DataConsumerHandleTestUtil.h"
#include "platform/Task.h"
#include "platform/ThreadSafeFunctional.h"
#include "platform/WebThreadSupportingGC.h"
#include "public/platform/Platform.h"
#include "public/platform/WebThread.h"
#include "public/platform/WebTraceLocation.h"
#include "public/platform/WebWaitableEvent.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefPtr.h"

#include <gtest/gtest.h>
#include <string.h>
#include <v8.h>

namespace blink {
namespace {

using Result = WebDataConsumerHandle::Result;
using Thread = DataConsumerHandleTestUtil::Thread;
const WebDataConsumerHandle::Flags kNone = WebDataConsumerHandle::FlagNone;
const Result kOk = WebDataConsumerHandle::Ok;
const Result kShouldWait = WebDataConsumerHandle::ShouldWait;
const Result kDone = WebDataConsumerHandle::Done;
const Result kUnexpectedError = WebDataConsumerHandle::UnexpectedError;

using Command = DataConsumerHandleTestUtil::Command;
using Handle = DataConsumerHandleTestUtil::ReplayingHandle;

class HandleReader : public WebDataConsumerHandle::Client {
public:
    HandleReader() : m_finalResult(kOk) { }

    // Need to wait for the event signal after this function is called.
    void start(PassOwnPtr<WebDataConsumerHandle> handle)
    {
        m_thread = adoptPtr(new Thread("reading thread"));
        m_waitableEvent = adoptPtr(Platform::current()->createWaitableEvent());
        m_thread->thread()->postTask(FROM_HERE, new Task(threadSafeBind(&HandleReader::obtainReader, AllowCrossThreadAccess(this), handle)));
    }

    void didGetReadable() override
    {
        Result r = kOk;
        char buffer[3];
        while (true) {
            size_t size;
            r = m_reader->read(buffer, sizeof(buffer), kNone, &size);
            if (r == kShouldWait)
                return;
            if (r != kOk)
                break;
            m_readString.append(String(buffer, size));
        }
        m_finalResult = r;
        m_reader = nullptr;
        m_waitableEvent->signal();
    }

    WebWaitableEvent* waitableEvent() { return m_waitableEvent.get(); }

    // These should be accessed after the thread joins.
    const String& readString() const { return m_readString; }
    Result finalResult() const { return m_finalResult; }

private:
    void obtainReader(PassOwnPtr<WebDataConsumerHandle> handle)
    {
        m_reader = handle->obtainReader(this);
    }

    OwnPtr<Thread> m_thread;
    OwnPtr<WebDataConsumerHandle::Reader> m_reader;
    String m_readString;
    Result m_finalResult;
    OwnPtr<WebWaitableEvent> m_waitableEvent;
};

class HandleTwoPhaseReader : public WebDataConsumerHandle::Client {
public:
    HandleTwoPhaseReader() : m_finalResult(kOk) { }

    // Need to wait for the event signal after this function is called.
    void start(PassOwnPtr<WebDataConsumerHandle> handle)
    {
        m_thread = adoptPtr(new Thread("reading thread"));
        m_waitableEvent = adoptPtr(Platform::current()->createWaitableEvent());
        m_thread->thread()->postTask(FROM_HERE, new Task(threadSafeBind(&HandleTwoPhaseReader::obtainReader, AllowCrossThreadAccess(this), handle)));
    }

    void didGetReadable() override
    {
        Result r = kOk;
        while (true) {
            const void* buffer = nullptr;
            size_t size;
            r = m_reader->beginRead(&buffer, kNone, &size);
            if (r == kShouldWait)
                return;
            if (r != kOk)
                break;
            // Read smaller than availabe in order to test |endRead|.
            size_t readSize = std::max(size * 2 / 3, static_cast<size_t>(1));
            m_readString.append(String(static_cast<const char*>(buffer), readSize));
            m_reader->endRead(readSize);
        }
        m_finalResult = r;
        m_reader = nullptr;
        m_waitableEvent->signal();
    }

    WebWaitableEvent* waitableEvent() { return m_waitableEvent.get(); }

    // These should be accessed after the thread joins.
    const String& readString() const { return m_readString; }
    Result finalResult() const { return m_finalResult; }

private:
    void obtainReader(PassOwnPtr<WebDataConsumerHandle> handle)
    {
        m_reader = handle->obtainReader(this);
    }

    OwnPtr<Thread> m_thread;
    OwnPtr<WebDataConsumerHandle::Reader> m_reader;
    String m_readString;
    Result m_finalResult;
    OwnPtr<WebWaitableEvent> m_waitableEvent;
};

class TeeCreationThread {
public:
    void run(PassOwnPtr<WebDataConsumerHandle> src, OwnPtr<WebDataConsumerHandle>* dest1, OwnPtr<WebDataConsumerHandle>* dest2)
    {
        m_thread = adoptPtr(new Thread("src thread", Thread::WithExecutionContext));
        m_waitableEvent = adoptPtr(Platform::current()->createWaitableEvent());
        m_thread->thread()->postTask(FROM_HERE, new Task(threadSafeBind(&TeeCreationThread::runInternal, AllowCrossThreadAccess(this), src, AllowCrossThreadAccess(dest1), AllowCrossThreadAccess(dest2))));
        m_waitableEvent->wait();
    }

    Thread* thread() { return m_thread.get(); }

private:
    void runInternal(PassOwnPtr<WebDataConsumerHandle> src, OwnPtr<WebDataConsumerHandle>* dest1, OwnPtr<WebDataConsumerHandle>* dest2)
    {
        DataConsumerTee::create(m_thread->executionContext(), src, dest1, dest2);
        m_waitableEvent->signal();
    }

    OwnPtr<Thread> m_thread;
    OwnPtr<WebWaitableEvent> m_waitableEvent;
};

TEST(DataConsumerTeeTest, CreateDone)
{
    OwnPtr<Handle> src(Handle::create());
    OwnPtr<WebDataConsumerHandle> dest1, dest2;

    src->add(Command(Command::Done));

    OwnPtr<TeeCreationThread> t = adoptPtr(new TeeCreationThread());
    t->run(src.release(), &dest1, &dest2);

    ASSERT_TRUE(dest1);
    ASSERT_TRUE(dest2);

    HandleReader r1, r2;
    r1.start(dest1.release());
    r2.start(dest2.release());

    r1.waitableEvent()->wait();
    r2.waitableEvent()->wait();

    EXPECT_EQ(kDone, r1.finalResult());
    EXPECT_EQ(String(), r1.readString());

    EXPECT_EQ(kDone, r2.finalResult());
    EXPECT_EQ(String(), r2.readString());
}

TEST(DataConsumerTeeTest, Read)
{
    OwnPtr<Handle> src(Handle::create());
    OwnPtr<WebDataConsumerHandle> dest1, dest2;

    src->add(Command(Command::Wait));
    src->add(Command(Command::Data, "hello, "));
    src->add(Command(Command::Wait));
    src->add(Command(Command::Data, "world"));
    src->add(Command(Command::Wait));
    src->add(Command(Command::Wait));
    src->add(Command(Command::Done));

    OwnPtr<TeeCreationThread> t = adoptPtr(new TeeCreationThread());
    t->run(src.release(), &dest1, &dest2);

    ASSERT_TRUE(dest1);
    ASSERT_TRUE(dest2);

    HandleReader r1, r2;
    r1.start(dest1.release());
    r2.start(dest2.release());

    r1.waitableEvent()->wait();
    r2.waitableEvent()->wait();

    EXPECT_EQ(kDone, r1.finalResult());
    EXPECT_EQ("hello, world", r1.readString());

    EXPECT_EQ(kDone, r2.finalResult());
    EXPECT_EQ("hello, world", r2.readString());
}

TEST(DataConsumerTeeTest, TwoPhaseRead)
{
    OwnPtr<Handle> src(Handle::create());
    OwnPtr<WebDataConsumerHandle> dest1, dest2;

    src->add(Command(Command::Wait));
    src->add(Command(Command::Data, "hello, "));
    src->add(Command(Command::Wait));
    src->add(Command(Command::Wait));
    src->add(Command(Command::Wait));
    src->add(Command(Command::Data, "world"));
    src->add(Command(Command::Wait));
    src->add(Command(Command::Done));

    OwnPtr<TeeCreationThread> t = adoptPtr(new TeeCreationThread());
    t->run(src.release(), &dest1, &dest2);

    ASSERT_TRUE(dest1);
    ASSERT_TRUE(dest2);

    HandleTwoPhaseReader r1, r2;
    r1.start(dest1.release());
    r2.start(dest2.release());

    r1.waitableEvent()->wait();
    r2.waitableEvent()->wait();

    EXPECT_EQ(kDone, r1.finalResult());
    EXPECT_EQ("hello, world", r1.readString());

    EXPECT_EQ(kDone, r2.finalResult());
    EXPECT_EQ("hello, world", r2.readString());
}

TEST(DataConsumerTeeTest, Error)
{
    OwnPtr<Handle> src(Handle::create());
    OwnPtr<WebDataConsumerHandle> dest1, dest2;

    src->add(Command(Command::Data, "hello, "));
    src->add(Command(Command::Data, "world"));
    src->add(Command(Command::Error));

    OwnPtr<TeeCreationThread> t = adoptPtr(new TeeCreationThread());
    t->run(src.release(), &dest1, &dest2);

    ASSERT_TRUE(dest1);
    ASSERT_TRUE(dest2);

    HandleReader r1, r2;
    r1.start(dest1.release());
    r2.start(dest2.release());

    r1.waitableEvent()->wait();
    r2.waitableEvent()->wait();

    EXPECT_EQ(kUnexpectedError, r1.finalResult());
    EXPECT_EQ(kUnexpectedError, r2.finalResult());
}

void postStop(Thread* thread)
{
    thread->executionContext()->stopActiveDOMObjects();
}

TEST(DataConsumerTeeTest, StopSource)
{
    OwnPtr<Handle> src(Handle::create());
    OwnPtr<WebDataConsumerHandle> dest1, dest2;

    src->add(Command(Command::Data, "hello, "));
    src->add(Command(Command::Data, "world"));

    OwnPtr<TeeCreationThread> t = adoptPtr(new TeeCreationThread());
    t->run(src.release(), &dest1, &dest2);

    ASSERT_TRUE(dest1);
    ASSERT_TRUE(dest2);

    HandleReader r1, r2;
    r1.start(dest1.release());
    r2.start(dest2.release());

    // We can pass a raw pointer because the subsequent |wait| calls ensure
    // t->thread() is alive.
    t->thread()->thread()->postTask(FROM_HERE, new Task(threadSafeBind(postStop, AllowCrossThreadAccess(t->thread()))));

    r1.waitableEvent()->wait();
    r2.waitableEvent()->wait();

    EXPECT_EQ(kUnexpectedError, r1.finalResult());
    EXPECT_EQ(kUnexpectedError, r2.finalResult());
}

TEST(DataConsumerTeeTest, DetachSource)
{
    OwnPtr<Handle> src(Handle::create());
    OwnPtr<WebDataConsumerHandle> dest1, dest2;

    src->add(Command(Command::Data, "hello, "));
    src->add(Command(Command::Data, "world"));

    OwnPtr<TeeCreationThread> t = adoptPtr(new TeeCreationThread());
    t->run(src.release(), &dest1, &dest2);

    ASSERT_TRUE(dest1);
    ASSERT_TRUE(dest2);

    HandleReader r1, r2;
    r1.start(dest1.release());
    r2.start(dest2.release());

    t = nullptr;

    r1.waitableEvent()->wait();
    r2.waitableEvent()->wait();

    EXPECT_EQ(kUnexpectedError, r1.finalResult());
    EXPECT_EQ(kUnexpectedError, r2.finalResult());
}

TEST(DataConsumerTeeTest, DetachSourceAfterReadingDone)
{
    OwnPtr<Handle> src(Handle::create());
    OwnPtr<WebDataConsumerHandle> dest1, dest2;

    src->add(Command(Command::Data, "hello, "));
    src->add(Command(Command::Data, "world"));
    src->add(Command(Command::Done));

    OwnPtr<TeeCreationThread> t = adoptPtr(new TeeCreationThread());
    t->run(src.release(), &dest1, &dest2);

    ASSERT_TRUE(dest1);
    ASSERT_TRUE(dest2);

    HandleReader r1, r2;
    r1.start(dest1.release());
    r1.waitableEvent()->wait();

    EXPECT_EQ(kDone, r1.finalResult());
    EXPECT_EQ("hello, world", r1.readString());

    t = nullptr;

    r2.start(dest2.release());
    r2.waitableEvent()->wait();

    EXPECT_EQ(kDone, r2.finalResult());
    EXPECT_EQ("hello, world", r2.readString());
}

TEST(DataConsumerTeeTest, DetachOneDestination)
{
    OwnPtr<Handle> src(Handle::create());
    OwnPtr<WebDataConsumerHandle> dest1, dest2;

    src->add(Command(Command::Data, "hello, "));
    src->add(Command(Command::Data, "world"));
    src->add(Command(Command::Done));

    OwnPtr<TeeCreationThread> t = adoptPtr(new TeeCreationThread());
    t->run(src.release(), &dest1, &dest2);

    ASSERT_TRUE(dest1);
    ASSERT_TRUE(dest2);

    dest1 = nullptr;

    HandleReader r2;
    r2.start(dest2.release());
    r2.waitableEvent()->wait();

    EXPECT_EQ(kDone, r2.finalResult());
    EXPECT_EQ("hello, world", r2.readString());
}

TEST(DataConsumerTeeTest, DetachBothDestinationsShouldStopSourceReader)
{
    OwnPtr<Handle> src(Handle::create());
    RefPtr<Handle::Context> context(src->context());
    OwnPtr<WebDataConsumerHandle> dest1, dest2;

    src->add(Command(Command::Data, "hello, "));
    src->add(Command(Command::Data, "world"));

    OwnPtr<TeeCreationThread> t = adoptPtr(new TeeCreationThread());
    t->run(src.release(), &dest1, &dest2);

    ASSERT_TRUE(dest1);
    ASSERT_TRUE(dest2);

    dest1 = nullptr;
    dest2 = nullptr;

    // Collect garbage to finalize the source reader.
    Heap::collectAllGarbage();
    context->detached()->wait();
}

} // namespace
} // namespace blink
