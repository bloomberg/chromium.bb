// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_NATIVE_MESSAGING_NATIVE_MESSAGING_CHANNEL_H_
#define REMOTING_HOST_NATIVE_MESSAGING_NATIVE_MESSAGING_CHANNEL_H_

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/platform_file.h"
#include "base/threading/non_thread_safe.h"
#include "remoting/host/native_messaging/native_messaging_reader.h"
#include "remoting/host/native_messaging/native_messaging_writer.h"

namespace base {
class DictionaryValue;
class Value;
}  // namespace base

namespace remoting {

// Implements reading messages and sending responses across the native messaging
// host pipe. Delegates processing of received messages to Delegate.
//
// TODO(alexeypa): Add ability to switch between different |delegate_| pointers
// on the fly. This is useful for implementing UAC-style elevation on Windows -
// an unprivileged delegate could be replaced with another delegate that
// forwards messages to the elevated instance of the native messaging host.
class NativeMessagingChannel : public base::NonThreadSafe {
 public:
  // Used to sends a response back to the client app.
  typedef base::Callback<void(scoped_ptr<base::DictionaryValue> response)>
      SendResponseCallback;

  class Delegate {
   public:
    virtual ~Delegate() {}

    // Processes a message received from the client app. Invokes |done| to send
    // a response back to the client app.
    virtual void ProcessMessage(scoped_ptr<base::DictionaryValue> message,
                                const SendResponseCallback& done) = 0;
  };

  // Constructs an object taking the ownership of |input| and |output|. Closes
  // |input| and |output| to prevent the caller from using them.
  NativeMessagingChannel(
      scoped_ptr<Delegate> delegate,
      base::PlatformFile input,
      base::PlatformFile output);
  ~NativeMessagingChannel();

  // Starts reading and processing messages.
  void Start(const base::Closure& quit_closure);

 private:
  // Processes a message received from the client app.
  void ProcessMessage(scoped_ptr<base::Value> message);

  // Sends a response back to the client app.
  void SendResponse(scoped_ptr<base::DictionaryValue> response);

  // Initiates shutdown and runs |quit_closure| if there are no pending requests
  // left.
  void Shutdown();

  base::Closure quit_closure_;

  NativeMessagingReader native_messaging_reader_;
  scoped_ptr<NativeMessagingWriter> native_messaging_writer_;

  // |delegate_| may post tasks to this object during destruction (but not
  // afterwards), so it needs to be destroyed before other members of this class
  // (except for |weak_factory_|).
  scoped_ptr<Delegate> delegate_;

  // Keeps track of pending requests. Used to delay shutdown until all responses
  // have been sent.
  int pending_requests_;

  // True if Shutdown() has been called.
  bool shutdown_;

  base::WeakPtr<NativeMessagingChannel> weak_ptr_;
  base::WeakPtrFactory<NativeMessagingChannel> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(NativeMessagingChannel);
};

}  // namespace remoting

#endif  // REMOTING_HOST_NATIVE_MESSAGING_NATIVE_MESSAGING_CHANNEL_H_
