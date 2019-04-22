// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_RECEIVER_H_
#define MOJO_PUBLIC_CPP_BINDINGS_RECEIVER_H_

#include <memory>
#include <utility>

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequenced_task_runner.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/public/cpp/bindings/lib/binding_state.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/raw_ptr_impl_ref_traits.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/system/message_pipe.h"

namespace mojo {

// A Receiver is used to receive and dispatch Interface method calls to a local
// implementation of Interface. Every Receiver object is permanently linked to
// an implementation of Interface at construction time. The Receiver begins
// receiving and scheduling method calls to the implementation once it becomes
// bound either by consuming a PendingReceiver (at construction time or via
// |Bind()|) or by calling |BindNewPipeAndPassRemote()|.
//
// Receiver is NOT thread- or sequence- safe and must be used from a single
// (but otherwise arbitrary) sequence. All bound Receiver objects are associated
// with a base::SequencedTaskRunner which the Receiver uses exclusively to
// schedule incoming method calls and disconnection notifications.
//
// IMPORTANT: In the name of memory safety, Interface method calls and
// disconnection notifications scheduled by a Receiver object will NEVER run
// beyond the lifetime of the Receiver object.
template <typename Interface,
          typename ImplRefTraits = RawPtrImplRefTraits<Interface>>
class Receiver {
 public:
  // Typically (and by default) a Receiver uses a raw pointer to reference its
  // linked Interface implementation object, because typically that
  // implementation object owns the Receiver. An alternative |ImplRefTraits| may
  // be provided as a second Receiver template argument in order to use a
  // different reference type.
  using ImplPointerType = typename ImplRefTraits::PointerType;

  // Constructs an unbound Receiver linked to |impl| for the duration of the
  // Receive's lifetime. The Receiver can be bound later by calling |Bind()| or
  // |BindNewPipeAndPassRemote()|. An unbound Receiver does not schedule any
  // asynchronous tasks.
  explicit Receiver(ImplPointerType impl) : internal_state_(std::move(impl)) {}

  // Constructs a bound Receiver by consuming |pending_receiver|. The Receiver
  // is permanently linked to |impl| and will schedule incoming |impl| method
  // and disconnection notifications on the default SequencedTaskRunner (i.e.
  // base::SequencedTaskRunnerHandle::Get() at construction time).
  Receiver(ImplPointerType impl, PendingReceiver<Interface> pending_receiver)
      : Receiver(std::move(impl), std::move(pending_receiver), nullptr) {}

  // Similar to above but the constructed Receiver schedules all tasks via
  // |task_runner| instead of the default SequencedTaskRunner. |task_runner|
  // must run tasks on the same sequence that owns this Receiver.
  Receiver(ImplPointerType impl,
           PendingReceiver<Interface> pending_receiver,
           scoped_refptr<base::SequencedTaskRunner> task_runner)
      : internal_state_(std::move(impl)) {
    Bind(std::move(pending_receiver), std::move(task_runner));
  }

  ~Receiver() = default;

  // Indicates whether this Receiver is bound, meaning it may continue to
  // receive Interface method calls from a remote caller.
  //
  // NOTE: A Receiver is NEVER passively unbound. The only way for it to become
  // unbound is to explicitly call |reset()| or |Unbind()|.
  bool is_bound() const { return internal_state_.is_bound(); }

  // Sets a OnceClosure to be invoked if this Receiver is cut off from its
  // Remote (or PendingRemote). This can happen if the corresponding Remote (or
  // unconsumed PendingRemote) has been destroyed, or if the Remote sends a
  // malformed message. Must only be called on a bound Receiver object, and only
  // remains set as long as the Receiver is both bound and connected.
  //
  // If ever invoked, |handler| will be scheduled asynchronously on the
  // Receiver's bound SequencedTaskRunner.
  void set_disconnect_handler(base::OnceClosure handler) {
    internal_state_.set_connection_error_handler(std::move(handler));
  }

  // Resets this Receiver to an unbound state. An unbound Receiver will NEVER
  // schedule method calls or disconnection notifications, and any pending tasks
  // which were scheduled prior to unbinding are effectively cancelled.
  void reset() { internal_state_.Close(); }

  // Binds this Receiver, connecting it to a new PendingRemote which is
  // returned for transmission elsewhere (typically to a Remote who will consume
  // it to start making calls).
  //
  // The Receiver will schedule incoming |impl| method calls and disconnection
  // notifications on the default SequencedTaskRunner (i.e.
  // base::SequencedTaskRunnerHandle::Get() at the time of this call). Must only
  // be called on an unbound Receiver.
  PendingRemote<Interface> BindNewPipeAndPassRemote() WARN_UNUSED_RESULT {
    return BindNewPipeAndPassRemote(nullptr);
  }

  // Like above, but the Receiver will schedule incoming |impl| method calls and
  // disconnection notifications on |task_runner| rather than on the default
  // SequencedTaskRunner. Must only be called on an unbound Receiver.
  // |task_runner| must run tasks on the same sequence that owns this Receiver.
  PendingRemote<Interface> BindNewPipeAndPassRemote(
      scoped_refptr<base::SequencedTaskRunner> task_runner) WARN_UNUSED_RESULT {
    DCHECK(!is_bound()) << "Receiver is already bound";
    PendingRemote<Interface> remote;
    Bind(remote.InitWithNewPipeAndPassReceiver(), std::move(task_runner));
    return remote;
  }

  // Binds this Receiver by consuming |pending_receiver|, which must be valid.
  // Must only be called on an unbound Receiver.
  //
  // The newly bound Receiver will schedule incoming |impl| method calls and
  // disconnection notifications on the default SequencedTaskRunner (i.e.
  // base::SequencedTaskRunnerHandle::Get() at the time of this call).
  void Bind(PendingReceiver<Interface> pending_receiver) {
    DCHECK(pending_receiver.is_valid());
    Bind(std::move(pending_receiver), nullptr);
  }

  // Like above, but the newly bound Receiver will schedule incoming |impl|
  // method calls and disconnection notifications on |task_runner| instead of
  // the default SequencedTaskRunner. Must only be called on an unbound
  // Receiver. |task_runner| must run tasks on the same sequence that owns this
  // Receiver.
  void Bind(PendingReceiver<Interface> pending_receiver,
            scoped_refptr<base::SequencedTaskRunner> task_runner) {
    internal_state_.Bind(pending_receiver.PassPipe(), std::move(task_runner));
  }

  // Unbinds this Receiver, preventing any further |impl| method calls or
  // disconnection notifications from being scheduled by it. Any such tasks that
  // were scheduled prior to unbinding are effectively cancelled.
  //
  // Returns a PendingReceiver which remains connected to this receiver's
  // Remote and which may be transferred elsewhere and consumed by another
  // Receiver. Any messages received but not actually dispatched by this
  // Receiver remain intact within the returned PendingReceiver and can be
  // dispatched by whomever binds with it later.
  //
  // Note that a Receiver should not be unbound while there are still living
  // response callbacks that haven't been invoked, as once the Receiver is
  // unbound those response callbacks are no longer valid and the Remote will
  // never be able to receive its expected responses.
  PendingReceiver<Interface> Unbind() {
    CHECK(!internal_state_.HasAssociatedInterfaces());
    return PendingReceiver<Interface>(
        internal_state_.Unbind().PassMessagePipe());
  }

  // Adds a message filter to be notified of each incoming message before
  // dispatch. If a filter returns |false| from Accept(), the message is not
  // dispatched and the pipe is closed. Filters cannot be removed once added.
  void AddFilter(std::unique_ptr<MessageReceiver> filter) {
    DCHECK(is_bound());
    internal_state_.AddFilter(std::move(filter));
  }

 private:
  internal::BindingState<Interface, ImplRefTraits> internal_state_;

  DISALLOW_COPY_AND_ASSIGN(Receiver);
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_RECEIVER_H_
