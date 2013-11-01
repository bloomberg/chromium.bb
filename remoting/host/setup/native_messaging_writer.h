// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_SETUP_NATIVE_MESSAGING_WRITER_H_
#define REMOTING_HOST_SETUP_NATIVE_MESSAGING_WRITER_H_

#include "base/platform_file.h"
#include "net/base/file_stream.h"

namespace base {
class Value;
}  // namespace base

namespace remoting {

// This class is used for sending messages to the Native Messaging client
// webapp.
class NativeMessagingWriter {
 public:
  explicit NativeMessagingWriter(base::PlatformFile handle);
  ~NativeMessagingWriter();

  // Sends a message to the Native Messaging client, returning true if
  // successful.
  bool WriteMessage(const base::Value& message);

 private:
  net::FileStream write_stream_;
  bool fail_;

  DISALLOW_COPY_AND_ASSIGN(NativeMessagingWriter);
};

}  // namespace remoting

#endif  // REMOTING_HOST_SETUP_NATIVE_MESSAGING_WRITER_H_
