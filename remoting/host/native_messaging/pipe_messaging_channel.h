// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_NATIVE_MESSAGING_PIPE_MESSAGING_CHANNEL_H_
#define REMOTING_HOST_NATIVE_MESSAGING_PIPE_MESSAGING_CHANNEL_H_

#include "base/callback.h"
#include "base/files/file.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/non_thread_safe.h"
#include "extensions/browser/api/messaging/native_messaging_channel.h"
#include "remoting/host/native_messaging/native_messaging_reader.h"
#include "remoting/host/native_messaging/native_messaging_writer.h"

namespace base {
class DictionaryValue;
class Value;
}  // namespace base

namespace remoting {

// An implementation of extensions::NativeMessagingChannel using a pipe. It
// is used by the It2MeNativeMessagingHost and Me2MeNativeMessagingHost to
// communicate with the chrome process.
// TODO(kelvinp): Move this class to the extensions/browser/api/messaging
// directory.
class PipeMessagingChannel : public extensions::NativeMessagingChannel,
                             public base::NonThreadSafe {
 public:
  typedef extensions::NativeMessagingChannel::EventHandler EventHandler;

  // Constructs an object taking the ownership of |input| and |output|. Closes
  // |input| and |output| to prevent the caller from using them.
  PipeMessagingChannel(base::File input, base::File output);
  virtual ~PipeMessagingChannel();

  // extensions::NativeMessagingChannel implementation.
  virtual void Start(EventHandler* event_handler) OVERRIDE;
  virtual void SendMessage(scoped_ptr<base::Value> message) OVERRIDE;

 private:
  // Processes a message received from the client app.
  void ProcessMessage(scoped_ptr<base::Value> message);

  // Initiates shutdown.
  void Shutdown();

  NativeMessagingReader native_messaging_reader_;
  scoped_ptr<NativeMessagingWriter> native_messaging_writer_;

  EventHandler* event_handler_;
  base::WeakPtr<PipeMessagingChannel> weak_ptr_;
  base::WeakPtrFactory<PipeMessagingChannel> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(PipeMessagingChannel);
};

}  // namespace remoting

#endif  // REMOTING_HOST_NATIVE_MESSAGING_PIPE_MESSAGING_CHANNEL_H_
