// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/setup/native_messaging_writer.h"

#include <string>

#include "base/basictypes.h"
#include "base/json/json_writer.h"

namespace {

// 4-byte type used for the message header.
typedef uint32 MessageLengthType;

// Defined as an int, for passing to APIs that take an int, to avoid
// signed/unsigned warnings about implicit cast.
const int kMessageHeaderSize = sizeof(MessageLengthType);

// Limit the size of sent messages, since Chrome will not accept messages
// larger than 1MB, and this helps deal with the problem of integer overflow
// when passing sizes to net::FileStream APIs that take |int| parameters.
// This is defined as size_t (unsigned type) so it can be compared with the
// result of std::string::length() without compiler warnings.
const size_t kMaximumMessageSize = 1024 * 1024;

// Performs the same task as FileStream::WriteSync(), but ensures that exactly
// |buffer_length| bytes are written. Unlike WriteSync(), a partial write may
// only occur as a result of end-of-file or fatal error. Returns the number of
// bytes written (buffer_length) or an error-code <= 0.
//
// TODO(lambroslambrou): Add this method to net::FileStream, with unit-tests.
// See http://crbug.com/232202.
int WriteUntilComplete(net::FileStream* out,
                       const char* buffer, int buffer_length) {
  int written = 0;
  while (written < buffer_length) {
    int result = out->WriteSync(buffer + written, buffer_length - written);
    if (result <= 0) {
      return result;
    }
    DCHECK_LE(result, buffer_length - written);
    written += result;
  }
  return written;
}

}  // namespace

namespace remoting {

NativeMessagingWriter::NativeMessagingWriter(base::PlatformFile handle)
    : write_stream_(handle, base::PLATFORM_FILE_WRITE, NULL),
      fail_(false) {
}

NativeMessagingWriter::~NativeMessagingWriter() {
}

bool NativeMessagingWriter::WriteMessage(const base::Value& message) {
  if (fail_) {
    LOG(ERROR) << "Stream marked as corrupt.";
    return false;
  }

  std::string message_json;
  base::JSONWriter::Write(&message, &message_json);

  CHECK_LE(message_json.length(), kMaximumMessageSize);

  // Cast from size_t to the proper header type. The check above ensures this
  // won't overflow.
  MessageLengthType message_length =
      static_cast<MessageLengthType>(message_json.length());

  int result = WriteUntilComplete(
      &write_stream_, reinterpret_cast<char*>(&message_length),
      kMessageHeaderSize);
  if (result != kMessageHeaderSize) {
    LOG(ERROR) << "Failed to send message header, write returned " << result;
    fail_ = true;
    return false;
  }

  // The length check above ensures that the cast won't overflow a signed
  // 32-bit int.
  int message_length_as_int = message_length;

  // CHECK needed since data() is undefined on an empty std::string.
  CHECK(!message_json.empty());
  result = WriteUntilComplete(&write_stream_, message_json.data(),
                              message_length_as_int);
  if (result != message_length_as_int) {
    LOG(ERROR) << "Failed to send message body, write returned " << result;
    fail_ = true;
    return false;
  }

  return true;
}

}  // namespace remoting
