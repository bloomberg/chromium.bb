// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_REMOTE_H_
#define MOJO_PUBLIC_CPP_BINDINGS_REMOTE_H_

#include <cstdint>
#include <utility>

#include "base/callback_forward.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequenced_task_runner.h"
#include "mojo/public/cpp/bindings/interface_ptr_info.h"
#include "mojo/public/cpp/bindings/lib/interface_ptr_state.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/system/message_pipe.h"

namespace mojo {

// A Remote is used to issue Interface method calls to a single connected
// Receiver or PendingReceiver. The Remote must be bound in order to issue those
// method calls, and it becomes bound by consuming a PendingRemote either at
// construction time or by calling |Bind()|.
//
// Remote is NOT thread- or sequence-safe and must be used on a single
// (but otherwise arbitrary) sequence. All bound Remote objects are associated
// with a base::SequenceTaskRunner which the Remote uses exclusively to schedule
// response callbacks and disconnection notifications.
//
// The most common ways to bind a Remote are to consume to a PendingRemote
// received via some IPC, or to call |BindNewPipeAndPassReceiver()| and send the
// returned PendingReceiver somewhere useful (i.e., to a remote Receiver who
// will consume it). For example:
//
//     mojo::Remote<mojom::Widget> widget;
//     widget_factory->CreateWidget(widget.BindNewPipeAndPassReceiver());
//     widget->Click();
//
// IMPORTANT: There are some things to be aware of regarding Interface method
// calls as they relate to Remote object lifetime:
//
//   - Interface method calls issued immediately before the destruction of a
//     Remote ARE guaranteed to be transmitted to the remote's receiver as long
//     as the receiver itself remains alive, either as a Receiver or a
//     PendingReceiver.
//
//   - In the name of memory safety, Interface method response callbacks (and in
//     general ANY tasks which can be scheduled by a Remote) will NEVER
//     be dispatched beyond the lifetime of the Remote object. As such, if
//     you make a call and you need its reply, you must keep the Remote alive
//     until the reply is received.
template <typename Interface>
class Remote {
 public:
  // Constructs an unbound Remote. This object cannot issue Interface method
  // calls and does not schedule any tasks.
  Remote() = default;
  Remote(Remote&& other) noexcept {
    internal_state_.Swap(&other.internal_state_);
  }

  // Constructs a new Remote which is bound from |pending_remote| and which
  // schedules response callbacks and disconnection notifications on the default
  // SequencedTaskRunner (i.e., base::SequencedTaskRunnerHandle::Get() at
  // construction time).
  explicit Remote(PendingRemote<Interface> pending_remote)
      : Remote(std::move(pending_remote), nullptr) {}

  // Constructs a new Remote which is bound from |pending_remote| and which
  // schedules response callbacks and disconnection notifications on
  // |task_runner|. |task_runner| must run tasks on the same sequence that owns
  // this Remote.
  Remote(PendingRemote<Interface> pending_remote,
         scoped_refptr<base::SequencedTaskRunner> task_runner) {
    DCHECK(pending_remote.is_valid());
    Bind(std::move(pending_remote), std::move(task_runner));
  }

  ~Remote() = default;

  // Exposes access to callable Interface methods directed at this Remote's
  // receiver. Must only be called on a bound Remote.
  typename Interface::Proxy_* get() const {
    DCHECK(is_bound())
        << "Cannot issue Interface method calls on an unbound Remote";
    return internal_state_.instance();
  }

  // Shorthand form of |get()|. See above.
  typename Interface::Proxy_* operator->() const { return get(); }

  // Indicates whether this Remote is bound and thus can issue Interface method
  // calls via the above accessors.
  //
  // NOTE: The state of being "bound" should not be confused with the state of
  // being "connected" (see |is_connected()| below). A Remote is NEVER passively
  // unbound and the only way for it to become unbound is to explicitly call
  // |reset()| or |Unbind()|. As such, unless you make explicit calls to those
  // methods, it is always safe to assume that a Remote you've bound will remain
  // bound and callable.
  bool is_bound() const { return internal_state_.is_bound(); }

  // Indicates whether this Remote is connected to a receiver. Must only be
  // called on a bound Remote. If this returns |true|, method calls made by this
  // Remote may eventually end up at the connected receiver (though it's of
  // course possible for this call to race with disconnection). If this returns
  // |false| however, all future Interface method calls on this Remote will be
  // silently dropped.
  //
  // A bound Remote becomes disconnected automatically either when its receiver
  // is destroyed, or when it receives a malformed or otherwise unexpected
  // response message from the receiver.
  //
  // NOTE: The state of being "bound" should not be confused with the state of
  // being "connected". See |is_bound()| above.
  bool is_connected() const {
    DCHECK(is_bound());
    return !internal_state_.encountered_error();
  }

  // Sets a Closure to be invoked if this Remote is cut off from its receiver.
  // This can happen if the corresponding Receiver (or unconsumed
  // PendingReceiver) is destroyed, or if the Receiver sends a malformed or
  // otherwise unexpected response message to this Remote. Must only be called
  // on a bound Remote object, and only remains set as long as the Remote is
  // both bound and connected.
  //
  // If invoked at all, |handler| will be scheduled asynchronously using the
  // Remote's bound SequencedTaskRunner.
  void set_disconnect_handler(base::OnceClosure handler) {
    if (is_connected())
      internal_state_.set_connection_error_handler(std::move(handler));
  }

  // Resets this Remote to an unbound state. To reset the Remote and recover an
  // PendingRemote that can be bound again later, use |Unbind()| instead.
  void reset() {
    State doomed_state;
    internal_state_.Swap(&doomed_state);
  }

  // Binds this Remote, connecting it to a new PendingReceiver which is
  // returned for transmission to some Receiver which can bind it. The Remote
  // will schedule any response callbacks or disconnection notifications on the
  // default SequencedTaskRunner (i.e. base::SequencedTaskRunnerHandle::Get() at
  // the time of this call). Must only be called on an unbound Remote.
  PendingReceiver<Interface> BindNewPipeAndPassReceiver() WARN_UNUSED_RESULT {
    return BindNewPipeAndPassReceiver(nullptr);
  }

  // Like above, but the Remote will schedule response callbacks and
  // disconnection notifications on |task_runner| instead of the default
  // SequencedTaskRunner. |task_runner| must run tasks on the same sequence that
  // owns this Remote.
  PendingReceiver<Interface> BindNewPipeAndPassReceiver(
      scoped_refptr<base::SequencedTaskRunner> task_runner) WARN_UNUSED_RESULT {
    MessagePipe pipe;
    Bind(PendingRemote<Interface>(std::move(pipe.handle0), 0),
         std::move(task_runner));
    return PendingReceiver<Interface>(std::move(pipe.handle1));
  }

  // Binds this Remote by consuming |pending_remote|, which must be valid. The
  // Remote will schedule any response callbacks or disconnection notifications
  // on the default SequencedTaskRunner (i.e.
  // base::SequencedTaskRunnerHandle::Get() at the time of this call). Must only
  // be called on an unbound Remote.
  void Bind(PendingRemote<Interface> pending_remote) {
    DCHECK(pending_remote.is_valid());
    Bind(std::move(pending_remote), nullptr);
  }

  // Like above, but the Remote will schedule response callbacks and
  // disconnection notifications on |task_runner| instead of the default
  // SequencedTaskRunner. Must only be called on an unbound Remote.
  // |task_runner| must run tasks on the same sequence that owns this Remote.
  void Bind(PendingRemote<Interface> pending_remote,
            scoped_refptr<base::SequencedTaskRunner> task_runner) {
    DCHECK(!is_bound()) << "Remote is already bound";
    internal_state_.Bind(InterfacePtrInfo<Interface>(pending_remote.PassPipe(),
                                                     pending_remote.version()),
                         std::move(task_runner));

    // Force the internal state to configure its proxy. Unlike InterfacePtr we
    // do not use Remote in transit, so binding to a pipe handle can also imply
    // binding to a SequencedTaskRunner and observing pipe handle state. This
    // allows for e.g. |is_connected()| to be a more reliable API than
    // |InterfacePtr::encountered_error()|.
    ignore_result(internal_state_.instance());
  }

  // Unbinds this Remote, rendering it unable to issue further Interface method
  // calls. Returns a PendingRemote which may be passed across threads or
  // processes and consumed by another Remote elsewhere.
  //
  // Note that it is an error (the bad, crashy kind of error) to attempt to
  // |Unbind()| a Remote which is awaiting one or more responses to previously
  // issued Interface method calls. Calling this method should only be
  // considered in cases where satisfaction of that constraint can be proven.
  //
  // Must only be called on a bound Remote.
  PendingRemote<Interface> Unbind() WARN_UNUSED_RESULT {
    DCHECK(is_bound());
    CHECK(!internal_state_.has_unbound_callbacks());
    State state;
    internal_state_.Swap(&state);
    InterfacePtrInfo<Interface> info = state.PassInterface();
    return PendingRemote<Interface>(info.PassHandle(), info.version());
  }

 private:
  using State = internal::InterfacePtrState<Interface>;
  mutable State internal_state_;

  DISALLOW_COPY_AND_ASSIGN(Remote);
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_REMOTE_H_
