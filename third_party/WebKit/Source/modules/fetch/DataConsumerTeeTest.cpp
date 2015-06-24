// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/fetch/DataConsumerTee.h"

#include "bindings/core/v8/ScriptState.h"
#include "core/testing/DummyPageHolder.h"
#include "core/testing/NullExecutionContext.h"
#include "gin/public/isolate_holder.h"
#include "platform/Task.h"
#include "platform/ThreadSafeFunctional.h"
#include "platform/WebThreadSupportingGC.h"
#include "public/platform/Platform.h"
#include "public/platform/WebThread.h"
#include "public/platform/WebTraceLocation.h"
#include "public/platform/WebWaitableEvent.h"
#include "wtf/Deque.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefPtr.h"
#include "wtf/ThreadSafeRefCounted.h"
#include "wtf/ThreadingPrimitives.h"
#include "wtf/Vector.h"

#include <gtest/gtest.h>
#include <string.h>
#include <v8.h>

namespace blink {
namespace {

using Result = WebDataConsumerHandle::Result;
const WebDataConsumerHandle::Flags kNone = WebDataConsumerHandle::FlagNone;
const Result kOk = WebDataConsumerHandle::Ok;
const Result kShouldWait = WebDataConsumerHandle::ShouldWait;
const Result kDone = WebDataConsumerHandle::Done;
const Result kUnexpectedError = WebDataConsumerHandle::UnexpectedError;

class Command final {
public:
    enum Name {
        Data,
        Done,
        Error,
        Wait,
    };

    Command(Name name) : m_name(name) { }
    Command(Name name, const Vector<char>& body) : m_name(name), m_body(body) { }
    Command(Name name, const char* body, size_t size) : m_name(name)
    {
        m_body.append(body, size);
    }
    Command(Name name, const char* body) : Command(name, body, strlen(body)) { }
    Name name() const { return m_name; }
    const Vector<char>& body() const { return m_body; }

private:
    const Name m_name;
    Vector<char> m_body;
};

// Handle stores commands via |add| and replays the stored commends when read.
class Handle final : public WebDataConsumerHandle {
public:
    class Context final : public ThreadSafeRefCounted<Context> {
    public:
        static PassRefPtr<Context> create() { return adoptRef(new Context); }

        // This function cannot be called after creating a tee.
        void add(const Command& command)
        {
            MutexLocker locker(m_mutex);
            m_commands.append(command);
        }

        void attachReader(WebDataConsumerHandle::Client* client)
        {
            MutexLocker locker(m_mutex);
            ASSERT(!m_readerThread);
            ASSERT(!m_client);
            m_readerThread = Platform::current()->currentThread();
            m_client = client;

            if (m_client && !(isEmpty() && m_result == kShouldWait))
                notify();
        }
        void detachReader()
        {
            MutexLocker locker(m_mutex);
            ASSERT(m_readerThread && m_readerThread->isCurrentThread());
            m_readerThread = nullptr;
            m_client = nullptr;
            if (!m_isHandleAttached)
                m_detached->signal();
        }

        void detachHandle()
        {
            MutexLocker locker(m_mutex);
            m_isHandleAttached = false;
            if (!m_readerThread)
                m_detached->signal();
        }

        Result beginRead(const void** buffer, Flags, size_t* available)
        {
            MutexLocker locker(m_mutex);
            *buffer = nullptr;
            *available = 0;
            if (isEmpty())
                return m_result;

            const Command& command = top();
            Result result = Ok;
            switch (command.name()) {
            case Command::Data: {
                auto& body = command.body();
                *available = body.size() - offset();
                *buffer = body.data() + offset();
                result = Ok;
                break;
            }
            case Command::Done:
                m_result = result = Done;
                consume(0);
                break;
            case Command::Wait:
                consume(0);
                result = ShouldWait;
                notify();
                break;
            case Command::Error:
                m_result = result = UnexpectedError;
                consume(0);
                break;
            }
            return result;
        }
        Result endRead(size_t readSize)
        {
            MutexLocker locker(m_mutex);
            consume(readSize);
            return Ok;
        }

        WebWaitableEvent* detached() { return m_detached.get(); }

    private:
        Context()
            : m_offset(0)
            , m_readerThread(nullptr)
            , m_client(nullptr)
            , m_result(ShouldWait)
            , m_isHandleAttached(true)
            , m_detached(adoptPtr(Platform::current()->createWaitableEvent()))
        {
        }

        bool isEmpty() const { return m_commands.isEmpty(); }
        const Command& top()
        {
            ASSERT(!isEmpty());
            return m_commands.first();
        }

        void consume(size_t size)
        {
            ASSERT(!isEmpty());
            ASSERT(size + m_offset <= top().body().size());
            bool fullyConsumed = (size + m_offset >= top().body().size());
            if (fullyConsumed) {
                m_offset = 0;
                m_commands.removeFirst();
            } else {
                m_offset += size;
            }
        }

        size_t offset() const { return m_offset; }

        void notify()
        {
            if (!m_client)
                return;
            ASSERT(m_readerThread);
            m_readerThread->postTask(FROM_HERE, new Task(threadSafeBind(&Context::notifyInternal, this)));
        }

        void notifyInternal()
        {
            {
                MutexLocker locker(m_mutex);
                if (!m_client || !m_readerThread->isCurrentThread()) {
                    // There is no client, or a new reader is attached.
                    return;
                }
            }
            // The reading thread is the current thread.
            m_client->didGetReadable();
        }

        Deque<Command> m_commands;
        size_t m_offset;
        WebThread* m_readerThread;
        Client* m_client;
        Result m_result;
        bool m_isHandleAttached;
        Mutex m_mutex;
        OwnPtr<WebWaitableEvent> m_detached;
    };

    class ReaderImpl final : public Reader {
    public:
        ReaderImpl(PassRefPtr<Context> context, Client* client)
            : m_context(context)
        {
            m_context->attachReader(client);
        }
        ~ReaderImpl()
        {
            m_context->detachReader();
        }

        Result read(void* buffer, size_t size, Flags flags, size_t* readSize) override
        {
            const void* src = nullptr;
            Result result = beginRead(&src, flags, readSize);
            if (result != Ok)
                return result;
            *readSize = std::min(*readSize, size);
            memcpy(buffer, src, *readSize);
            return endRead(*readSize);
        }
        Result beginRead(const void** buffer, Flags flags, size_t* available) override
        {
            return m_context->beginRead(buffer, flags, available);
        }
        Result endRead(size_t readSize) override
        {
            return m_context->endRead(readSize);
        }

    private:
        RefPtr<Context> m_context;
    };

    Handle() : m_context(Context::create()) { }
    ~Handle()
    {
        m_context->detachHandle();
    }

    ReaderImpl* obtainReaderInternal(Client* client) override { return new ReaderImpl(m_context, client); }

    // Add a command to this handle. This function must be called on the
    // creator thread. This function must be called BEFORE any reader is
    // obtained.
    void add(const Command& command)
    {
        m_context->add(command);
    }

    Context* context() { return m_context.get(); };

private:
    RefPtr<Context> m_context;
};

class TestingThread final {
public:
    explicit TestingThread(const char* name)
        : m_thread(WebThreadSupportingGC::create(name))
        , m_waitableEvent(adoptPtr(Platform::current()->createWaitableEvent()))
    {
        m_thread->postTask(FROM_HERE, new Task(threadSafeBind(&TestingThread::initialize, AllowCrossThreadAccess(this))));
        m_waitableEvent->wait();
    }

    ~TestingThread()
    {
        m_thread->postTask(FROM_HERE, new Task(threadSafeBind(&TestingThread::shutdown, AllowCrossThreadAccess(this))));
        m_waitableEvent->wait();
    }

    WebThreadSupportingGC* thread() { return m_thread.get(); }
    ExecutionContext* executionContext() { return m_executionContext.get(); }
    ScriptState* scriptState() { return m_scriptState.get(); }
    v8::Isolate* isolate() { return m_isolateHolder->isolate(); }

private:
    void initialize()
    {
        m_isolateHolder = adoptPtr(new gin::IsolateHolder());
        m_thread->initialize();
        isolate()->Enter();
        v8::HandleScope handleScope(isolate());
        v8::Local<v8::Context> context = v8::Context::New(isolate());
        m_scriptState = ScriptState::create(context, DOMWrapperWorld::create(isolate()));
        m_executionContext = adoptRefWillBeNoop(new NullExecutionContext());
        m_waitableEvent->signal();
    }

    void shutdown()
    {
        m_executionContext = nullptr;
        m_scriptState = nullptr;
        isolate()->Exit();
        m_thread->shutdown();
        m_isolateHolder = nullptr;
        m_waitableEvent->signal();
    }

    OwnPtr<WebThreadSupportingGC> m_thread;
    OwnPtr<WebWaitableEvent> m_waitableEvent;
    RefPtrWillBePersistent<NullExecutionContext> m_executionContext;
    OwnPtr<gin::IsolateHolder> m_isolateHolder;
    RefPtr<ScriptState> m_scriptState;
};

class HandleReader : public WebDataConsumerHandle::Client {
public:
    HandleReader() : m_finalResult(kOk) { }

    // Need to wait for the event signal after this function is called.
    void start(PassOwnPtr<WebDataConsumerHandle> handle)
    {
        m_thread = adoptPtr(new TestingThread("reading thread"));
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

    OwnPtr<TestingThread> m_thread;
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
        m_thread = adoptPtr(new TestingThread("reading thread"));
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

    OwnPtr<TestingThread> m_thread;
    OwnPtr<WebDataConsumerHandle::Reader> m_reader;
    String m_readString;
    Result m_finalResult;
    OwnPtr<WebWaitableEvent> m_waitableEvent;
};

class TeeCreationThread {
public:
    void run(PassOwnPtr<WebDataConsumerHandle> src, OwnPtr<WebDataConsumerHandle>* dest1, OwnPtr<WebDataConsumerHandle>* dest2)
    {
        m_thread = adoptPtr(new TestingThread("src thread"));
        m_waitableEvent = adoptPtr(Platform::current()->createWaitableEvent());
        m_thread->thread()->postTask(FROM_HERE, new Task(threadSafeBind(&TeeCreationThread::runInternal, AllowCrossThreadAccess(this), src, AllowCrossThreadAccess(dest1), AllowCrossThreadAccess(dest2))));
        m_waitableEvent->wait();
    }

    TestingThread* thread() { return m_thread.get(); }

private:
    void runInternal(PassOwnPtr<WebDataConsumerHandle> src, OwnPtr<WebDataConsumerHandle>* dest1, OwnPtr<WebDataConsumerHandle>* dest2)
    {
        DataConsumerTee::create(m_thread->executionContext(), src, dest1, dest2);
        m_waitableEvent->signal();
    }

    OwnPtr<TestingThread> m_thread;
    OwnPtr<WebWaitableEvent> m_waitableEvent;
};

TEST(DataConsumerTeeTest, CreateDone)
{
    OwnPtr<Handle> src(adoptPtr(new Handle));
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
    OwnPtr<Handle> src(adoptPtr(new Handle));
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
    OwnPtr<Handle> src(adoptPtr(new Handle));
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
    OwnPtr<Handle> src(adoptPtr(new Handle));
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

void postStop(TestingThread* thread)
{
    thread->executionContext()->stopActiveDOMObjects();
}

TEST(DataConsumerTeeTest, StopSource)
{
    OwnPtr<Handle> src(adoptPtr(new Handle));
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
    OwnPtr<Handle> src(adoptPtr(new Handle));
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
    OwnPtr<Handle> src(adoptPtr(new Handle));
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
    OwnPtr<Handle> src(adoptPtr(new Handle));
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
    OwnPtr<Handle> src(adoptPtr(new Handle));
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
