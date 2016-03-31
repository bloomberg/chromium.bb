// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_SECURITY_KEY_REMOTE_SECURITY_KEY_MESSAGE_READER_H_
#define REMOTING_HOST_SECURITY_KEY_REMOTE_SECURITY_KEY_MESSAGE_READER_H_

#include "base/callback.h"
#include "base/files/file.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread.h"
#include "remoting/host/security_key/security_key_message.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace remoting {

// Used for receiving remote security key messages using a file handle.
class RemoteSecurityKeyMessageReader {
 public:
  explicit RemoteSecurityKeyMessageReader(base::File input_file);
  ~RemoteSecurityKeyMessageReader();

  // Starts reading messages from the input file provided in the C'Tor.
  // |message_callback| is called for each received message.
  // |error_callback| is called in case of an error or the file is closed.
  // This method is asynchronous, callbacks will be called on the thread this
  // method is called on.  These callbacks can be called up to the point this
  // instance is destroyed and may be destroyed as a result of the callback
  // being invoked.
  void Start(SecurityKeyMessageCallback message_callback,
             base::Closure error_callback);

 private:
  // Reads a message from the remote security key process and passes it to
  // |message_callback_| on the originating thread. Run on |read_task_runner_|.
  void ReadMessage();

  // Callback run on |read_task_runner_| when an error occurs or EOF is reached.
  void NotifyError();

  // Used for callbacks on the appropriate task runner to signal status changes.
  // These callbacks are invoked on |main_task_runner_|.
  void InvokeMessageCallback(scoped_ptr<SecurityKeyMessage> message);
  void InvokeErrorCallback();

  base::File read_stream_;

  // Caller-supplied message and error callbacks.
  SecurityKeyMessageCallback message_callback_;
  base::Closure error_callback_;

  // Thread used for blocking IO operations.
  base::Thread reader_thread_;
  scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> read_task_runner_;

  base::WeakPtr<RemoteSecurityKeyMessageReader> reader_;
  base::WeakPtrFactory<RemoteSecurityKeyMessageReader> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(RemoteSecurityKeyMessageReader);
};

}  // namespace remoting

#endif  // REMOTING_HOST_SECURITY_KEY_REMOTE_SECURITY_KEY_MESSAGE_READER_H_
