// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/mojo/heap_mojo_unique_receiver_set.h"
#include "base/test/null_task_runner.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/interfaces/bindings/tests/sample_service.mojom-blink.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/context_lifecycle_notifier.h"
#include "third_party/blink/renderer/platform/heap/heap_test_utilities.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/heap_observer_list.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_wrapper_mode.h"

namespace blink {

namespace {

class FakeContextNotifier final : public GarbageCollected<FakeContextNotifier>,
                                  public ContextLifecycleNotifier {
  USING_GARBAGE_COLLECTED_MIXIN(FakeContextNotifier);

 public:
  FakeContextNotifier() = default;

  void AddContextLifecycleObserver(
      ContextLifecycleObserver* observer) override {
    observers_.AddObserver(observer);
  }
  void RemoveContextLifecycleObserver(
      ContextLifecycleObserver* observer) override {
    observers_.RemoveObserver(observer);
  }

  void NotifyContextDestroyed() {
    observers_.ForEachObserver([](ContextLifecycleObserver* observer) {
      observer->ContextDestroyed();
    });
  }

  void Trace(Visitor* visitor) override {
    visitor->Trace(observers_);
    ContextLifecycleNotifier::Trace(visitor);
  }

 private:
  HeapObserverList<ContextLifecycleObserver> observers_;
};

template <HeapMojoWrapperMode Mode>
class GCOwner : public GarbageCollected<GCOwner<Mode>> {
 public:
  explicit GCOwner(FakeContextNotifier* context) : receiver_set_(context) {}
  void Trace(Visitor* visitor) { visitor->Trace(receiver_set_); }

  HeapMojoUniqueReceiverSet<sample::blink::Service,
                            std::default_delete<sample::blink::Service>,
                            Mode>&
  receiver_set() {
    return receiver_set_;
  }

 private:
  HeapMojoUniqueReceiverSet<sample::blink::Service,
                            std::default_delete<sample::blink::Service>,
                            Mode>
      receiver_set_;
};

template <HeapMojoWrapperMode Mode>
class HeapMojoUniqueReceiverSetBaseTest : public TestSupportingGC {
 public:
  FakeContextNotifier* context() { return context_; }
  scoped_refptr<base::NullTaskRunner> task_runner() {
    return null_task_runner_;
  }
  GCOwner<Mode>* owner() { return owner_; }

  void ClearOwner() { owner_ = nullptr; }

  void MarkServiceDeleted() { service_deleted_ = true; }

 protected:
  void SetUp() override {
    context_ = MakeGarbageCollected<FakeContextNotifier>();
    owner_ = MakeGarbageCollected<GCOwner<Mode>>(context());
  }
  void TearDown() override {}

  Persistent<FakeContextNotifier> context_;
  Persistent<GCOwner<Mode>> owner_;
  scoped_refptr<base::NullTaskRunner> null_task_runner_ =
      base::MakeRefCounted<base::NullTaskRunner>();
  bool service_deleted_ = false;
};

class HeapMojoUniqueReceiverSetWithContextObserverTest
    : public HeapMojoUniqueReceiverSetBaseTest<
          HeapMojoWrapperMode::kWithContextObserver> {};
class HeapMojoUniqueReceiverSetWithoutContextObserverTest
    : public HeapMojoUniqueReceiverSetBaseTest<
          HeapMojoWrapperMode::kWithoutContextObserver> {};

}  // namespace

namespace {

template <typename T>
class MockService : public sample::blink::Service {
 public:
  explicit MockService(T* test) : test_(test) {}
  // Notify the test when the service is deleted by the UniqueReceiverSet.
  ~MockService() override { test_->MarkServiceDeleted(); }

  void Frobinate(sample::blink::FooPtr foo,
                 Service::BazOptions baz,
                 mojo::PendingRemote<sample::blink::Port> port,
                 FrobinateCallback callback) override {}
  void GetPort(mojo::PendingReceiver<sample::blink::Port> receiver) override {}

 private:
  T* test_;
};

}  // namespace

// GC the HeapMojoUniqueReceiverSet with context observer and verify that the
// receiver is no longer part of the set, and that the service was deleted.
TEST_F(HeapMojoUniqueReceiverSetWithContextObserverTest, ResetsOnGC) {
  auto& receiver_set = owner()->receiver_set();
  auto service = std::make_unique<
      MockService<HeapMojoUniqueReceiverSetWithContextObserverTest>>(this);
  auto receiver = mojo::PendingReceiver<sample::blink::Service>(
      mojo::MessagePipe().handle0);

  mojo::ReceiverId rid =
      receiver_set.Add(std::move(service), std::move(receiver), task_runner());
  EXPECT_TRUE(receiver_set.HasReceiver(rid));
  EXPECT_FALSE(service_deleted_);

  ClearOwner();
  PreciselyCollectGarbage();

  EXPECT_TRUE(service_deleted_);

  CompleteSweepingIfNeeded();
}

// GC the HeapMojoUniqueReceiverSet without context observer and verify that the
// receiver is no longer part of the set, and that the service was deleted.
TEST_F(HeapMojoUniqueReceiverSetWithoutContextObserverTest, ResetsOnGC) {
  auto& receiver_set = owner()->receiver_set();
  auto service = std::make_unique<
      MockService<HeapMojoUniqueReceiverSetWithoutContextObserverTest>>(this);
  auto receiver = mojo::PendingReceiver<sample::blink::Service>(
      mojo::MessagePipe().handle0);

  mojo::ReceiverId rid =
      receiver_set.Add(std::move(service), std::move(receiver), task_runner());
  EXPECT_TRUE(receiver_set.HasReceiver(rid));
  EXPECT_FALSE(service_deleted_);

  ClearOwner();
  PreciselyCollectGarbage();

  EXPECT_TRUE(service_deleted_);

  CompleteSweepingIfNeeded();
}

// Destroy the context with context observer and verify that the receiver is no
// longer part of the set, and that the service was deleted.
TEST_F(HeapMojoUniqueReceiverSetWithContextObserverTest,
       ResetsOnContextDestroyed) {
  HeapMojoUniqueReceiverSet<sample::blink::Service> receiver_set(context());
  auto service = std::make_unique<
      MockService<HeapMojoUniqueReceiverSetWithContextObserverTest>>(this);
  auto receiver = mojo::PendingReceiver<sample::blink::Service>(
      mojo::MessagePipe().handle0);

  mojo::ReceiverId rid =
      receiver_set.Add(std::move(service), std::move(receiver), task_runner());
  EXPECT_TRUE(receiver_set.HasReceiver(rid));
  EXPECT_FALSE(service_deleted_);

  context_->NotifyContextDestroyed();

  EXPECT_FALSE(receiver_set.HasReceiver(rid));
  EXPECT_TRUE(service_deleted_);
}

// Destroy the context without context observer and verify that the receiver is
// no longer part of the set, and that the service was deleted.
TEST_F(HeapMojoUniqueReceiverSetWithoutContextObserverTest,
       ResetsOnContextDestroyed) {
  HeapMojoUniqueReceiverSet<sample::blink::Service> receiver_set(context());
  auto service = std::make_unique<
      MockService<HeapMojoUniqueReceiverSetWithoutContextObserverTest>>(this);
  auto receiver = mojo::PendingReceiver<sample::blink::Service>(
      mojo::MessagePipe().handle0);

  mojo::ReceiverId rid =
      receiver_set.Add(std::move(service), std::move(receiver), task_runner());
  EXPECT_TRUE(receiver_set.HasReceiver(rid));
  EXPECT_FALSE(service_deleted_);

  context_->NotifyContextDestroyed();

  EXPECT_FALSE(receiver_set.HasReceiver(rid));
  EXPECT_TRUE(service_deleted_);
}

}  // namespace blink
