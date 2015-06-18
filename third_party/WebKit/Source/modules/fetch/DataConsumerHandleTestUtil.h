// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DataConsumerHandleTestUtil_h
#define DataConsumerHandleTestUtil_h

#include "modules/fetch/DataConsumerHandleUtil.h"
#include "modules/fetch/FetchDataConsumerHandle.h"
#include "platform/Task.h"
#include "platform/ThreadSafeFunctional.h"
#include "platform/heap/Handle.h"
#include "public/platform/Platform.h"
#include "public/platform/WebDataConsumerHandle.h"
#include "public/platform/WebThread.h"
#include "public/platform/WebTraceLocation.h"
#include "public/platform/WebWaitableEvent.h"
#include "wtf/Locker.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace blink {

class DataConsumerHandleTestUtil {
public:
    class NoopClient final : public WebDataConsumerHandle::Client {
    public:
        void didGetReadable() override { }
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
            WebThread* readingThread() { return m_readingThread.get(); }
            WebThread* updatingThread() { return m_updatingThread.get(); }

        private:
            Context()
                : m_readingThread(adoptPtr(Platform::current()->createThread("reading thread")))
                , m_updatingThread(adoptPtr(Platform::current()->createThread("updating thread")))
            {
            }
            String currentThreadName()
            {
                if (m_readingThread->isCurrentThread())
                    return "the reading thread";
                if (m_updatingThread->isCurrentThread())
                    return "the updating thread";
                return "an unknown thread";
            }

            OwnPtr<WebThread> m_readingThread;
            OwnPtr<WebThread> m_updatingThread;
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
        WebThread* readingThread() { return m_context->readingThread(); }
        WebThread* updatingThread() { return m_context->updatingThread(); }

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

            readingThread()->postTask(FROM_HERE, new Task(threadSafeBind(&Self::obtainReader, this)));

            m_waitableEvent->wait();
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

            readingThread()->postTask(FROM_HERE, new Task(threadSafeBind(&Self::obtainReader, this)));

            m_waitableEvent->wait();
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
};

} // namespace blink

#endif // DataConsumerHandleTestUtil_h
