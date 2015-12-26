// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/battor_agent/battor_connection.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/weak_ptr.h"
#include "device/serial/serial.mojom.h"
#include "device/serial/test_serial_io_handler.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "tools/battor_agent/battor_protocol_types.h"

namespace {

void NullWriteCallback(int, device::serial::SendError) {}
void NullReadCallback(int, device::serial::ReceiveError) {}

}  // namespace

namespace battor {

// TestableBattOrConnection uses a fake serial connection be testable.
class TestableBattOrConnection : public BattOrConnection {
 public:
  TestableBattOrConnection(BattOrConnection::Listener* listener)
      : BattOrConnection("/dev/test", listener, nullptr, nullptr) {}
  scoped_refptr<device::SerialIoHandler> CreateIoHandler() override {
    return device::TestSerialIoHandler::Create();
  }

  scoped_refptr<device::SerialIoHandler> GetIoHandler() { return io_handler_; }
};

// BattOrConnectionTest provides a BattOrConnection and captures the
// results of all its commands.
class BattOrConnectionTest : public testing::Test,
                             public BattOrConnection::Listener {
 public:
  void OnConnectionOpened(bool success) override { open_success_ = success; };
  void OnBytesSent(bool success) override { send_success_ = success; }
  void OnBytesRead(bool success,
                   BattOrMessageType type,
                   scoped_ptr<std::vector<char>> bytes) override {
    is_read_complete_ = true;
    read_success_ = success;
    read_type_ = type;
    read_bytes_ = std::move(bytes);
  }

 protected:
  void SetUp() override {
    connection_.reset(new TestableBattOrConnection(this));
  }

  void OpenConnection() { connection_->Open(); }

  void ReadBytes(uint16_t bytes_to_read) {
    is_read_complete_ = false;
    connection_->ReadBytes(bytes_to_read);
  }

  // Reads the specified number of bytes directly from the serial connection.
  scoped_refptr<net::IOBuffer> ReadBytesRaw(int bytes_to_read) {
    scoped_refptr<net::IOBuffer> buffer(
        new net::IOBuffer((size_t)bytes_to_read));

    connection_->GetIoHandler()->Read(make_scoped_ptr(new device::ReceiveBuffer(
        buffer, bytes_to_read, base::Bind(&NullReadCallback))));

    return buffer;
  }

  void SendControlMessage(BattOrControlMessageType type,
                          uint16_t param1,
                          uint16_t param2) {
    BattOrControlMessage msg{type, param1, param2};
    connection_->SendBytes(BATTOR_MESSAGE_TYPE_CONTROL,
                           reinterpret_cast<char*>(&msg), sizeof(msg));
  }

  // Writes the specified bytes directly to the serial connection.
  void SendBytesRaw(const char* data, uint16_t bytes_to_send) {
    std::vector<char> data_vector(data, data + bytes_to_send);
    connection_->GetIoHandler()->Write(make_scoped_ptr(
        new device::SendBuffer(data_vector, base::Bind(&NullWriteCallback))));
  }

  bool GetOpenSuccess() { return open_success_; }
  bool GetSendSuccess() { return send_success_; }
  bool IsReadComplete() { return is_read_complete_; }
  bool GetReadSuccess() { return read_success_; }
  BattOrMessageType GetReadType() { return read_type_; }
  std::vector<char>* GetReadBytes() { return read_bytes_.get(); }

 private:
  scoped_ptr<TestableBattOrConnection> connection_;

  // Result from the last connect command.
  bool open_success_;
  // Result from the last send command.
  bool send_success_;
  // Results from the last read command.
  bool is_read_complete_;
  bool read_success_;
  BattOrMessageType read_type_;
  scoped_ptr<std::vector<char>> read_bytes_;
};

TEST_F(BattOrConnectionTest, InitSendsCorrectBytes) {
  OpenConnection();
  ASSERT_TRUE(GetOpenSuccess());

  SendControlMessage(BATTOR_CONTROL_MESSAGE_TYPE_INIT, 0, 0);

  const char expected_data[] = {
      BATTOR_CONTROL_BYTE_START,  BATTOR_MESSAGE_TYPE_CONTROL,
      BATTOR_CONTROL_BYTE_ESCAPE, BATTOR_CONTROL_MESSAGE_TYPE_INIT,
      BATTOR_CONTROL_BYTE_ESCAPE, 0x00,
      BATTOR_CONTROL_BYTE_ESCAPE, 0x00,
      BATTOR_CONTROL_BYTE_ESCAPE, 0x00,
      BATTOR_CONTROL_BYTE_ESCAPE, 0x00,
      BATTOR_CONTROL_BYTE_END,
  };

  ASSERT_TRUE(GetSendSuccess());
  ASSERT_EQ(0, std::memcmp(ReadBytesRaw(13)->data(), expected_data, 13));
}

TEST_F(BattOrConnectionTest, ResetSendsCorrectBytes) {
  OpenConnection();
  ASSERT_TRUE(GetOpenSuccess());

  SendControlMessage(BATTOR_CONTROL_MESSAGE_TYPE_RESET, 0, 0);

  const char expected_data[] = {
      BATTOR_CONTROL_BYTE_START,
      BATTOR_MESSAGE_TYPE_CONTROL,
      BATTOR_CONTROL_MESSAGE_TYPE_RESET,
      BATTOR_CONTROL_BYTE_ESCAPE,
      0x00,
      BATTOR_CONTROL_BYTE_ESCAPE,
      0x00,
      BATTOR_CONTROL_BYTE_ESCAPE,
      0x00,
      BATTOR_CONTROL_BYTE_ESCAPE,
      0x00,
      BATTOR_CONTROL_BYTE_END,
  };

  ASSERT_TRUE(GetSendSuccess());
  ASSERT_EQ(0, std::memcmp(ReadBytesRaw(12)->data(), expected_data, 12));
}

TEST_F(BattOrConnectionTest, ReadBytesControlMessage) {
  OpenConnection();
  ASSERT_TRUE(GetOpenSuccess());

  const char data[] = {
      BATTOR_CONTROL_BYTE_START,
      BATTOR_MESSAGE_TYPE_CONTROL,
      BATTOR_CONTROL_MESSAGE_TYPE_RESET,
      0x04,
      0x04,
      0x04,
      0x04,
      BATTOR_CONTROL_BYTE_END,
  };
  SendBytesRaw(data, 8);
  ReadBytes(5);

  const char expected[] = {BATTOR_CONTROL_MESSAGE_TYPE_RESET, 0x04, 0x04, 0x04,
                           0x04};

  ASSERT_TRUE(IsReadComplete());
  ASSERT_TRUE(GetReadSuccess());
  ASSERT_EQ(BATTOR_MESSAGE_TYPE_CONTROL, GetReadType());
  ASSERT_EQ(0, std::memcmp(GetReadBytes()->data(), expected, 5));
}

TEST_F(BattOrConnectionTest, ReadBytesNotEnoughBytes) {
  OpenConnection();
  ASSERT_TRUE(GetOpenSuccess());

  // 3 (h/f), 1 (control message type), 4 (data)
  SendControlMessage(BATTOR_CONTROL_MESSAGE_TYPE_RESET, 0, 0);
  ReadBytes(sizeof(BattOrControlMessage) + 1);

  ASSERT_FALSE(IsReadComplete());
}

TEST_F(BattOrConnectionTest, ReadBytesInvalidType) {
  OpenConnection();
  ASSERT_TRUE(GetOpenSuccess());

  const char data[] = {
      BATTOR_CONTROL_BYTE_START,
      UINT8_MAX,
      BATTOR_CONTROL_MESSAGE_TYPE_INIT,
      0x04,
      0x04,
      BATTOR_CONTROL_BYTE_END,
  };
  SendBytesRaw(data, 8);

  ReadBytes(3);

  ASSERT_TRUE(IsReadComplete());
  ASSERT_FALSE(GetReadSuccess());
}

TEST_F(BattOrConnectionTest, ReadBytesWithEscapeCharacters) {
  OpenConnection();
  ASSERT_TRUE(GetOpenSuccess());

  const char data[] = {
      BATTOR_CONTROL_BYTE_START,
      BATTOR_MESSAGE_TYPE_CONTROL_ACK,
      BATTOR_CONTROL_MESSAGE_TYPE_RESET,
      BATTOR_CONTROL_BYTE_ESCAPE,
      0x00,
      BATTOR_CONTROL_BYTE_END,
  };
  SendBytesRaw(data, 6);

  ReadBytes(2);

  const char expected[] = {BATTOR_CONTROL_MESSAGE_TYPE_RESET, 0x00};

  ASSERT_TRUE(IsReadComplete());
  ASSERT_TRUE(GetReadSuccess());
  ASSERT_EQ(BATTOR_MESSAGE_TYPE_CONTROL_ACK, GetReadType());
  ASSERT_EQ(0, std::memcmp(GetReadBytes()->data(), expected, 2));
}

TEST_F(BattOrConnectionTest, ReadBytesWithEscapeCharactersInSubsequentReads) {
  OpenConnection();
  ASSERT_TRUE(GetOpenSuccess());

  // The first read should request 7 bytes. Of those 7 bytes, though, 2 of them
  // are escape bytes, so we'll then do a second read of 2 bytes. In that second
  // read, we'll see another escape byte, so we'll have to do a third read of 1
  // byte. That third read should complete the message.
  const char data[] = {
      // These bytes make up the first read.
      BATTOR_CONTROL_BYTE_START, BATTOR_MESSAGE_TYPE_CONTROL_ACK,
      BATTOR_CONTROL_MESSAGE_TYPE_RESET, BATTOR_CONTROL_BYTE_ESCAPE, 0x00,
      BATTOR_CONTROL_BYTE_ESCAPE, 0x00,
      // These bytes make up the second read.
      BATTOR_CONTROL_BYTE_ESCAPE, 0x00,
      // This byte makes up the third read.
      BATTOR_CONTROL_BYTE_END,
  };
  SendBytesRaw(data, 10);

  ReadBytes(4);

  const char expected[] = {BATTOR_CONTROL_MESSAGE_TYPE_RESET, 0x00, 0x00, 0x00};

  ASSERT_TRUE(IsReadComplete());
  ASSERT_TRUE(GetReadSuccess());
  ASSERT_EQ(BATTOR_MESSAGE_TYPE_CONTROL_ACK, GetReadType());
  ASSERT_EQ(0, std::memcmp(GetReadBytes()->data(), expected, 4));
}

TEST_F(BattOrConnectionTest, ReadControlMessage) {
  OpenConnection();
  ASSERT_TRUE(GetOpenSuccess());

  SendControlMessage(BATTOR_CONTROL_MESSAGE_TYPE_RESET, 4, 7);
  ReadBytes(sizeof(BattOrControlMessage));

  ASSERT_TRUE(IsReadComplete());
  ASSERT_TRUE(GetReadSuccess());
  ASSERT_EQ(BATTOR_MESSAGE_TYPE_CONTROL, GetReadType());

  BattOrControlMessage* msg =
      reinterpret_cast<BattOrControlMessage*>(GetReadBytes()->data());

  ASSERT_EQ(BATTOR_CONTROL_MESSAGE_TYPE_RESET, msg->type);
  ASSERT_EQ(4, msg->param1);
  ASSERT_EQ(7, msg->param2);
}

}  // namespace battor
