// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_MOJO_HEAP_MOJO_ASSOCIATED_REMOTE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_MOJO_HEAP_MOJO_ASSOCIATED_REMOTE_H_

#include <utility>

#include "mojo/public/cpp/bindings/associated_remote.h"
#include "third_party/blink/renderer/platform/context_lifecycle_observer.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_wrapper_mode.h"

namespace blink {

// HeapMojoAssociatedRemote is a wrapper for mojo::AssociatedRemote to be owned
// by a garbage-collected object. Blink is expected to use
// HeapMojoAssociatedRemote by default. HeapMojoAssociatedRemote must be
// associated with context. HeapMojoAssociatedRemote's constructor takes context
// as a mandatory parameter. HeapMojoAssociatedRemote resets the mojo connection
// when 1) the owner object is garbage-collected and 2) the associated
// ExecutionContext is detached.

// TODO(crbug.com/1058076) HeapMojoWrapperMode should be removed once we ensure
// that the interface is not used after ContextDestroyed().
template <typename Interface,
          HeapMojoWrapperMode Mode = HeapMojoWrapperMode::kWithContextObserver>
class HeapMojoAssociatedRemote {
  DISALLOW_NEW();

 public:
  explicit HeapMojoAssociatedRemote(ContextLifecycleNotifier* notifier)
      : wrapper_(MakeGarbageCollected<Wrapper>(notifier)) {}
  HeapMojoAssociatedRemote(const HeapMojoAssociatedRemote&) = delete;
  HeapMojoAssociatedRemote& operator=(const HeapMojoAssociatedRemote&) = delete;
  HeapMojoAssociatedRemote(HeapMojoAssociatedRemote&&) = default;
  HeapMojoAssociatedRemote& operator=(HeapMojoAssociatedRemote&&) = default;

  // Methods to redirect to mojo::AssociatedRemote.
  using Proxy = typename Interface::Proxy_;
  Proxy* operator->() const { return get(); }
  Proxy* get() const { return wrapper_->associated_remote().get(); }
  bool is_bound() const { return wrapper_->associated_remote().is_bound(); }
  bool is_connected() const {
    return wrapper_->associated_remote().is_connected();
  }
  void reset() { wrapper_->associated_remote().reset(); }
  void ResetWithReason(uint32_t custom_reason, const std::string& description) {
    wrapper_->associated_remote().ResetWithReason(custom_reason, description);
  }
  void set_disconnect_handler(base::OnceClosure handler) {
    wrapper_->associated_remote().set_disconnect_handler(std::move(handler));
  }
  void set_disconnect_with_reason_handler(
      mojo::ConnectionErrorWithReasonCallback handler) {
    wrapper_->associated_remote().set_disconnect_with_reason_handler(
        std::move(handler));
  }
  mojo::PendingAssociatedReceiver<Interface> BindNewEndpointAndPassReceiver(
      scoped_refptr<base::SequencedTaskRunner> task_runner) WARN_UNUSED_RESULT {
    DCHECK(task_runner);
    return wrapper_->associated_remote().BindNewEndpointAndPassReceiver(
        std::move(task_runner));
  }
  mojo::PendingAssociatedReceiver<Interface>
  BindNewEndpointAndPassDedicatedReceiverForTesting() WARN_UNUSED_RESULT {
    return wrapper_->associated_remote()
        .BindNewEndpointAndPassDedicatedReceiverForTesting();
  }
  void Bind(mojo::PendingAssociatedRemote<Interface> pending_associated_remote,
            scoped_refptr<base::SequencedTaskRunner> task_runner) {
    DCHECK(task_runner);
    wrapper_->associated_remote().Bind(std::move(pending_associated_remote),
                                       std::move(task_runner));
  }
  void FlushForTesting() {
    return wrapper_->associated_remote().FlushForTesting();
  }

  void Trace(Visitor* visitor) { visitor->Trace(wrapper_); }

 private:
  // Garbage collected wrapper class to add a prefinalizer.
  class Wrapper final : public GarbageCollected<Wrapper>,
                        public ContextLifecycleObserver {
    USING_PRE_FINALIZER(Wrapper, Dispose);
    USING_GARBAGE_COLLECTED_MIXIN(Wrapper);

   public:
    explicit Wrapper(ContextLifecycleNotifier* notifier) {
      DCHECK(notifier);
      SetContextLifecycleNotifier(notifier);
    }
    Wrapper(const Wrapper&) = delete;
    Wrapper& operator=(const Wrapper&) = delete;
    Wrapper(Wrapper&&) = default;
    Wrapper& operator=(Wrapper&&) = default;

    void Trace(Visitor* visitor) override {
      ContextLifecycleObserver::Trace(visitor);
    }

    void Dispose() { associated_remote_.reset(); }

    mojo::AssociatedRemote<Interface>& associated_remote() {
      return associated_remote_;
    }

    // ContextLifecycleObserver methods
    void ContextDestroyed() override {
      if (Mode == HeapMojoWrapperMode::kWithContextObserver)
        associated_remote_.reset();
    }

   private:
    mojo::AssociatedRemote<Interface> associated_remote_;
  };

  Member<Wrapper> wrapper_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_MOJO_HEAP_MOJO_ASSOCIATED_REMOTE_H_
