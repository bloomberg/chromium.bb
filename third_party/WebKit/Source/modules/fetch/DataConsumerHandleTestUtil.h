// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DataConsumerHandleTestUtil_h
#define DataConsumerHandleTestUtil_h

#include <memory>

#include "core/testing/NullExecutionContext.h"
#include "gin/public/isolate_holder.h"
#include "platform/WaitableEvent.h"
#include "platform/WebThreadSupportingGC.h"
#include "platform/bindings/ScriptState.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Deque.h"
#include "platform/wtf/Locker.h"
#include "platform/wtf/PtrUtil.h"
#include "platform/wtf/ThreadSafeRefCounted.h"
#include "platform/wtf/ThreadingPrimitives.h"
#include "platform/wtf/Vector.h"
#include "platform/wtf/text/StringBuilder.h"
#include "public/platform/Platform.h"
#include "public/platform/WebDataConsumerHandle.h"
#include "public/platform/WebTraceLocation.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "v8/include/v8.h"

namespace blink {

class DataConsumerHandleTestUtil {
  STATIC_ONLY(DataConsumerHandleTestUtil);

 public:
  class NoopClient final : public WebDataConsumerHandle::Client {
    DISALLOW_NEW();

   public:
    void DidGetReadable() override {}
  };

  // Thread has a WebThreadSupportingGC. It initializes / shutdowns
  // additional objects based on the given policy. The constructor and the
  // destructor blocks during the setup and the teardown.
  class Thread final {
    USING_FAST_MALLOC(Thread);

   public:
    // Initialization policy of a thread.
    enum InitializationPolicy {
      // Only garbage collection is supported.
      kGarbageCollection,
      // Creating an isolate in addition to GarbageCollection.
      kScriptExecution,
      // Creating an execution context in addition to ScriptExecution.
      kWithExecutionContext,
    };

    Thread(const char* name, InitializationPolicy = kGarbageCollection);
    ~Thread();

    WebThreadSupportingGC* GetThread() { return thread_.get(); }
    ExecutionContext* GetExecutionContext() { return execution_context_.Get(); }
    ScriptState* GetScriptState() { return script_state_.get(); }
    v8::Isolate* GetIsolate() { return isolate_holder_->isolate(); }

   private:
    void Initialize();
    void Shutdown();

    std::unique_ptr<WebThreadSupportingGC> thread_;
    const InitializationPolicy initialization_policy_;
    std::unique_ptr<WaitableEvent> waitable_event_;
    Persistent<NullExecutionContext> execution_context_;
    std::unique_ptr<gin::IsolateHolder> isolate_holder_;
    scoped_refptr<ScriptState> script_state_;
  };

  class ThreadingTestBase : public ThreadSafeRefCounted<ThreadingTestBase> {
   public:
    virtual ~ThreadingTestBase() {}

    class ThreadHolder;

    class Context : public ThreadSafeRefCounted<Context> {
     public:
      static scoped_refptr<Context> Create() {
        return WTF::AdoptRef(new Context);
      }
      void RecordAttach(const String& handle) {
        MutexLocker locker(logging_mutex_);
        result_.Append("A reader is attached to ");
        result_.Append(handle);
        result_.Append(" on ");
        result_.Append(CurrentThreadName());
        result_.Append(".\n");
      }
      void RecordDetach(const String& handle) {
        MutexLocker locker(logging_mutex_);
        result_.Append("A reader is detached from ");
        result_.Append(handle);
        result_.Append(" on ");
        result_.Append(CurrentThreadName());
        result_.Append(".\n");
      }

      String Result() {
        MutexLocker locker(logging_mutex_);
        return result_.ToString();
      }

      void RegisterThreadHolder(ThreadHolder* holder) {
        MutexLocker locker(holder_mutex_);
        DCHECK(!holder_);
        holder_ = holder;
      }
      void UnregisterThreadHolder() {
        MutexLocker locker(holder_mutex_);
        DCHECK(holder_);
        holder_ = nullptr;
      }
      void PostTaskToReadingThread(const WebTraceLocation& location,
                                   CrossThreadClosure task) {
        MutexLocker locker(holder_mutex_);
        DCHECK(holder_);
        holder_->ReadingThread()->PostTask(location, std::move(task));
      }
      void PostTaskToUpdatingThread(const WebTraceLocation& location,
                                    CrossThreadClosure task) {
        MutexLocker locker(holder_mutex_);
        DCHECK(holder_);
        holder_->UpdatingThread()->PostTask(location, std::move(task));
      }

     private:
      Context() : holder_(nullptr) {}
      String CurrentThreadName() {
        MutexLocker locker(holder_mutex_);
        if (holder_) {
          if (holder_->ReadingThread()->IsCurrentThread())
            return "the reading thread";
          if (holder_->UpdatingThread()->IsCurrentThread())
            return "the updating thread";
        }
        return "an unknown thread";
      }

      // Protects |m_result|.
      Mutex logging_mutex_;
      StringBuilder result_;

      // Protects |m_holder|.
      Mutex holder_mutex_;
      // Because Context outlives ThreadHolder, holding a raw pointer
      // here is safe.
      ThreadHolder* holder_;
    };

    // The reading/updating threads are alive while ThreadHolder is alive.
    class ThreadHolder {
      DISALLOW_NEW();

     public:
      ThreadHolder(ThreadingTestBase* test)
          : context_(test->context_),
            reading_thread_(WTF::WrapUnique(new Thread("reading thread"))),
            updating_thread_(WTF::WrapUnique(new Thread("updating thread"))) {
        context_->RegisterThreadHolder(this);
      }
      ~ThreadHolder() { context_->UnregisterThreadHolder(); }

      WebThreadSupportingGC* ReadingThread() {
        return reading_thread_->GetThread();
      }
      WebThreadSupportingGC* UpdatingThread() {
        return updating_thread_->GetThread();
      }

     private:
      scoped_refptr<Context> context_;
      std::unique_ptr<Thread> reading_thread_;
      std::unique_ptr<Thread> updating_thread_;
    };

    class ReaderImpl final : public WebDataConsumerHandle::Reader {
      USING_FAST_MALLOC(ReaderImpl);

     public:
      ReaderImpl(const String& name, scoped_refptr<Context> context)
          : name_(name.IsolatedCopy()), context_(std::move(context)) {
        context_->RecordAttach(name_.IsolatedCopy());
      }
      ~ReaderImpl() override { context_->RecordDetach(name_.IsolatedCopy()); }

      using Result = WebDataConsumerHandle::Result;
      using Flags = WebDataConsumerHandle::Flags;
      Result BeginRead(const void**, Flags, size_t*) override {
        return WebDataConsumerHandle::kShouldWait;
      }
      Result EndRead(size_t) override {
        return WebDataConsumerHandle::kUnexpectedError;
      }

     private:
      const String name_;
      scoped_refptr<Context> context_;
    };
    class DataConsumerHandle final : public WebDataConsumerHandle {
      USING_FAST_MALLOC(DataConsumerHandle);

     public:
      static std::unique_ptr<WebDataConsumerHandle> Create(
          const String& name,
          scoped_refptr<Context> context) {
        return WTF::WrapUnique(
            new DataConsumerHandle(name, std::move(context)));
      }

     private:
      DataConsumerHandle(const String& name, scoped_refptr<Context> context)
          : name_(name.IsolatedCopy()), context_(std::move(context)) {}

      std::unique_ptr<Reader> ObtainReader(Client*) {
        return WTF::MakeUnique<ReaderImpl>(name_, context_);
      }
      const char* DebugName() const override {
        return "ThreadingTestBase::DataConsumerHandle";
      }

      const String name_;
      scoped_refptr<Context> context_;
    };

    void ResetReader() { reader_ = nullptr; }
    void SignalDone() { waitable_event_->Signal(); }
    String Result() { return context_->Result(); }
    void PostTaskToReadingThread(const WebTraceLocation& location,
                                 CrossThreadClosure task) {
      context_->PostTaskToReadingThread(location, std::move(task));
    }
    void PostTaskToUpdatingThread(const WebTraceLocation& location,
                                  CrossThreadClosure task) {
      context_->PostTaskToUpdatingThread(location, std::move(task));
    }
    void PostTaskToReadingThreadAndWait(const WebTraceLocation& location,
                                        CrossThreadClosure task) {
      PostTaskToReadingThread(location, std::move(task));
      waitable_event_->Wait();
    }
    void PostTaskToUpdatingThreadAndWait(const WebTraceLocation& location,
                                         CrossThreadClosure task) {
      PostTaskToUpdatingThread(location, std::move(task));
      waitable_event_->Wait();
    }

   protected:
    ThreadingTestBase() : context_(Context::Create()) {}

    scoped_refptr<Context> context_;
    std::unique_ptr<WebDataConsumerHandle::Reader> reader_;
    std::unique_ptr<WaitableEvent> waitable_event_;
    NoopClient client_;
  };

  class ThreadingHandleNotificationTest : public ThreadingTestBase,
                                          public WebDataConsumerHandle::Client {
   public:
    using Self = ThreadingHandleNotificationTest;
    static scoped_refptr<Self> Create() { return WTF::AdoptRef(new Self); }

    void Run(std::unique_ptr<WebDataConsumerHandle> handle) {
      ThreadHolder holder(this);
      waitable_event_ = WTF::MakeUnique<WaitableEvent>();
      handle_ = std::move(handle);

      PostTaskToReadingThreadAndWait(
          BLINK_FROM_HERE,
          CrossThreadBind(&Self::ObtainReader, WrapRefCounted(this)));
    }

   private:
    ThreadingHandleNotificationTest() = default;
    void ObtainReader() { reader_ = handle_->ObtainReader(this); }
    void DidGetReadable() override {
      PostTaskToReadingThread(
          BLINK_FROM_HERE,
          CrossThreadBind(&Self::ResetReader, WrapRefCounted(this)));
      PostTaskToReadingThread(
          BLINK_FROM_HERE,
          CrossThreadBind(&Self::SignalDone, WrapRefCounted(this)));
    }

    std::unique_ptr<WebDataConsumerHandle> handle_;
  };

  class ThreadingHandleNoNotificationTest
      : public ThreadingTestBase,
        public WebDataConsumerHandle::Client {
   public:
    using Self = ThreadingHandleNoNotificationTest;
    static scoped_refptr<Self> Create() { return WTF::AdoptRef(new Self); }

    void Run(std::unique_ptr<WebDataConsumerHandle> handle) {
      ThreadHolder holder(this);
      waitable_event_ = WTF::MakeUnique<WaitableEvent>();
      handle_ = std::move(handle);

      PostTaskToReadingThreadAndWait(
          BLINK_FROM_HERE,
          CrossThreadBind(&Self::ObtainReader, WrapRefCounted(this)));
    }

   private:
    ThreadingHandleNoNotificationTest() = default;
    void ObtainReader() {
      reader_ = handle_->ObtainReader(this);
      reader_ = nullptr;
      PostTaskToReadingThread(
          BLINK_FROM_HERE,
          CrossThreadBind(&Self::SignalDone, WrapRefCounted(this)));
    }
    void DidGetReadable() override { NOTREACHED(); }

    std::unique_ptr<WebDataConsumerHandle> handle_;
  };

  class Command final {
    DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();

   public:
    enum Name {
      kData,
      kDone,
      kError,
      kWait,
    };

    Command(Name name) : name_(name) {}
    Command(Name name, const Vector<char>& body) : name_(name), body_(body) {}
    Command(Name name, const char* body, size_t size) : name_(name) {
      body_.Append(body, size);
    }
    Command(Name name, const char* body) : Command(name, body, strlen(body)) {}
    Name GetName() const { return name_; }
    const Vector<char>& Body() const { return body_; }

   private:
    const Name name_;
    Vector<char> body_;
  };

  // ReplayingHandle stores commands via |add| and replays the stored commends
  // when read.
  class ReplayingHandle final : public WebDataConsumerHandle {
    USING_FAST_MALLOC(ReplayingHandle);

   public:
    static std::unique_ptr<ReplayingHandle> Create() {
      return WTF::WrapUnique(new ReplayingHandle());
    }
    ~ReplayingHandle();

    // Add a command to this handle. This function must be called on the
    // creator thread. This function must be called BEFORE any reader is
    // obtained.
    void Add(const Command&);

    class Context final : public ThreadSafeRefCounted<Context> {
     public:
      static scoped_refptr<Context> Create() {
        return WTF::AdoptRef(new Context);
      }

      // This function cannot be called after creating a tee.
      void Add(const Command&);
      void AttachReader(WebDataConsumerHandle::Client*);
      void DetachReader();
      void DetachHandle();
      Result BeginRead(const void** buffer, Flags, size_t* available);
      Result EndRead(size_t read_size);
      WaitableEvent* Detached() { return detached_.get(); }

     private:
      Context();
      bool IsEmpty() const { return commands_.IsEmpty(); }
      const Command& Top();
      void Consume(size_t);
      size_t Offset() const { return offset_; }
      void Notify();
      void NotifyInternal();

      Deque<Command> commands_;
      size_t offset_;
      WebThread* reader_thread_;
      Client* client_;
      Result result_;
      bool is_handle_attached_;
      Mutex mutex_;
      std::unique_ptr<WaitableEvent> detached_;
    };

    Context* GetContext() { return context_.get(); }
    std::unique_ptr<Reader> ObtainReader(Client*) override;

   private:
    class ReaderImpl;

    ReplayingHandle();
    const char* DebugName() const override { return "ReplayingHandle"; }

    scoped_refptr<Context> context_;
  };

  static std::unique_ptr<WebDataConsumerHandle>
  CreateWaitingDataConsumerHandle();
};

}  // namespace blink

#endif  // DataConsumerHandleTestUtil_h
