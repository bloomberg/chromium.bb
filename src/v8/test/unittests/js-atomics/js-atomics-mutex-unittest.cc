// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/base/platform/platform.h"
#include "src/base/platform/semaphore.h"
#include "src/base/platform/time.h"
#include "src/objects/js-atomics-synchronization-inl.h"
#include "test/unittests/test-utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace v8 {
namespace internal {

using JSAtomicsMutexTest = TestWithSharedIsolate;

namespace {

class ClientIsolateWithContextWrapper final {
 public:
  explicit ClientIsolateWithContextWrapper(v8::Isolate* shared_isolate)
      : client_isolate_wrapper_(kNoCounters, kClientIsolate, shared_isolate),
        isolate_scope_(client_isolate_wrapper_.isolate()),
        handle_scope_(client_isolate_wrapper_.isolate()),
        context_(v8::Context::New(client_isolate_wrapper_.isolate())),
        context_scope_(context_) {}

  v8::Isolate* v8_isolate() const { return client_isolate_wrapper_.isolate(); }
  Isolate* isolate() const { return reinterpret_cast<Isolate*>(v8_isolate()); }

 private:
  IsolateWrapper client_isolate_wrapper_;
  v8::Isolate::Scope isolate_scope_;
  v8::HandleScope handle_scope_;
  v8::Local<v8::Context> context_;
  v8::Context::Scope context_scope_;
};

class LockingThread final : public v8::base::Thread {
 public:
  LockingThread(v8::Isolate* shared_isolate, Handle<JSAtomicsMutex> mutex,
                base::Semaphore* sema_ready,
                base::Semaphore* sema_execute_start,
                base::Semaphore* sema_execute_complete)
      : Thread(Options("ThreadWithAtomicsMutex")),
        shared_isolate_(shared_isolate),
        mutex_(mutex),
        sema_ready_(sema_ready),
        sema_execute_start_(sema_execute_start),
        sema_execute_complete_(sema_execute_complete) {}

  void Run() override {
    ClientIsolateWithContextWrapper client_isolate_wrapper(shared_isolate_);
    Isolate* isolate = client_isolate_wrapper.isolate();

    sema_ready_->Signal();
    sema_execute_start_->Wait();

    HandleScope scope(isolate);
    JSAtomicsMutex::Lock(isolate, mutex_);
    EXPECT_TRUE(mutex_->IsHeld());
    EXPECT_TRUE(mutex_->IsCurrentThreadOwner());
    base::OS::Sleep(base::TimeDelta::FromMilliseconds(1));
    mutex_->Unlock(isolate);

    sema_execute_complete_->Signal();
  }

 protected:
  v8::Isolate* shared_isolate_;
  Handle<JSAtomicsMutex> mutex_;
  base::Semaphore* sema_ready_;
  base::Semaphore* sema_execute_start_;
  base::Semaphore* sema_execute_complete_;
};

}  // namespace

TEST_F(JSAtomicsMutexTest, Contention) {
  if (!IsJSSharedMemorySupported()) return;

  FLAG_harmony_struct = true;

  v8::Isolate* shared_isolate = v8_isolate();
  ClientIsolateWithContextWrapper client_isolate_wrapper(shared_isolate);

  constexpr int kThreads = 32;

  Handle<JSAtomicsMutex> contended_mutex =
      JSAtomicsMutex::Create(client_isolate_wrapper.isolate());
  base::Semaphore sema_ready(0);
  base::Semaphore sema_execute_start(0);
  base::Semaphore sema_execute_complete(0);
  std::vector<std::unique_ptr<LockingThread>> threads;
  for (int i = 0; i < kThreads; i++) {
    auto thread = std::make_unique<LockingThread>(
        shared_isolate, contended_mutex, &sema_ready, &sema_execute_start,
        &sema_execute_complete);
    CHECK(thread->Start());
    threads.push_back(std::move(thread));
  }

  for (int i = 0; i < kThreads; i++) sema_ready.Wait();
  for (int i = 0; i < kThreads; i++) sema_execute_start.Signal();
  for (int i = 0; i < kThreads; i++) sema_execute_complete.Wait();

  for (auto& thread : threads) {
    thread->Join();
  }

  EXPECT_FALSE(contended_mutex->IsHeld());
}

}  // namespace internal
}  // namespace v8
