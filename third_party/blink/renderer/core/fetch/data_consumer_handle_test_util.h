// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FETCH_DATA_CONSUMER_HANDLE_TEST_UTIL_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FETCH_DATA_CONSUMER_HANDLE_TEST_UTIL_H_

#include <memory>

#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "gin/public/isolate_holder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/public/platform/web_data_consumer_handle.h"
#include "third_party/blink/renderer/core/testing/null_execution_context.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/waitable_event.h"
#include "third_party/blink/renderer/platform/web_thread_supporting_gc.h"
#include "third_party/blink/renderer/platform/wtf/deque.h"
#include "third_party/blink/renderer/platform/wtf/locker.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/thread_safe_ref_counted.h"
#include "third_party/blink/renderer/platform/wtf/threading_primitives.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "v8/include/v8.h"

namespace blink {

class DataConsumerHandleTestUtil {
  STATIC_ONLY(DataConsumerHandleTestUtil);

 public:
  class Command final {
    DISALLOW_NEW();

   public:
    enum Name {
      kData,
      kDone,
      kError,
      kWait,
    };

    Command(Name name) : name_(name) {}
    Command(Name name, const Vector<char>& body) : name_(name), body_(body) {}
    Command(Name name, const char* body, wtf_size_t size) : name_(name) {
      body_.Append(body, size);
    }
    Command(Name name, const char* body)
        : Command(name, body, static_cast<wtf_size_t>(strlen(body))) {}
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
      return base::WrapUnique(new ReplayingHandle());
    }
    ~ReplayingHandle() override;

    // Add a command to this handle. This function must be called on the
    // creator thread. This function must be called BEFORE any reader is
    // obtained.
    void Add(const Command&);

    class Context final : public ThreadSafeRefCounted<Context> {
     public:
      static scoped_refptr<Context> Create() {
        return base::AdoptRef(new Context);
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
      blink::Thread* reader_thread_;
      Client* client_;
      Result result_;
      bool is_handle_attached_;
      Mutex mutex_;
      std::unique_ptr<WaitableEvent> detached_;
    };

    Context* GetContext() { return context_.get(); }
    std::unique_ptr<Reader> ObtainReader(
        Client*,
        scoped_refptr<base::SingleThreadTaskRunner>) override;

   private:
    class ReaderImpl;

    ReplayingHandle();
    const char* DebugName() const override { return "ReplayingHandle"; }

    scoped_refptr<Context> context_;
  };
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_FETCH_DATA_CONSUMER_HANDLE_TEST_UTIL_H_
