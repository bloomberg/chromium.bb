// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_SECURITY_KEY_REMOTE_SECURITY_KEY_MESSAGE_WRITER_IMPL_H_
#define REMOTING_HOST_SECURITY_KEY_REMOTE_SECURITY_KEY_MESSAGE_WRITER_IMPL_H_

#include <string>

#include "base/files/file.h"
#include "base/macros.h"
#include "remoting/host/security_key/remote_security_key_message_writer.h"
#include "remoting/host/security_key/security_key_message.h"

namespace remoting {

// Used for sending remote security key messages using a file handle.
class RemoteSecurityKeyMessageWriterImpl
    : public RemoteSecurityKeyMessageWriter {
 public:
  explicit RemoteSecurityKeyMessageWriterImpl(base::File output_file);
  ~RemoteSecurityKeyMessageWriterImpl() override;

 private:
  // RemoteSecurityKeyMessageWriter interface.
  bool WriteMessage(RemoteSecurityKeyMessageType message_type) override;
  bool WriteMessageWithPayload(RemoteSecurityKeyMessageType message_type,
                               const std::string& message_payload) override;

  // Writes |bytes_to_write| bytes from |message| to |output_stream_|.
  bool WriteBytesToOutput(const char* message, int bytes_to_write);

  base::File output_stream_;
  bool write_failed_ = false;

  DISALLOW_COPY_AND_ASSIGN(RemoteSecurityKeyMessageWriterImpl);
};

}  // namespace remoting

#endif  // REMOTING_HOST_SECURITY_KEY_REMOTE_SECURITY_KEY_MESSAGE_WRITER_IMPL_H_
