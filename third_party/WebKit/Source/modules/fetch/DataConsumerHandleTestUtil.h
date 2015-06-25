// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DataConsumerHandleTestUtil_h
#define DataConsumerHandleTestUtil_h

#include "bindings/core/v8/ScriptState.h"
#include "core/testing/NullExecutionContext.h"
#include "gin/public/isolate_holder.h"
#include "modules/fetch/DataConsumerHandleUtil.h"
#include "modules/fetch/FetchDataConsumerHandle.h"
#include "modules/fetch/FetchDataLoader.h"
#include "platform/Task.h"
#include "platform/ThreadSafeFunctional.h"
#include "platform/WebThreadSupportingGC.h"
#include "platform/heap/Handle.h"
#include "public/platform/Platform.h"
#include "public/platform/WebDataConsumerHandle.h"
#include "public/platform/WebTraceLocation.h"
#include "public/platform/WebWaitableEvent.h"
#include "wtf/Deque.h"
#include "wtf/Locker.h"
#include "wtf/ThreadSafeRefCounted.h"
#include "wtf/ThreadingPrimitives.h"
#include "wtf/Vector.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <v8.h>

namespace blink {

class DataConsumerHandleTestUtil {
public:
    class NoopClient final : public WebDataConsumerHandle::Client {
    public:
        void didGetReadable() override { }
    };

    // Thread has a WebThreadSupportingGC. It initializes / shutdowns
    // additional objects based on the given policy. The constructor and the
    // destructor blocks during the setup and the teardown.
    class Thread final {
    public:
        // Initialization policy of a thread.
        enum InitializationPolicy {
            // Only garbage collection is supported.
            GarbageCollection,
            // Creating an isolate in addition to GarbageCollection.
            ScriptExecution,
            // Creating an execution context in addition to ScriptExecution.
            WithExecutionContext,
        };

        Thread(const char* name, InitializationPolicy = GarbageCollection);
        ~Thread();

        WebThreadSupportingGC* thread() { return m_thread.get(); }
        ExecutionContext* executionContext() { return m_executionContext.get(); }
        ScriptState* scriptState() { return m_scriptState.get(); }
        v8::Isolate* isolate() { return m_isolateHolder->isolate(); }

    private:
        void initialize();
        void shutdown();

        OwnPtr<WebThreadSupportingGC> m_thread;
        const InitializationPolicy m_initializationPolicy;
        OwnPtr<WebWaitableEvent> m_waitableEvent;
        RefPtrWillBePersistent<NullExecutionContext> m_executionContext;
        OwnPtr<gin::IsolateHolder> m_isolateHolder;
        RefPtr<ScriptState> m_scriptState;
    };

    class ThreadingTestBase : public ThreadSafeRefCounted<ThreadingTestBase> {
    public:
        class Context : public ThreadSafeRefCounted<Context> {
        public:
            static PassRefPtr<Context> create() { return adoptRef(new Context); }
            void recordAttach(const String& handle)
            {
                MutexLocker locker(m_loggingMutex);
                m_result.append("A reader is attached to " + handle + " on " + currentThreadName() + ".\n");
            }
            void recordDetach(const String& handle)
            {
                MutexLocker locker(m_loggingMutex);
                m_result.append("A reader is detached from " + handle + " on " + currentThreadName() + ".\n");
            }

            const String& result()
            {
                MutexLocker locker(m_loggingMutex);
                return m_result;
            }
            WebThreadSupportingGC* readingThread() { return m_readingThread->thread(); }
            WebThreadSupportingGC* updatingThread() { return m_updatingThread->thread(); }

        private:
            Context()
                : m_readingThread(adoptPtr(new Thread("reading thread")))
                , m_updatingThread(adoptPtr(new Thread("updating thread")))
            {
            }
            String currentThreadName()
            {
                if (m_readingThread->thread()->isCurrentThread())
                    return "the reading thread";
                if (m_updatingThread->thread()->isCurrentThread())
                    return "the updating thread";
                return "an unknown thread";
            }

            OwnPtr<Thread> m_readingThread;
            OwnPtr<Thread> m_updatingThread;
            Mutex m_loggingMutex;
            String m_result;
        };

        class ReaderImpl final : public WebDataConsumerHandle::Reader {
        public:
            ReaderImpl(const String& name, PassRefPtr<Context> context) : m_name(name.isolatedCopy()), m_context(context)
            {
                m_context->recordAttach(m_name.isolatedCopy());
            }
            ~ReaderImpl() override { m_context->recordDetach(m_name.isolatedCopy()); }

            using Result = WebDataConsumerHandle::Result;
            using Flags = WebDataConsumerHandle::Flags;
            Result read(void*, size_t, Flags, size_t*) override { return WebDataConsumerHandle::ShouldWait; }
            Result beginRead(const void**, Flags, size_t*) override { return WebDataConsumerHandle::ShouldWait; }
            Result endRead(size_t) override { return WebDataConsumerHandle::UnexpectedError; }

        private:
            const String m_name;
            RefPtr<Context> m_context;
        };
        class DataConsumerHandle final : public WebDataConsumerHandle {
        public:
            DataConsumerHandle(const String& name, PassRefPtr<Context> context) : m_name(name.isolatedCopy()), m_context(context) { }

        private:
            Reader* obtainReaderInternal(Client*) { return new ReaderImpl(m_name, m_context); }
            const String m_name;
            RefPtr<Context> m_context;
        };

        void resetReader() { m_reader = nullptr; }
        void signalDone() { m_waitableEvent->signal(); }
        const String& result() { return m_context->result(); }
        WebThreadSupportingGC* readingThread() { return m_context->readingThread(); }
        WebThreadSupportingGC* updatingThread() { return m_context->updatingThread(); }
        void postTaskAndWait(WebThreadSupportingGC* thread, const WebTraceLocation& location, Task* task)
        {
            thread->postTask(location, task);
            m_waitableEvent->wait();
        }

    protected:
        RefPtr<Context> m_context;
        OwnPtr<WebDataConsumerHandle::Reader> m_reader;
        OwnPtr<WebWaitableEvent> m_waitableEvent;
        NoopClient m_client;
    };

    class ThreadingHandleNotificationTest : public ThreadingTestBase, public WebDataConsumerHandle::Client {
    public:
        using Self = ThreadingHandleNotificationTest;
        void run(PassOwnPtr<WebDataConsumerHandle> handle)
        {
            m_context = Context::create();
            m_waitableEvent = adoptPtr(Platform::current()->createWaitableEvent());
            m_handle = handle;

            postTaskAndWait(readingThread(), FROM_HERE, new Task(threadSafeBind(&Self::obtainReader, this)));
        }

    private:
        void obtainReader()
        {
            m_reader = m_handle->obtainReader(this);
        }
        void didGetReadable() override
        {
            readingThread()->postTask(FROM_HERE, new Task(threadSafeBind(&Self::resetReader, this)));
            readingThread()->postTask(FROM_HERE, new Task(threadSafeBind(&Self::signalDone, this)));
        }

        OwnPtr<WebDataConsumerHandle> m_handle;
    };

    class ThreadingHandleNoNotificationTest : public ThreadingTestBase, public WebDataConsumerHandle::Client {
    public:
        using Self = ThreadingHandleNoNotificationTest;
        void run(PassOwnPtr<WebDataConsumerHandle> handle)
        {
            m_context = Context::create();
            m_waitableEvent = adoptPtr(Platform::current()->createWaitableEvent());
            m_handle = handle;

            postTaskAndWait(readingThread(), FROM_HERE, new Task(threadSafeBind(&Self::obtainReader, this)));
        }

    private:
        void obtainReader()
        {
            m_reader = m_handle->obtainReader(this);
            m_reader = nullptr;
            readingThread()->postTask(FROM_HERE, new Task(threadSafeBind(&Self::signalDone, this)));
        }
        void didGetReadable() override
        {
            ASSERT_NOT_REACHED();
        }

        OwnPtr<WebDataConsumerHandle> m_handle;
    };

    class MockFetchDataLoaderClient : public GarbageCollectedFinalized<MockFetchDataLoaderClient>, public FetchDataLoader::Client {
        USING_GARBAGE_COLLECTED_MIXIN(MockFetchDataLoaderClient);
    public:
        static ::testing::StrictMock<MockFetchDataLoaderClient>* create() { return new ::testing::StrictMock<MockFetchDataLoaderClient>; }

        DEFINE_INLINE_VIRTUAL_TRACE()
        {
            FetchDataLoader::Client::trace(visitor);
        }

        MOCK_METHOD1(didFetchDataLoadedBlobHandleMock, void(RefPtr<BlobDataHandle>));
        MOCK_METHOD1(didFetchDataLoadedArrayBufferMock, void(RefPtr<DOMArrayBuffer>));
        MOCK_METHOD1(didFetchDataLoadedString, void(const String&));
        MOCK_METHOD0(didFetchDataLoadStream, void());
        MOCK_METHOD0(didFetchDataLoadFailed, void());

        // In mock methods we use RefPtr<> rather than PassRefPtr<>.
        void didFetchDataLoadedArrayBuffer(PassRefPtr<DOMArrayBuffer> arrayBuffer) override
        {
            didFetchDataLoadedArrayBufferMock(arrayBuffer);
        }
        void didFetchDataLoadedBlobHandle(PassRefPtr<BlobDataHandle> blobDataHandle) override
        {
            didFetchDataLoadedBlobHandleMock(blobDataHandle);
        }
    };

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

    // ReplayingHandle stores commands via |add| and replays the stored commends when read.
    class ReplayingHandle final : public WebDataConsumerHandle {
    public:
        static PassOwnPtr<ReplayingHandle> create() { return adoptPtr(new ReplayingHandle()); }
        ~ReplayingHandle();

        // Add a command to this handle. This function must be called on the
        // creator thread. This function must be called BEFORE any reader is
        // obtained.
        void add(const Command&);

        class Context final : public ThreadSafeRefCounted<Context> {
        public:
            static PassRefPtr<Context> create() { return adoptRef(new Context); }

            // This function cannot be called after creating a tee.
            void add(const Command&);
            void attachReader(WebDataConsumerHandle::Client*);
            void detachReader();
            void detachHandle();
            Result beginRead(const void** buffer, Flags, size_t* available);
            Result endRead(size_t readSize);
            WebWaitableEvent* detached() { return m_detached.get(); }

        private:
            Context();
            bool isEmpty() const { return m_commands.isEmpty(); }
            const Command& top();
            void consume(size_t);
            size_t offset() const { return m_offset; }
            void notify();
            void notifyInternal();

            Deque<Command> m_commands;
            size_t m_offset;
            WebThread* m_readerThread;
            Client* m_client;
            Result m_result;
            bool m_isHandleAttached;
            Mutex m_mutex;
            OwnPtr<WebWaitableEvent> m_detached;
        };

        Context* context() { return m_context.get(); }

    private:
        class ReaderImpl;

        ReplayingHandle();
        Reader* obtainReaderInternal(Client*) override;

        RefPtr<Context> m_context;
    };

};

} // namespace blink

#endif // DataConsumerHandleTestUtil_h
