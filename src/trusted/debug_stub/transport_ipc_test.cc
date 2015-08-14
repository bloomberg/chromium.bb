/*
 * Copyright (c) 2015 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "gtest/gtest.h"

#include <errno.h>

#include "native_client/src/include/build_config.h"
#include "native_client/src/trusted/debug_stub/transport.h"

#if NACL_WINDOWS
#include <windows.h>
#else
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#endif

static const int kBufSize = 1024;
static const int kDisconnectFlag = -1;

class TransportIPCTests : public ::testing::Test {
 protected:
  port::ITransport *transport;
  NaClHandle fd[2];
  char buf[kBufSize];

  void SetUp() {
#if NACL_WINDOWS
    const char* PIPENAME = "\\\\.\\pipe\\TestTransportIPC";

    // Windows named pipes act more like a server-client than pipes or
    // socket pairs on POSIX, therefore the server end of the pipe needs to
    // call connect while creating the client end of the pipe.
    //
    // Since this is happening on a single thread we make the server pipe
    // operations asynchronous. The main order of operations to mimic a
    // socketpair is as so:
    // 1. Create server pipe
    // 2. Wait for connections async
    // 3. Create client end synchronously
    // 4. Wait on async server connection object
    //
    // Async is handled using overlapped objects which can be read about on:
    // msdn.microsoft.com/en-us/library/windows/desktop/ms686358(v=vs.85).aspx
    fd[0] = CreateNamedPipe(PIPENAME,
                            PIPE_ACCESS_DUPLEX |
                              FILE_FLAG_FIRST_PIPE_INSTANCE |
                              FILE_FLAG_OVERLAPPED,
                            PIPE_TYPE_BYTE | PIPE_READMODE_BYTE,
                            1,
                            kBufSize,
                            kBufSize,
                            5000,
                            NULL);

    EXPECT_NE(INVALID_HANDLE_VALUE, fd[0]);

    OVERLAPPED overlap;

    // If these aren't set to 0 windows will complain.
    overlap.Offset = 0;
    overlap.OffsetHigh = 0;

    // Create event used for async operations.
    overlap.hEvent = CreateEvent(NULL, TRUE, TRUE, NULL);
    EXPECT_NE(INVALID_HANDLE_VALUE, overlap.hEvent);

    // Run connect operation asynchronously.
    EXPECT_FALSE(ConnectNamedPipe(fd[0], &overlap));
    EXPECT_EQ(ERROR_IO_PENDING, GetLastError());

    // Connect file side of pipe.
    fd[1] = CreateFile(PIPENAME,
                       GENERIC_READ | GENERIC_WRITE,
                       0,
                       NULL,
                       OPEN_EXISTING,
                       SECURITY_SQOS_PRESENT | SECURITY_ANONYMOUS,
                       NULL);

    EXPECT_NE(INVALID_HANDLE_VALUE, fd[1]);

    DWORD bytes;
    // Wait for the connect operation to complete. Should already be done
    // given the above call to CreateFile().
    EXPECT_TRUE(GetOverlappedResult(fd[0], &overlap, &bytes, TRUE));

    CloseHandle(overlap.hEvent);
#else
    EXPECT_EQ(0, socketpair(AF_UNIX, SOCK_STREAM, 0, fd));
#endif
    transport = port::CreateTransportIPC(fd[1]);
    memset(buf, 0, kBufSize);
  }

  void TearDown() {
#if NACL_WINDOWS
    CloseHandle(fd[0]);
#else
    close(fd[0]);
#endif
    delete transport;
  }
};

static const int packet[9] = {
  32,  // Payload size is 32 bytes.
  0, 1, 2, 3, 4, 5, 6, 7
};

static const int packet2[2] = {
  4,  // Payload size is 4 bytes.
  8
};

static const int out_packet[4] = {
  0, 1, 2, 3
};

// Block until you read len bytes.
// Return false on EOF or error, but retries on EINTR.
bool ReadNBytes(NaClHandle handle, void *ptr, uint32_t len) {
  char *buf = reinterpret_cast<char *>(ptr);
  uint32_t bytes_read = 0;
  while (len > 0) {
    int result;

#if NACL_WINDOWS
    DWORD num_bytes;
    OVERLAPPED overlap;

    // If these aren't set to 0 windows will complain.
    overlap.Offset = 0;
    overlap.OffsetHigh = 0;

    overlap.hEvent = CreateEvent(NULL, TRUE, TRUE, NULL);

    if (!ReadFile(handle, buf, len, &num_bytes, &overlap)) {
      EXPECT_EQ(ERROR_IO_PENDING, GetLastError());
      if (!GetOverlappedResult(handle,
                               &overlap,
                               &num_bytes,
                               TRUE)) {
        return false;
      }
    }

    result = static_cast<int>(num_bytes);

    CloseHandle(overlap.hEvent);
#else
    result = ::read(handle, buf + bytes_read, len);
    if (result == 0 || (result == -1 && errno != EINTR))
      return false;
#endif

    bytes_read += result;
    len -= result;
  }
  return true;
}

// Keep trying until you write len bytes.
// Return false on EOF or error, but retries on EINTR.
bool WriteNBytes(NaClHandle handle, const void *ptr, uint32_t len) {
  const char *buf = reinterpret_cast<const char *>(ptr);
  uint32_t bytes_written = 0;
  while (len > 0) {
    int result;

#if NACL_WINDOWS
    DWORD num_bytes;
    OVERLAPPED overlap;

    // If these aren't set to 0 windows will complain.
    overlap.Offset = 0;
    overlap.OffsetHigh = 0;

    overlap.hEvent = CreateEvent(NULL, TRUE, TRUE, NULL);

    if (!WriteFile(handle, buf, len, &num_bytes, &overlap)) {
      EXPECT_EQ(ERROR_IO_PENDING, GetLastError());
      if (!GetOverlappedResult(handle,
                               &overlap,
                               &num_bytes,
                               TRUE)) {
        return false;
      }
    }

    result = static_cast<int>(num_bytes);

    CloseHandle(overlap.hEvent);
#else
    result = ::write(handle, buf + bytes_written, len);
    if (result == 0 || (result == -1 && errno != EINTR))
      return false;
#endif

    bytes_written += result;
    len -= result;
  }
  return true;
}

// Test accepting multiple connections in sequence.
TEST_F(TransportIPCTests, TestAcceptConnection) {
  // Write data so AcceptConnection() wont block.
  EXPECT_TRUE(WriteNBytes(fd[0], packet, sizeof(packet)));

  // Since there is data this should return true without blocking.
  EXPECT_TRUE(transport->AcceptConnection());

  // Write -1 so transport marks itself as disconnected.
  EXPECT_TRUE(WriteNBytes(fd[0], &kDisconnectFlag, sizeof(kDisconnectFlag)));

  // Read data sent and -1 flag.
  EXPECT_FALSE(transport->Read(buf, kBufSize));

  // Try to establish new connection.
  EXPECT_TRUE(WriteNBytes(fd[0], packet, sizeof(packet)));
  EXPECT_TRUE(transport->AcceptConnection());
}

// Test reading multiple buffered packets at once.
TEST_F(TransportIPCTests, TestReadMultiplePacketsAtOnce) {
  // Write initial data and accept connection.
  EXPECT_TRUE(WriteNBytes(fd[0], packet, sizeof(packet)));
  EXPECT_TRUE(transport->AcceptConnection());

  // Write a second packet.
  EXPECT_TRUE(WriteNBytes(fd[0], packet2, sizeof(packet2)));

  // Read both packets.
  EXPECT_TRUE(transport->Read(buf, 9 * sizeof(int)));

  // Check if packets were both read properly.
  for (int i = 0; i < 9; i++) {
    EXPECT_EQ(i, reinterpret_cast<int*>(buf)[i]);
  }
}

// Test reading single packet over multiple calls to read.
TEST_F(TransportIPCTests, TestReadSinglePacket) {
  // Write initial data and accept connection.
  EXPECT_TRUE(WriteNBytes(fd[0], packet, sizeof(packet)));
  EXPECT_TRUE(transport->AcceptConnection());

  // Read first half of the packet.
  EXPECT_TRUE(transport->Read(buf, 4 * sizeof(int)));
  // Read second half of the packet.
  EXPECT_TRUE(transport->Read(buf + 16, 4 * sizeof(int)));

  // Check if entire packets were both read properly.
  for (int i = 0; i < 8; i++) {
    EXPECT_EQ(i, reinterpret_cast<int*>(buf)[i]);
  }
}

// Test IsDataAvailable()
TEST_F(TransportIPCTests, TestIsDataAvailable) {
  // Write initial data and accept connection.
  EXPECT_TRUE(WriteNBytes(fd[0], packet, sizeof(packet)));
  EXPECT_TRUE(transport->AcceptConnection());

  // Should be data available from initial write.
  EXPECT_TRUE(transport->IsDataAvailable());

  // Read some of the data
  EXPECT_TRUE(transport->Read(buf, 4 * sizeof(int)));

  // Should still be data available from initial write.
  EXPECT_TRUE(transport->IsDataAvailable());

  // Read the rest of the data
  EXPECT_TRUE(transport->Read(buf, 4 * sizeof(int)));

  // No more data.
  EXPECT_FALSE(transport->IsDataAvailable());
}

// Test writing data.
TEST_F(TransportIPCTests, TestWrite) {
  // Write initial data and accept connection.
  EXPECT_TRUE(WriteNBytes(fd[0], packet, sizeof(packet)));
  EXPECT_TRUE(transport->AcceptConnection());

  // Write packet out.
  EXPECT_TRUE(transport->Write(out_packet, sizeof(out_packet)));

  EXPECT_TRUE(ReadNBytes(fd[0], buf, sizeof(out_packet)));

  // Check if out packet was written properly.
  for (int i = 0; i < 4; i++) {
    EXPECT_EQ(i, reinterpret_cast<int*>(buf)[i]);
  }
}

// Test disconnect.
TEST_F(TransportIPCTests, TestDisconnect) {
  // Write initial data and accept connection.
  EXPECT_TRUE(WriteNBytes(fd[0], packet, sizeof(packet)));
  EXPECT_TRUE(transport->AcceptConnection());

  // Write -1 so transport can disconnect.
  EXPECT_TRUE(WriteNBytes(fd[0], &kDisconnectFlag, sizeof(kDisconnectFlag)));

  // Disconnect and throw away the unread packet.
  transport->Disconnect();

  // Write data so AcceptConnection() wont block.
  EXPECT_TRUE(WriteNBytes(fd[0], packet, sizeof(packet)));
  EXPECT_TRUE(transport->AcceptConnection());

  // Read packet.
  EXPECT_TRUE(transport->Read(buf, sizeof(packet) - 4));

  // Second packet should have been thrown away.
  EXPECT_FALSE(transport->IsDataAvailable());
}

int main(int argc, char *argv[]) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
