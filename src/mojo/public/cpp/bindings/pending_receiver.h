// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_PENDING_RECEIVER_H_
#define MOJO_PUBLIC_CPP_BINDINGS_PENDING_RECEIVER_H_

#include <utility>

#include "base/macros.h"
#include "mojo/public/cpp/system/message_pipe.h"

namespace mojo {

// A PendingReceiver receives and accumulates a queue of incoming Interface
// method calls made by a single corresponding Remote. PendingReceiver instances
// may be freely moved to another thread/sequence, or even transferred to
// another process via a Mojo interface call (see pending_receiver<T> syntax in
// mojom IDL).
//
// This object should eventually be consumed to bind a Receiver, which will then
// begin dispatching any queued and future incoming method calls to a local
// implementation of Interface. See Receiver documentation for more details.
//
// NOTE: This object is essentially semantic sugar wrapping a message pipe
// handle that is expected to receive Interface messages from a Remote. As such,
// consumers who know what they're doing (i.e. who are confident about what lies
// on the other end of a pipe) may freely convert between a PendingReceiver and
// a raw message pipe handle.
template <typename Interface>
class PendingReceiver {
 public:
  // Constructs an invalid PendingReceiver. This object is not entangled with
  // any Remote and cannot be used to bind a Receiver.
  //
  // A valid PendingReceiver is commonly obtained by calling
  // |Remote::BindNewReceiver()| on an existing unbound Remote instance or less
  // commonly by calling calling |PendingRemote::MakeReceiver()| on an existing
  // but invalid PendingRemote instance.
  PendingReceiver() = default;
  PendingReceiver(PendingReceiver&&) noexcept = default;

  // Constructs a valid PendingReceiver from a valid raw message pipe handle.
  explicit PendingReceiver(ScopedMessagePipeHandle pipe)
      : pipe_(std::move(pipe)) {
    DCHECK(pipe_.is_valid());
  }

  ~PendingReceiver() = default;

  PendingReceiver& operator=(PendingReceiver&&) noexcept = default;

  // Indicates whether the PendingReceiver is valid, meaning it can ne used to
  // bind a Receiver that wants to begin dispatching method calls made by the
  // entangled Remote.
  bool is_valid() const { return pipe_.is_valid(); }

  // Resets this PendingReceiver to an invalid state. If it was entangled with a
  // Remote or PendingRemote, that object remains in a valid state and will
  // eventually detect that its receiver is gone. Any calls it makes will
  // effectively be dropped.
  void reset() { pipe_.reset(); }

  // Takes ownership of this PendingReceiver's message pipe handle. After this
  // call, the PendingReceiver is no longer in a valid state and can no longer
  // be used to bind a Receiver.
  ScopedMessagePipeHandle TakePipe() WARN_UNUSED_RESULT {
    return std::move(pipe_);
  }

 private:
  ScopedMessagePipeHandle pipe_;

  DISALLOW_COPY_AND_ASSIGN(PendingReceiver);
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_PENDING_RECEIVER_H_
