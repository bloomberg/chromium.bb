// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_WEBKIT_COMMON_MESSAGE_PORT_MESSAGE_PORT_CHANNEL_H_
#define THIRD_PARTY_WEBKIT_COMMON_MESSAGE_PORT_MESSAGE_PORT_CHANNEL_H_

#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "mojo/public/cpp/system/watcher.h"
#include "third_party/WebKit/common/common_export.h"

namespace blink {

// MessagePortChannel corresponds to a HTML MessagePort. It is a thin wrapper
// around a Mojo MessagePipeHandle and provides methods for reading and writing
// messages.
//
// A MessagePortChannel is only actively listening for incoming messages once
// SetCallback has been called with a valid callback. If ClearCallback is
// called (or if SetCallback is called with a null callback), then the
// MessagePortChannel will stop listening for incoming messages. The callback
// runs on an unspecified background thread.
//
// Upon destruction, if the MessagePortChannel is listening for incoming
// messages, then the destructor will first synchronize with the background
// thread, waiting for it to finish any in-process callback before closing the
// underlying MessagePipeHandle. This synchronization ensures that any code
// running in the callback can be sure to not worry about the MessagePortChannel
// becoming invalid during callback execution.
//
// MessagePortChannel methods may be used from any thread; however, care must be
// taken when using ReleaseHandle, ReleaseHandles or when destroying a
// MessagePortChannel instance. The MessagePortChannel class does not
// synchronize those methods with methods like PostMessage, GetMessage and
// SetCallback that use the underlying MessagePipeHandle.
//
// TODO(darin): Make this class move-only once no longer used with Chrome IPC.
//
class BLINK_COMMON_EXPORT MessagePortChannel {
 public:
  ~MessagePortChannel();
  MessagePortChannel();

  // Shallow copy, resulting in multiple references to the same port.
  MessagePortChannel(const MessagePortChannel& other);
  MessagePortChannel& operator=(const MessagePortChannel& other);

  explicit MessagePortChannel(mojo::ScopedMessagePipeHandle handle);

  const mojo::ScopedMessagePipeHandle& GetHandle() const;
  mojo::ScopedMessagePipeHandle ReleaseHandle() const;

  static std::vector<mojo::ScopedMessagePipeHandle> ReleaseHandles(
      const std::vector<MessagePortChannel>& ports);
  static std::vector<MessagePortChannel> CreateFromHandles(
      std::vector<mojo::ScopedMessagePipeHandle> handles);

  // Sends an encoded message (along with ports to transfer) to this port's
  // peer.
  void PostMessage(const uint8_t* encoded_message,
                   size_t encoded_message_size,
                   std::vector<MessagePortChannel> ports);

  // Get the next available encoded message if any. Returns true if a message
  // was read.
  bool GetMessage(std::vector<uint8_t>* encoded_message,
                  std::vector<MessagePortChannel>* ports);

  // This callback will be invoked on a background thread when messages are
  // available to be read via GetMessage. It must not synchronously call back
  // into the MessagePortChannel instance.
  void SetCallback(const base::Closure& callback);

  // Clears any callback specified by a prior call to SetCallback.
  void ClearCallback();

 private:
  class State : public base::RefCountedThreadSafe<State> {
   public:
    State();
    explicit State(mojo::ScopedMessagePipeHandle handle);

    void StartWatching(const base::Closure& callback);
    void StopWatching();

    mojo::ScopedMessagePipeHandle TakeHandle();

    const mojo::ScopedMessagePipeHandle& handle() const { return handle_; }

   private:
    friend class base::RefCountedThreadSafe<State>;

    ~State();

    void ArmWatcher();
    void OnHandleReady(MojoResult result);

    static void CallOnHandleReady(uintptr_t context,
                                  MojoResult result,
                                  MojoHandleSignalsState signals_state,
                                  MojoWatcherNotificationFlags flags);

    // Guards access to the fields below.
    base::Lock lock_;

    mojo::ScopedWatcherHandle watcher_handle_;
    mojo::ScopedMessagePipeHandle handle_;

    // Callback to invoke when the State is notified about a change to
    // |handle_|'s signaling state.
    base::Closure callback_;
  };
  mutable scoped_refptr<State> state_;
};

}  // namespace blink

#endif  // THIRD_PARTY_WEBKIT_COMMON_MESSAGE_PORT_MESSAGE_PORT_CHANNEL_H_
