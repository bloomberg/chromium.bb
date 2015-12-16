// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_BATTOR_AGENT_BATTOR_CONNECTION_H_
#define TOOLS_BATTOR_AGENT_BATTOR_CONNECTION_H_

#include <vector>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "device/serial/serial.mojom.h"
#include "tools/battor_agent/battor_error.h"
#include "tools/battor_agent/battor_protocol_types.h"

namespace device {
class SerialIoHandler;
}
namespace net {
class IOBuffer;
}

namespace battor {

// A BattOrConnection is a wrapper around the serial connection to the BattOr
// that handles conversion of a message to and from the byte-level BattOr
// protocol.
//
// At a high-level, all BattOr messages consist of:
//
//   0x00               (1 byte start marker)
//   uint8_t            (1 byte header indicating the message type)
//   data               (message data, with 0x00s and 0x01s escaped with 0x02)
//   0x01               (1 byte end marker)
//
// For a more in-depth description of the protocol, see http://bit.ly/1NvNVc3.
class BattOrConnection : public base::SupportsWeakPtr<BattOrConnection> {
 public:
  // The listener interface that must be implemented in order to interact with
  // the BattOrConnection.
  class Listener {
   public:
    virtual void OnConnectionOpened(bool success) = 0;
    virtual void OnBytesSent(bool success) = 0;
    virtual void OnBytesRead(bool success,
                             BattOrMessageType type,
                             scoped_ptr<std::vector<char>> bytes) = 0;
  };

  // Constructs a new BattOrConnection. We use a WeakPtr for the listener
  // because it's possible for the SerialIoHandler's connect, read, or write to
  // complete after we've been destroyed, in which case we just want to cancel
  BattOrConnection(
      const std::string& path,
      Listener* listener,
      scoped_refptr<base::SingleThreadTaskRunner> file_thread_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> ui_thread_task_runner);
  virtual ~BattOrConnection();

  // Initializes the serial connection and calls the listener's
  // OnConnectionOpened() when complete. This function must be called before
  // using the BattOrConnection.
  void Open();
  bool IsOpen();
  // Closes the serial connection and releases any handles being held.
  void Close();

  // Sends the specified buffer over the serial connection and calls the
  // listener's OnBytesSent() when complete. Note that bytes_to_send should not
  // include the start, end, type, or escape bytes required by the BattOr
  // protocol.
  void SendBytes(BattOrMessageType type,
                 const void* buffer,
                 size_t bytes_to_send);

  // Reads the specified number of bytes from the serial connection and calls
  // the listener's OnBytesRead() when complete. Note that the number of bytes
  // requested should not include the start, end, or type bytes required by the
  // BattOr protocol, and that this method may issue multiple read read requests
  // if the message contains escape characters.
  void ReadBytes(size_t bytes_to_read);

  // Flushes the serial connection to the BattOr.
  void Flush();

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

  // The listener receiving the results of the commands being executed.
  Listener* listener_;

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

  DISALLOW_COPY_AND_ASSIGN(BattOrConnection);
};

}  // namespace battor

#endif  // TOOLS_BATTOR_AGENT_BATTOR_CONNECTION_H_
