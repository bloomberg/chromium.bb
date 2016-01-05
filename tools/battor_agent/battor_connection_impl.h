// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_BATTOR_AGENT_BATTOR_CONNECTION_IMPL_H_
#define TOOLS_BATTOR_AGENT_BATTOR_CONNECTION_IMPL_H_

#include <vector>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "device/serial/serial.mojom.h"
#include "tools/battor_agent/battor_connection.h"
#include "tools/battor_agent/battor_error.h"
#include "tools/battor_agent/battor_protocol_types.h"

namespace device {
class SerialIoHandler;
}
namespace net {
class IOBuffer;
}

namespace battor {

// A BattOrConnectionImpl is a concrete implementation of a BattOrConnection.
class BattOrConnectionImpl
    : public BattOrConnection,
      public base::SupportsWeakPtr<BattOrConnectionImpl> {
 public:
  BattOrConnectionImpl(
      const std::string& path,
      BattOrConnection::Listener* listener,
      scoped_refptr<base::SingleThreadTaskRunner> file_thread_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> ui_thread_task_runner);
  ~BattOrConnectionImpl() override;

  void Open() override;
  void Close() override;
  void SendBytes(BattOrMessageType type,
                 const void* buffer,
                 size_t bytes_to_send) override;
  void ReadBytes(size_t bytes_to_read) override;
  void Flush() override;

 protected:
  // Overridden by the test to use a fake serial connection.
  virtual scoped_refptr<device::SerialIoHandler> CreateIoHandler();

  // IO handler capable of reading and writing from the serial connection.
  scoped_refptr<device::SerialIoHandler> io_handler_;

 private:
  void OnOpened(bool success);

  // Reads the specified number of additional bytes and adds them to the pending
  // read buffer.
  void ReadMoreBytes(size_t bytes_to_read);

  // Internal callback for when bytes are read. This method may trigger
  // additional reads if any newly read bytes are escape bytes.
  void OnBytesRead(int bytes_read, device::serial::ReceiveError error);

  // Internal callback for when bytes are sent.
  void OnBytesSent(int bytes_sent, device::serial::SendError error);

  // The path of the BattOr.
  std::string path_;

  // All bytes that have been read since the user requested a read. If multiple
  // reads are required due to the presence of escape bytes,
  // pending_read_buffer_ grows with each read.
  scoped_ptr<std::vector<char>> pending_read_buffer_;
  // The bytes that were read in just the last read. If multiple reads are
  // required due to the presence of escape bytes, last_read_buffer_ only
  // contains the results of the last read.
  scoped_refptr<net::IOBuffer> last_read_buffer_;
  // The number of bytes that we requested in the last read.
  size_t pending_read_length_;
  // The number of escape bytes that have already been read.
  size_t pending_read_escape_byte_count_;

  // The total number of bytes that we're expecting to send.
  size_t pending_write_length_;

  // Threads needed for serial communication.
  scoped_refptr<base::SingleThreadTaskRunner> file_thread_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> ui_thread_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(BattOrConnectionImpl);
};

}  // namespace battor

#endif  // TOOLS_BATTOR_AGENT_BATTOR_CONNECTION_IMPL_H_
