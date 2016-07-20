// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_SECURITY_KEY_FAKE_REMOTE_SECURITY_KEY_MESSAGE_READER_H_
#define REMOTING_HOST_SECURITY_KEY_FAKE_REMOTE_SECURITY_KEY_MESSAGE_READER_H_

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "remoting/host/security_key/remote_security_key_message_reader.h"
#include "remoting/host/security_key/security_key_message.h"

namespace remoting {

// Simulates the RemoteSecurityKeyMessageReader and provides access to data
// members for testing.
class FakeRemoteSecurityKeyMessageReader
    : public RemoteSecurityKeyMessageReader {
 public:
  FakeRemoteSecurityKeyMessageReader();
  ~FakeRemoteSecurityKeyMessageReader() override;

  // RemoteSecurityKeyMessageReader interface.
  void Start(const SecurityKeyMessageCallback& message_callback,
             const base::Closure& error_callback) override;

  base::WeakPtr<FakeRemoteSecurityKeyMessageReader> AsWeakPtr();

  const SecurityKeyMessageCallback& message_callback() {
    return message_callback_;
  }

  const base::Closure& error_callback() { return error_callback_; }

 private:
  // Caller-supplied message and error callbacks.
  SecurityKeyMessageCallback message_callback_;
  base::Closure error_callback_;

  base::WeakPtrFactory<FakeRemoteSecurityKeyMessageReader> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(FakeRemoteSecurityKeyMessageReader);
};

}  // namespace remoting

#endif  // REMOTING_HOST_SECURITY_KEY_FAKE_REMOTE_SECURITY_KEY_MESSAGE_READER_H_
