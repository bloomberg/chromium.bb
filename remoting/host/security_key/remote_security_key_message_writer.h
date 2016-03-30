// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_SECURITY_KEY_REMOTE_SECURITY_KEY_MESSAGE_WRITER_H_
#define REMOTING_HOST_SECURITY_KEY_REMOTE_SECURITY_KEY_MESSAGE_WRITER_H_

#include <string>

#include "base/files/file.h"
#include "base/macros.h"
#include "remoting/host/security_key/security_key_message.h"

namespace remoting {

// Used for sending remote security key messages using a file handle.
class RemoteSecurityKeyMessageWriter {
 public:
  explicit RemoteSecurityKeyMessageWriter(base::File output_file);
  ~RemoteSecurityKeyMessageWriter();

  // Writes a remote security key message w/o a payload to |output_stream_|.
  bool WriteMessage(RemoteSecurityKeyMessageType message_type);

  // Writes a remote security key message with a payload to |output_stream_|.
  bool WriteMessageWithPayload(RemoteSecurityKeyMessageType message_type,
                               const std::string& message_payload);

 private:
  // Writes |bytes_to_write| bytes from |message| to |output_stream_|.
  bool WriteBytesToOutput(const char* message, int bytes_to_write);

  base::File output_stream_;
  bool write_failed_ = false;

  DISALLOW_COPY_AND_ASSIGN(RemoteSecurityKeyMessageWriter);
};

}  // namespace remoting

#endif  // REMOTING_HOST_SECURITY_KEY_REMOTE_SECURITY_KEY_MESSAGE_WRITER_H_
