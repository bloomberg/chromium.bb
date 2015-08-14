/*
 * Copyright (c) 2015 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <string>

#include "native_client/src/include/build_config.h"
#include "native_client/src/include/nacl_scoped_ptr.h"
#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/trusted/debug_stub/transport.h"
#include "native_client/src/trusted/service_runtime/sel_ldr.h"

#if NACL_WINDOWS
#include <windows.h>
#endif

namespace port {

// This transport is to handle connections over ipc to another
// process. Instead of directly talking over a server socket,
// the server socket is listening on the other process and
// ferries the data between the socket and the given pipe.
//
// Since we also need to handle multiple sequential connections
// but cannot close our handle the incoming data is encoded.
// The encoding is simple and just prepends a 4 byte length
// to each message. A length of -1 signifies that the server
// socket was disconnected and is awaiting a new connection.
// Once in a disconnected state any new incoming data will
// symbolize a new connection.
class TransportIPC : public ITransport {
 public:
  TransportIPC()
    : buf_(new char[kBufSize]),
      unconsumed_bytes_(0),
      bytes_to_read_(kServerSocketDisconnect),
      handle_(NACL_INVALID_HANDLE) { }

  explicit TransportIPC(NaClHandle fd)
    : buf_(new char[kBufSize]),
      unconsumed_bytes_(0),
      bytes_to_read_(kServerSocketDisconnect),
      handle_(fd) { }

  ~TransportIPC() {
    if (handle_ != NACL_INVALID_HANDLE) {
#if NACL_WINDOWS
      if (!CloseHandle(handle_))
#else
      if (::close(handle_))
#endif
        NaClLog(LOG_FATAL,
                "TransportIPC::Disconnect: Failed to close handle.\n");
    }
  }

  // Read from this transport, return true on success.
  // Returning false means we have disconnected on the server end.
  virtual bool Read(void *ptr, int32_t len) {
    CHECK(IsConnected());
    CHECK(ptr && len >= 0);
    char *dst = static_cast<char *>(ptr);

    while (len > 0) {
      if (!FillBufferIfEmpty()) return false;
      CopyFromBuffer(&dst, &len);
    }
    return true;
  }

  // Write to this transport, return true on success.
  virtual bool Write(const void *ptr, int32_t len) {
    CHECK(IsConnected());
    CHECK(ptr && len >= 0);
    const char *src = static_cast<const char *>(ptr);
    while (len > 0) {
      int result = WriteInternal(handle_, src, len);
      if (result >= 0) {
        src += result;
        len -= result;
      } else {
        NaClLog(LOG_FATAL,
                "TransportIPC::Write: Pipe closed from other process.\n");
      }
    }
    return true;
  }

  // Return true if there is data to read.
  virtual bool IsDataAvailable() {
    CHECK(IsConnected());
    if (unconsumed_bytes_ > 0) return true;

#if NACL_WINDOWS
  DWORD available_bytes = 0;
  // Return true if the pipe has data or there is an error.
  if (PeekNamedPipe(handle_, NULL, 0, NULL, &available_bytes, NULL))
    return available_bytes > 0;
  else
    return true;
#else
    fd_set fds;

    FD_ZERO(&fds);
    FD_SET(handle_, &fds);

    // We want a "non-blocking" check
    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;

    // Check if this file handle can select on read
    int cnt = select(handle_ + 1, &fds, 0, 0, &timeout);

    // If we are ready, or if there is an error.  We return true
    // on error, to let the next IO request fail.
    if (cnt != 0) return true;

    return false;
#endif
  }

  void WaitForDebugStubEvent(struct NaClApp *nap,
                             bool ignore_input_from_gdb) {
    bool wait = true;
    // If we are told to ignore messages from gdb, we will exit from this
    // function only if new data is sent by gdb.
    if ((unconsumed_bytes_ > 0 && !ignore_input_from_gdb) ||
        nap->faulted_thread_count > 0) {
      // Clear faulted thread events to save debug stub loop iterations.
      wait = false;
    }

#if NACL_WINDOWS
  HANDLE handles[2];
  handles[0] = nap->faulted_thread_event;
  handles[1] = handle_;
  int result = WaitForMultipleObjects(2, handles, FALSE,
                                      wait ? INFINITE : 0);
  if (result == WAIT_OBJECT_0 ||
      result == WAIT_OBJECT_0 + 1 ||
      result == WAIT_TIMEOUT)
    return;
  NaClLog(LOG_FATAL,
          "TransportIPC::WaitForDebugStubEvent: Wait for events failed.\n");
#else
    fd_set fds;

    FD_ZERO(&fds);
    FD_SET(nap->faulted_thread_fd_read, &fds);
    int max_fd = nap->faulted_thread_fd_read;
    if (unconsumed_bytes_ < kBufSize) {
      FD_SET(handle_, &fds);
      max_fd = std::max(max_fd, handle_);
    }

    int ret;
    // We don't need sleep-polling on Linux now,
    // so we set either zero or infinite timeout.
    if (wait) {
      ret = select(max_fd + 1, &fds, NULL, NULL, NULL);
    } else {
      struct timeval timeout;
      timeout.tv_sec = 0;
      timeout.tv_usec = 0;
      ret = select(max_fd + 1, &fds, NULL, NULL, &timeout);
    }
    if (ret < 0) {
      NaClLog(LOG_FATAL,
              "TransportIPC::WaitForDebugStubEvent: Wait for events failed.\n");
    }

    if (ret > 0) {
      if (FD_ISSET(nap->faulted_thread_fd_read, &fds)) {
        char buf[16];
        if (read(nap->faulted_thread_fd_read, &buf, sizeof(buf)) < 0) {
          NaClLog(LOG_FATAL,
                  "TransportIPC::WaitForDebugStubEvent: Failed to read from "
                  "debug stub event pipe fd.\n");
        }
      }
      if (FD_ISSET(handle_, &fds))
        FillBufferIfEmpty();
    }
#endif
  }

  virtual void Disconnect() {
    // If we are being marked as disconnected then we should also
    // receive the disconnect marker so the next connection is in
    // a proper state.
    if (IsConnected()) {
      do {
        unconsumed_bytes_ = 0;
        // FillBufferIfEmpty() returns false on disconnect or a
        // Fatal error, and in both cases we want to exit the loop.
      } while (FillBufferIfEmpty());
    }

    // Throw away unused data.
    unconsumed_bytes_ = 0;
  }

  // Block until we have new data on the pipe, new data means the
  // server socket across the pipe got a new connection.
  virtual bool AcceptConnection() {
    CHECK(!IsConnected());
#if NACL_WINDOWS
  if (WaitForSingleObject(handle_, INFINITE) == WAIT_OBJECT_0) {
    // This marks ourself as connected.
    bytes_to_read_ = 0;
    return true;
  }
  NaClLog(LOG_FATAL,
          "TransportIPC::WaitForDebugStubEvent: Wait for events failed.\n");
  return false;
#else
    fd_set fds;

    FD_ZERO(&fds);
    FD_SET(handle_, &fds);

    // Check if this file handle can select on read
    int cnt = select(handle_ + 1, &fds, 0, 0, NULL);

    // If we are ready, or if there is an error.  We return true
    // on error, to let the next IO request fail.
    if (cnt != 0) {
      // This marks ourself as connected.
      bytes_to_read_ = 0;
      return true;
    }

    return false;
#endif
  }

 private:
  // Returns whether we are in a connected state. This refers to the
  // connection of the server across the pipe, if the pipe itself is
  // ever disconnected we will probably be crashing or exitting soon.
  bool IsConnected() {
    return bytes_to_read_ != kServerSocketDisconnect;
  }

  // Copy buffered data to *dst up to len bytes and update dst and len.
  void CopyFromBuffer(char **dst, int32_t *len) {
    int32_t copy_bytes = std::min(*len, unconsumed_bytes_);
    memcpy(*dst, buf_.get(), copy_bytes);
    // Keep data aligned with start of buffer.
    memmove(buf_.get(), buf_.get() + copy_bytes,
            unconsumed_bytes_ - copy_bytes);
    unconsumed_bytes_ -= copy_bytes;
    *len -= copy_bytes;
    *dst += copy_bytes;
  }

  // If the buffer is empty it tries to read data and returns true if
  // successful, otherwise if the buffer has data it returns true immediately.
  // Returns false if we got the disconnect marker from the other end of the
  // pipe meaning there was a server disconnect on the other side and we need
  // to wait for a new connection. Also return false on a fatal error.
  bool FillBufferIfEmpty() {
    if (unconsumed_bytes_ > 0)
      return true;

    if (bytes_to_read_ == 0) {
      // If this read fails it means something went wrong on other side.
      if (!ReadNBytes(reinterpret_cast<char *>(&bytes_to_read_),
                      sizeof(bytes_to_read_))) {
        NaClLog(LOG_FATAL,
                "TransportIPC::FillBufferIfEmpty: "
                "Pipe closed from other process.\n");
        // We want to mark ourselves as disconnected on an error.
        bytes_to_read_ = kServerSocketDisconnect;
        return false;
      }

      // If we got the disconnect flag mark it as such.
      if (bytes_to_read_ == kServerSocketDisconnect)
        return false;
    }

    int result = ReadInternal(handle_, buf_.get() + unconsumed_bytes_,
                        std::min(bytes_to_read_, kBufSize));
    if (result >= 0) {
      unconsumed_bytes_ += result;
      bytes_to_read_ -= result;
    } else {
      NaClLog(LOG_FATAL,
              "TransportIPC::FillBufferIfEmpty: "
              "Pipe closed from other process.\n");
      // We want to mark ourselves as disconnected on an error.
      bytes_to_read_ = kServerSocketDisconnect;
      return false;
    }

    return true;
  }

  // Block until you read len bytes. Return false on error.
  bool ReadNBytes(char *buf, uint32_t len) {
    uint32_t bytes_read = 0;
    while (len > 0) {
      int result = ReadInternal(handle_, buf + bytes_read, len);
      if (result >= 0) {
        bytes_read += result;
        len -= result;
      } else {
        return false;
      }
    }
    return true;
  }

  // Platform independant read. Returns number of bytes read or
  // -1 on eof or error. Eof is treated as an error since the pipe
  // should never be closed on the other end. On posix EINTR will
  // return 0.
  static int ReadInternal(NaClHandle handle, char *buf, int len) {
#if NACL_WINDOWS
    DWORD bytes_read = 0;

    if (!ReadFile(handle, buf, len, &bytes_read, NULL))
      return -1;
    else
      return static_cast<int>(bytes_read);
#else
    int result = ::read(handle, buf, len);
    if (result > 0)
      return result;
    else if (result == 0 || errno != EINTR)
      return -1;
    else
      return 0;
#endif
  }

  // Platform independant write. Returns number of bytes read or
  // -1 on eof or error. Eof is treated as an error since the pipe
  // should never be closed on the other end. On posix EINTR will
  // return 0.
  static int WriteInternal(NaClHandle handle, const char *buf, int len) {
#if NACL_WINDOWS
    DWORD bytes_written = 0;

    if (!WriteFile(handle, buf, len, &bytes_written, NULL))
      return -1;
    else
      return static_cast<int>(bytes_written);
#else
    int result = ::write(handle, buf, len);
    if (result > 0)
      return result;
    else if (result == 0 || errno != EINTR)
      return -1;
    else
      return 0;
#endif
  }

  static const int kServerSocketDisconnect = -1;
  static const int kBufSize = 4096;
  nacl::scoped_array<char> buf_;

  // Number of bytes stored in internal buffer.
  int32_t unconsumed_bytes_;

  // Number of bytes left in packet encoding from browser.
  int32_t bytes_to_read_;
  NaClHandle handle_;
};

// The constant is passed by reference in some cases. So
// under some optmizations or lack thereof it needs space.
const int TransportIPC::kBufSize;

ITransport *CreateTransportIPC(NaClHandle fd) {
  return new TransportIPC(fd);
}

}  // namespace port
