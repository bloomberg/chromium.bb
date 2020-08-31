// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_MOJO_HEAP_MOJO_RECEIVER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_MOJO_HEAP_MOJO_RECEIVER_H_

#include "mojo/public/cpp/bindings/receiver.h"
#include "third_party/blink/renderer/platform/context_lifecycle_observer.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_wrapper_mode.h"

namespace blink {

// HeapMojoReceiver is a wrapper for mojo::Receiver to be owned by a
// garbage-collected object. Blink is expected to use HeapMojoReceiver by
// default. HeapMojoReceiver must be associated with context.
// HeapMojoReceiver's constructor takes context as a mandatory parameter.
// HeapMojoReceiver resets the mojo connection when 1) the owner object is
// garbage-collected and 2) the associated ExecutionContext is detached.

// TODO(crbug.com/1058076) HeapMojoWrapperMode should be removed once we ensure
// that the interface is not used after ContextDestroyed().
template <typename Interface,
          typename Owner,
          HeapMojoWrapperMode Mode = HeapMojoWrapperMode::kWithContextObserver>
class HeapMojoReceiver {
  DISALLOW_NEW();

 public:
  HeapMojoReceiver(Owner* owner, ContextLifecycleNotifier* context)
      : wrapper_(MakeGarbageCollected<Wrapper>(owner, context)) {
    static_assert(std::is_base_of<Interface, Owner>::value,
                  "Owner should implement Interface");
    static_assert(IsGarbageCollectedType<Owner>::value,
                  "Owner needs to be a garbage collected object");
  }
  HeapMojoReceiver(const HeapMojoReceiver&) = delete;
  HeapMojoReceiver& operator=(const HeapMojoReceiver&) = delete;

  // Methods to redirect to mojo::Receiver:
  bool is_bound() const { return wrapper_->receiver().is_bound(); }
  void reset() { wrapper_->receiver().reset(); }
  void ResetWithReason(uint32_t custom_reason_code,
                       const std::string& description) {
    wrapper_->receiver().ResetWithReason(custom_reason_code, description);
  }
  void set_disconnect_handler(base::OnceClosure handler) {
    wrapper_->receiver().set_disconnect_handler(std::move(handler));
  }
  void set_disconnect_with_reason_handler(
      mojo::ConnectionErrorWithReasonCallback error_handler) {
    wrapper_->receiver().set_disconnect_with_reason_handler(
        std::move(error_handler));
  }
  mojo::PendingRemote<Interface> BindNewPipeAndPassRemote(
      scoped_refptr<base::SequencedTaskRunner> task_runner) WARN_UNUSED_RESULT {
    DCHECK(task_runner);
    return wrapper_->receiver().BindNewPipeAndPassRemote(
        std::move(task_runner));
  }
  void Bind(mojo::PendingReceiver<Interface> pending_receiver,
            scoped_refptr<base::SequencedTaskRunner> task_runner) {
    DCHECK(task_runner);
    wrapper_->receiver().Bind(std::move(pending_receiver),
                              std::move(task_runner));
  }
  bool WaitForIncomingCall() {
    return wrapper_->receiver().WaitForIncomingCall();
  }

  void Trace(Visitor* visitor) { visitor->Trace(wrapper_); }

 private:
  FRIEND_TEST_ALL_PREFIXES(HeapMojoReceiverGCWithContextObserverTest,
                           NoResetOnConservativeGC);

  // Garbage collected wrapper class to add a prefinalizer.
  class Wrapper final : public GarbageCollected<Wrapper>,
                        public ContextLifecycleObserver {
    USING_PRE_FINALIZER(Wrapper, Dispose);
    USING_GARBAGE_COLLECTED_MIXIN(Wrapper);

   public:
    Wrapper(Owner* owner, ContextLifecycleNotifier* notifier)
        : owner_(owner), receiver_(owner) {
      SetContextLifecycleNotifier(notifier);
    }

    void Trace(Visitor* visitor) override {
      visitor->Trace(owner_);
      ContextLifecycleObserver::Trace(visitor);
    }

    void Dispose() { receiver_.reset(); }

    mojo::Receiver<Interface>& receiver() { return receiver_; }

    // ContextLifecycleObserver methods
    void ContextDestroyed() override {
      if (Mode == HeapMojoWrapperMode::kWithContextObserver)
        receiver_.reset();
    }

   private:
    Member<Owner> owner_;
    mojo::Receiver<Interface> receiver_;
  };

  Member<Wrapper> wrapper_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_MOJO_HEAP_MOJO_RECEIVER_H_
