// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/battor_agent/battor_connection.h"

#include "base/bind.h"
#include "base/callback.h"
#include "device/serial/buffer.h"
#include "device/serial/serial_io_handler.h"
#include "net/base/io_buffer.h"

using std::vector;

namespace battor {

namespace {

// Serial configuration parameters for the BattOr.
const uint32_t kBattOrBitrate = 2000000;
const device::serial::DataBits kBattOrDataBits =
    device::serial::DATA_BITS_EIGHT;
const device::serial::ParityBit kBattOrParityBit =
    device::serial::PARITY_BIT_NONE;
const device::serial::StopBits kBattOrStopBit = device::serial::STOP_BITS_ONE;
const bool kBattOrCtsFlowControl = true;
const bool kBattOrHasCtsFlowControl = true;
const uint32_t kMaxMessageSize = 50000;

// MessageHealth describes the possible healthiness states that a partially
// received message could be in.
enum class MessageHealth {
  INVALID,
  INCOMPLETE,
  COMPLETE,
};

// Parses the specified message.
//   - message: The incoming message that needs to be parsed.
//   - parsed_content: Output argument for the message content after removal of
//     any start, end, type, and escape bytes.
//   - health: Output argument for the health of the message.
//   - type: Output argument for the type of message being parsed.
//   - escape_byte_count: Output argument for the number of escape bytes
//     removed from the parsed content.
void ParseMessage(const vector<char>& message,
                  vector<char>* parsed_content,
                  MessageHealth* health,
                  BattOrMessageType* type,
                  size_t* escape_byte_count) {
  *health = MessageHealth::INCOMPLETE;
  *type = BATTOR_MESSAGE_TYPE_CONTROL;
  *escape_byte_count = 0;
  parsed_content->reserve(message.size());

  if (message.size() == 0)
    return;

  // The first byte is the start byte.
  if (message[0] != BATTOR_CONTROL_BYTE_START) {
    *health = MessageHealth::INVALID;
    return;
  }

  if (message.size() == 1)
    return;

  // The second byte specifies the message type.
  *type = static_cast<BattOrMessageType>(message[1]);

  if (*type < static_cast<uint8_t>(BATTOR_MESSAGE_TYPE_CONTROL) ||
      *type > static_cast<uint8_t>(BATTOR_MESSAGE_TYPE_PRINT)) {
    *health = MessageHealth::INVALID;
    return;
  }

  // After that comes the message data.
  bool escape_next_byte = false;
  for (size_t i = 2; i < message.size(); i++) {
    if (i >= kMaxMessageSize) {
      *health = MessageHealth::INVALID;
      return;
    }

    char next_byte = message[i];

    if (escape_next_byte) {
      parsed_content->push_back(next_byte);
      escape_next_byte = false;
      continue;
    }

    switch (next_byte) {
      case BATTOR_CONTROL_BYTE_START:
        // Two start bytes in a message is invalid.
        *health = MessageHealth::INVALID;
        return;

      case BATTOR_CONTROL_BYTE_END:
        if (i != message.size() - 1) {
          // We're only parsing a single message here. If we received more bytes
          // after the end byte, what we've received so far is *not* valid.
          *health = MessageHealth::INVALID;
          return;
        }

        *health = MessageHealth::COMPLETE;
        return;

      case BATTOR_CONTROL_BYTE_ESCAPE:
        escape_next_byte = true;
        (*escape_byte_count)++;
        continue;

      default:
        parsed_content->push_back(next_byte);
    }
  }
}

}  // namespace

BattOrConnection::BattOrConnection(
    const std::string& path,
    Listener* listener,
    scoped_refptr<base::SingleThreadTaskRunner> file_thread_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> ui_thread_task_runner)
    : path_(path),
      listener_(listener),
      file_thread_task_runner_(file_thread_task_runner),
      ui_thread_task_runner_(ui_thread_task_runner) {}

BattOrConnection::~BattOrConnection() {}

void BattOrConnection::Open() {
  io_handler_ = CreateIoHandler();

  device::serial::ConnectionOptions options;
  options.bitrate = kBattOrBitrate;
  options.data_bits = kBattOrDataBits;
  options.parity_bit = kBattOrParityBit;
  options.stop_bits = kBattOrStopBit;
  options.cts_flow_control = kBattOrCtsFlowControl;
  options.has_cts_flow_control = kBattOrHasCtsFlowControl;

  io_handler_->Open(path_, options,
                    base::Bind(&BattOrConnection::OnOpened, AsWeakPtr()));
}

void BattOrConnection::OnOpened(bool success) {
  if (!success)
    Close();

  listener_->OnConnectionOpened(success);
}

bool BattOrConnection::IsOpen() {
  return io_handler_;
}

void BattOrConnection::Close() {
  io_handler_ = nullptr;
}

void BattOrConnection::SendBytes(BattOrMessageType type,
                                 const void* buffer,
                                 size_t bytes_to_send) {
  const char* bytes = reinterpret_cast<const char*>(buffer);

  // Reserve a send buffer with 3 extra bytes (start, type, and end byte) and
  // twice as many bytes as we're actually sending, because each raw data byte
  // might need to be escaped.
  vector<char> data;
  data.reserve(2 * bytes_to_send + 3);

  data.push_back(BATTOR_CONTROL_BYTE_START);
  data.push_back(type);

  for (size_t i = 0; i < bytes_to_send; i++) {
    if (bytes[i] == BATTOR_CONTROL_BYTE_START ||
        bytes[i] == BATTOR_CONTROL_BYTE_END) {
      data.push_back(BATTOR_CONTROL_BYTE_ESCAPE);
    }

    data.push_back(bytes[i]);
  }

  data.push_back(BATTOR_CONTROL_BYTE_END);

  pending_write_length_ = data.size();
  io_handler_->Write(make_scoped_ptr(new device::SendBuffer(
      data, base::Bind(&BattOrConnection::OnBytesSent, AsWeakPtr()))));
}

void BattOrConnection::ReadBytes(size_t bytes_to_read) {
  // Allocate a read buffer and reserve enough space in it to account for the
  // start, type, end, and escape bytes.
  pending_read_buffer_.reset(new vector<char>());
  pending_read_buffer_->reserve(2 * bytes_to_read + 3);
  pending_read_escape_byte_count_ = 0;

  // Add 3 bytes to however many bytes the caller requested because we know
  // we'll have to read the start, type, and end bytes.
  bytes_to_read += 3;

  ReadMoreBytes(bytes_to_read);
}

void BattOrConnection::Flush() {
  io_handler_->Flush();
}

scoped_refptr<device::SerialIoHandler> BattOrConnection::CreateIoHandler() {
  return device::SerialIoHandler::Create(file_thread_task_runner_,
                                         ui_thread_task_runner_);
}

void BattOrConnection::ReadMoreBytes(size_t bytes_to_read) {
  last_read_buffer_ = make_scoped_refptr(new net::IOBuffer(bytes_to_read));
  auto on_receive_buffer_filled =
      base::Bind(&BattOrConnection::OnBytesRead, AsWeakPtr());

  pending_read_length_ = bytes_to_read;
  io_handler_->Read(make_scoped_ptr(new device::ReceiveBuffer(
      last_read_buffer_, bytes_to_read, on_receive_buffer_filled)));
}

void BattOrConnection::OnBytesRead(int bytes_read,
                                   device::serial::ReceiveError error) {
  if ((static_cast<size_t>(bytes_read) < pending_read_length_) ||
      (error != device::serial::RECEIVE_ERROR_NONE)) {
    listener_->OnBytesRead(false, BATTOR_MESSAGE_TYPE_CONTROL, nullptr);
    return;
  }

  pending_read_buffer_->insert(pending_read_buffer_->end(),
                               last_read_buffer_->data(),
                               last_read_buffer_->data() + bytes_read);

  scoped_ptr<vector<char>> parsed_content(new vector<char>());
  MessageHealth health;
  BattOrMessageType type;
  size_t escape_byte_count;

  ParseMessage(*pending_read_buffer_, parsed_content.get(), &health, &type,
               &escape_byte_count);

  if (health == MessageHealth::INVALID) {
    // If we already have an invalid message, there's no sense in continuing to
    // process it.
    listener_->OnBytesRead(false, BATTOR_MESSAGE_TYPE_CONTROL, nullptr);
    return;
  }

  size_t new_escape_bytes = escape_byte_count - pending_read_escape_byte_count_;
  pending_read_escape_byte_count_ = escape_byte_count;

  if (new_escape_bytes > 0) {
    // When the caller requested that we read X additional bytes, they weren't
    // taking into account any escape bytes that we received. Because we got
    // some escape bytes, we need to fire off another read to get the rest of
    // the data.
    ReadMoreBytes(new_escape_bytes);
    return;
  }

  if (health == MessageHealth::INCOMPLETE)
    // If everything is valid and we didn't see any escape bytes, then we should
    // have the whole message. If we don't, the message was malformed.
    listener_->OnBytesRead(false, BATTOR_MESSAGE_TYPE_CONTROL, nullptr);

  // If we've gotten this far, we've received the whole, well-formed message.
  listener_->OnBytesRead(true, type, std::move(parsed_content));
}

void BattOrConnection::OnBytesSent(int bytes_sent,
                                   device::serial::SendError error) {
  bool success = (error == device::serial::SEND_ERROR_NONE) &&
                 (pending_write_length_ == static_cast<size_t>(bytes_sent));
  listener_->OnBytesSent(success);
}

}  // namespace battor
