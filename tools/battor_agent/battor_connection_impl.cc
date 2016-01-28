// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/battor_agent/battor_connection_impl.h"

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
    device::serial::DataBits::EIGHT;
const device::serial::ParityBit kBattOrParityBit =
    device::serial::ParityBit::NONE;
const device::serial::StopBits kBattOrStopBit = device::serial::StopBits::ONE;
const bool kBattOrCtsFlowControl = true;
const bool kBattOrHasCtsFlowControl = true;
// The maximum BattOr message is 50kB long.
const size_t kMaxMessageSizeBytes = 50000;

// Returns the maximum number of bytes that could be required to read a message
// of the specified type.
size_t GetMaxBytesForMessageType(BattOrMessageType type) {
  switch (type) {
    case BATTOR_MESSAGE_TYPE_CONTROL:
      return 2 * sizeof(BattOrControlMessage) + 3;
    case BATTOR_MESSAGE_TYPE_CONTROL_ACK:
      // The BattOr EEPROM is sent back with this type, even though it's
      // technically more of a response than an ack. We have to make sure that
      // we read enough bytes to accommodate this behavior.
      return 2 * sizeof(BattOrEEPROM) + 3;
    case BATTOR_MESSAGE_TYPE_SAMPLES:
      return 2 * kMaxMessageSizeBytes + 3;
    default:
      return 0;
  }
}

}  // namespace

BattOrConnectionImpl::BattOrConnectionImpl(
    const std::string& path,
    BattOrConnection::Listener* listener,
    scoped_refptr<base::SingleThreadTaskRunner> file_thread_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> ui_thread_task_runner)
    : BattOrConnection(listener),
      path_(path),
      file_thread_task_runner_(file_thread_task_runner),
      ui_thread_task_runner_(ui_thread_task_runner) {}

BattOrConnectionImpl::~BattOrConnectionImpl() {}

void BattOrConnectionImpl::Open() {
  if (io_handler_) {
    OnOpened(true);
    return;
  }

  io_handler_ = CreateIoHandler();

  device::serial::ConnectionOptions options;
  options.bitrate = kBattOrBitrate;
  options.data_bits = kBattOrDataBits;
  options.parity_bit = kBattOrParityBit;
  options.stop_bits = kBattOrStopBit;
  options.cts_flow_control = kBattOrCtsFlowControl;
  options.has_cts_flow_control = kBattOrHasCtsFlowControl;

  io_handler_->Open(path_, options,
                    base::Bind(&BattOrConnectionImpl::OnOpened, AsWeakPtr()));
}

void BattOrConnectionImpl::OnOpened(bool success) {
  if (!success)
    Close();

  listener_->OnConnectionOpened(success);
}

void BattOrConnectionImpl::Close() {
  io_handler_ = nullptr;
}

void BattOrConnectionImpl::SendBytes(BattOrMessageType type,
                                     const void* buffer,
                                     size_t bytes_to_send) {
  const char* bytes = reinterpret_cast<const char*>(buffer);

  // Reserve a send buffer with enough extra bytes for the start, type, end, and
  // escape bytes.
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
      data, base::Bind(&BattOrConnectionImpl::OnBytesSent, AsWeakPtr()))));
}

void BattOrConnectionImpl::ReadMessage(BattOrMessageType type) {
  pending_read_message_type_ = type;
  size_t max_bytes_to_read = GetMaxBytesForMessageType(type);

  // Check the left-over bytes from the last read to make sure that we don't
  // already have a full message.
  BattOrMessageType parsed_type;
  scoped_ptr<vector<char>> bytes(new vector<char>());
  bytes->reserve(max_bytes_to_read);

  if (ParseMessage(&parsed_type, bytes.get())) {
    listener_->OnMessageRead(true, parsed_type, std::move(bytes));
    return;
  }

  BeginReadBytes(max_bytes_to_read - already_read_buffer_.size());
}

void BattOrConnectionImpl::Flush() {
  io_handler_->Flush();
  already_read_buffer_.clear();
}

scoped_refptr<device::SerialIoHandler> BattOrConnectionImpl::CreateIoHandler() {
  return device::SerialIoHandler::Create(file_thread_task_runner_,
                                         ui_thread_task_runner_);
}

void BattOrConnectionImpl::BeginReadBytes(size_t max_bytes_to_read) {
  pending_read_buffer_ =
      make_scoped_refptr(new net::IOBuffer(max_bytes_to_read));

  auto on_receive_buffer_filled =
      base::Bind(&BattOrConnectionImpl::OnBytesRead, AsWeakPtr());

  io_handler_->Read(make_scoped_ptr(new device::ReceiveBuffer(
      pending_read_buffer_, static_cast<uint32_t>(max_bytes_to_read),
      on_receive_buffer_filled)));
}

void BattOrConnectionImpl::OnBytesRead(int bytes_read,
                                       device::serial::ReceiveError error) {
  if (bytes_read == 0 || error != device::serial::ReceiveError::NONE) {
    // If we didn't have a message before, and we weren't able to read any
    // additional bytes, then there's no valid message available.
    EndReadBytes(false, BATTOR_MESSAGE_TYPE_CONTROL, nullptr);
    return;
  }

  already_read_buffer_.insert(already_read_buffer_.end(),
                              pending_read_buffer_->data(),
                              pending_read_buffer_->data() + bytes_read);

  BattOrMessageType type;
  scoped_ptr<vector<char>> bytes(new vector<char>());
  bytes->reserve(GetMaxBytesForMessageType(pending_read_message_type_));

  if (!ParseMessage(&type, bytes.get())) {
    // Even after reading the max number of bytes, we still don't have a valid
    // message.
    EndReadBytes(false, BATTOR_MESSAGE_TYPE_CONTROL, nullptr);
    return;
  }

  if (type != pending_read_message_type_) {
    // We received a complete message, but it wasn't the type we were expecting.
    EndReadBytes(false, BATTOR_MESSAGE_TYPE_CONTROL, nullptr);
    return;
  }

  EndReadBytes(true, type, std::move(bytes));
}

void BattOrConnectionImpl::EndReadBytes(bool success,
                                        BattOrMessageType type,
                                        scoped_ptr<std::vector<char>> bytes) {
  pending_read_buffer_ = nullptr;

  listener_->OnMessageRead(success, type, std::move(bytes));
}

bool BattOrConnectionImpl::ParseMessage(BattOrMessageType* type,
                                        vector<char>* bytes) {
  if (already_read_buffer_.size() <= 3)
    return false;

  // The first byte is the start byte.
  if (already_read_buffer_[0] != BATTOR_CONTROL_BYTE_START) {
    return false;
  }

  // The second byte specifies the message type.
  *type = static_cast<BattOrMessageType>(already_read_buffer_[1]);

  if (*type < static_cast<uint8_t>(BATTOR_MESSAGE_TYPE_CONTROL) ||
      *type > static_cast<uint8_t>(BATTOR_MESSAGE_TYPE_PRINT)) {
    return false;
  }

  // After that comes the message bytes.
  bool escape_next_byte = false;
  for (size_t i = 2; i < already_read_buffer_.size(); i++) {
    char next_byte = already_read_buffer_[i];

    if (escape_next_byte) {
      bytes->push_back(next_byte);
      escape_next_byte = false;
      continue;
    }

    switch (next_byte) {
      case BATTOR_CONTROL_BYTE_START:
        // Two start bytes in a message is invalid.
        return false;

      case BATTOR_CONTROL_BYTE_END:
        already_read_buffer_.erase(already_read_buffer_.begin(),
                                   already_read_buffer_.begin() + i + 1);
        return true;

      case BATTOR_CONTROL_BYTE_ESCAPE:
        escape_next_byte = true;
        continue;

      default:
        bytes->push_back(next_byte);
    }
  }

  // If we made it to the end of the read buffer and no end byte was seen, then
  // we don't have a complete message.
  return false;
}

void BattOrConnectionImpl::OnBytesSent(int bytes_sent,
                                       device::serial::SendError error) {
  bool success = (error == device::serial::SendError::NONE) &&
                 (pending_write_length_ == static_cast<size_t>(bytes_sent));
  listener_->OnBytesSent(success);
}

}  // namespace battor
