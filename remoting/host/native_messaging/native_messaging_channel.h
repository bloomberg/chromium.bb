// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_NATIVE_MESSAGING_NATIVE_MESSAGING_CHANNEL_H_
#define REMOTING_HOST_NATIVE_MESSAGING_NATIVE_MESSAGING_CHANNEL_H_

#include "base/callback.h"
#include "base/files/file.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/non_thread_safe.h"
#include "remoting/host/native_messaging/native_messaging_reader.h"
#include "remoting/host/native_messaging/native_messaging_writer.h"

namespace base {
class DictionaryValue;
class Value;
}  // namespace base

namespace remoting {

// Implements reading messages and sending responses across the native messaging
// host pipe.
class NativeMessagingChannel : public base::NonThreadSafe {
 public:
  // Used to send a message to the client app.
  typedef base::Callback<void(scoped_ptr<base::DictionaryValue> message)>
      SendMessageCallback;

  // Constructs an object taking the ownership of |input| and |output|. Closes
  // |input| and |output| to prevent the caller from using them.
  NativeMessagingChannel(base::File input, base::File output);
  ~NativeMessagingChannel();

  // Starts reading and processing messages.
  void Start(const SendMessageCallback& received_message,
             const base::Closure& quit_closure);

  // Sends a message to the client app.
  void SendMessage(scoped_ptr<base::DictionaryValue> message);

 private:
  // Processes a message received from the client app.
  void ProcessMessage(scoped_ptr<base::Value> message);

  // Initiates shutdown and runs |quit_closure| if there are no pending requests
  // left.
  void Shutdown();

  base::Closure quit_closure_;

  NativeMessagingReader native_messaging_reader_;
  scoped_ptr<NativeMessagingWriter> native_messaging_writer_;

  // The callback to invoke when a message is received.
  SendMessageCallback received_message_;

  base::WeakPtr<NativeMessagingChannel> weak_ptr_;
  base::WeakPtrFactory<NativeMessagingChannel> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(NativeMessagingChannel);
};

}  // namespace remoting

#endif  // REMOTING_HOST_NATIVE_MESSAGING_NATIVE_MESSAGING_CHANNEL_H_
