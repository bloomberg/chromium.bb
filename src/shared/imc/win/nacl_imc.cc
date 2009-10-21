/*
 * Copyright 2008, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


// NaCl inter-module communication primitives.

#ifndef NOMINMAX
#define NOMINMAX  // Disables the generation of the min and max macro in
                  // <windows.h>.
#endif

#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <windows.h>
#include <sys/types.h>
#include <algorithm>
#include "native_client/src/include/atomic_ops.h"
#include "native_client/src/shared/imc/nacl_imc.h"
#include "native_client/src/trusted/handle_pass/handle_lookup.h"

namespace nacl {

namespace {
// This prefix used to be appended to pipe names for pipes
// created in BoundSocket. We keep it for backward compatibility.
// TODO(gregoryd): implement versioning support
const char kOldPipePrefix[] = "\\\\.\\pipe\\google-nacl-";
const char kPipePrefix[] = "\\\\.\\pipe\\chrome.nacl.";

const size_t kPipePrefixSize = sizeof kPipePrefix / sizeof kPipePrefix[0];
const size_t kOldPipePrefixSize =
    sizeof kOldPipePrefix / sizeof kOldPipePrefix[0];

const int kPipePathMax = kPipePrefixSize + kPathMax + 1;
const int kOutBufferSize = 4096;  // TBD
const int kInBufferSize = 4096;   // TBD
const int kDefaultTimeoutMilliSeconds = 1000;

// ControlHeader::command
const int kEchoRequest = 0;
const int kEchoResponse = 1;
const int kMessage = 2;
const int kCancel = 3;   // Cancels Handle transfer operations

struct ControlHeader {
  int command;
  DWORD pid;
  size_t message_length;
  size_t handle_count;
};

bool GetSocketName(const SocketAddress* address, char* name) {
  if (address == NULL || !isprint(address->path[0])) {
    SetLastError(ERROR_INVALID_PARAMETER);
    return false;
  }
  sprintf_s(name, kPipePathMax, "%s%.*s",
    kPipePrefix, kPathMax, address->path);
  return true;
}

bool GetSocketNameWithOldPrefix(const SocketAddress* address, char* name) {
  if (address == NULL || !isprint(address->path[0])) {
    SetLastError(ERROR_INVALID_PARAMETER);
    return false;
  }
  sprintf_s(name, kPipePathMax, "%s%.*s",
            kOldPipePrefix, kPathMax, address->path);
  return true;
}

int ReadAll(HANDLE handle, void* buffer, size_t length) {
  size_t count = 0;
  while (count < length) {
    DWORD len;
    DWORD chunk = static_cast<DWORD>(
      ((length - count) <= UINT_MAX) ? (length - count) : UINT_MAX);
    if (ReadFile(handle, static_cast<char*>(buffer) + count,
                 chunk, &len, NULL) == FALSE) {
      return static_cast<int>((0 < count) ? count : -1);
    }
    count += len;
  }
  return static_cast<int>(count);
};

int WriteAll(HANDLE handle, const void* buffer, size_t length) {
  size_t count = 0;
  while (count < length) {
    DWORD len;
    // The following statement is for the 64 bit portability.
    DWORD chunk = static_cast<DWORD>(
      ((length - count) <= UINT_MAX) ? (length - count) : UINT_MAX);
    if (WriteFile(handle, static_cast<const char*>(buffer) + count,
                  chunk, &len, NULL) == FALSE) {
      return static_cast<int>((0 < count) ? count : -1);
    }
    count += len;
  }
  return static_cast<int>(count);
};

BOOL SkipFile(HANDLE handle, size_t length) {
  while (0 < length) {
    char scratch[1024];
    size_t count = std::min(sizeof scratch, length);
    if (ReadAll(handle, scratch, count) != count) {
      return FALSE;
    }
    length -= count;
  }
  return TRUE;
}

BOOL SkipHandles(HANDLE handle, size_t count) {
  while (0 < count) {
    Handle discard;
    if (ReadAll(handle, &discard, sizeof discard) != sizeof discard) {
      return FALSE;
    }
    CloseHandle(discard);
    --count;
  }
  return TRUE;
}

}  // namespace

bool WouldBlock() {
  return (GetLastError() == ERROR_PIPE_LISTENING) ? true : false;
}

int GetLastErrorString(char* buffer, size_t length) {
  DWORD error = GetLastError();
  return FormatMessageA(
      FORMAT_MESSAGE_FROM_SYSTEM |
      FORMAT_MESSAGE_IGNORE_INSERTS,
      NULL,
      error,
      MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
      buffer,
      static_cast<DWORD>((64 * 1024 < length) ? 64 * 1024 : length),
      NULL) ? 0 : -1;
}

Handle BoundSocket(const SocketAddress* address) {
  char name[kPipePathMax];
  if (!GetSocketName(address, name)) {
    return kInvalidHandle;
  }
  // Create a named pipe in nonblocking mode.
  return CreateNamedPipeA(
      name,
      PIPE_ACCESS_DUPLEX | FILE_FLAG_FIRST_PIPE_INSTANCE,
      PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_NOWAIT,
      PIPE_UNLIMITED_INSTANCES,
      kOutBufferSize,
      kInBufferSize,
      kDefaultTimeoutMilliSeconds,
      NULL);
}

int SocketPair(Handle pair[2]) {
  static AtomicWord socket_pair_count;

  char name[kPipePathMax];
  do {
    sprintf_s(name, kPipePathMax, "%s%u.%lu",
              kPipePrefix, GetCurrentProcessId(),
              AtomicIncrement(&socket_pair_count, 1));
    pair[0] = CreateNamedPipeA(
        name,
        PIPE_ACCESS_DUPLEX | FILE_FLAG_FIRST_PIPE_INSTANCE,
        PIPE_TYPE_BYTE | PIPE_READMODE_BYTE,
        1,
        kOutBufferSize,
        kInBufferSize,
        kDefaultTimeoutMilliSeconds,
        NULL);
    if (pair[0] == INVALID_HANDLE_VALUE &&
        GetLastError() != ERROR_ACCESS_DENIED &&
        GetLastError() != ERROR_PIPE_BUSY) {
      return -1;
    }
  } while (pair[0] == INVALID_HANDLE_VALUE);
  pair[1] = CreateFileA(name,
                        GENERIC_READ | GENERIC_WRITE,
                        0,              // no sharing
                        NULL,           // default security attributes
                        OPEN_EXISTING,  // opens existing pipe
                        0,              // default attributes
                        NULL);          // no template file
  if (pair[1] == INVALID_HANDLE_VALUE) {
    CloseHandle(pair[0]);
    return -1;
  }
  if (ConnectNamedPipe(pair[0], NULL) == FALSE) {
    DWORD error = GetLastError();
    if (error != ERROR_PIPE_CONNECTED) {
      CloseHandle(pair[0]);
      CloseHandle(pair[1]);
      return -1;
    }
  }
  return 0;
}

int Close(Handle handle) {
  if (handle == NULL || handle == INVALID_HANDLE_VALUE) {
    return 0;
  }
  return CloseHandle(handle) ? 0 : -1;
}

int SendDatagram(Handle handle, const MessageHeader* message, int flags) {
  ControlHeader header = { kEchoRequest, GetCurrentProcessId(), 0, 0 };
  HANDLE remote_handles[kHandleCountMax];

  if (kHandleCountMax < message->handle_count) {
    SetLastError(ERROR_INVALID_PARAMETER);
    return -1;
  }
  if (0 < message->handle_count && message->handles) {
    // TODO(shiki): On Windows Vista, we can use GetNamedPipeClientProcessId()
    // and GetNamedPipeServerProcessId() and probably we can remove
    // kEchoRequest and kEchoResponse completely.
    if (WriteAll(handle, &header, sizeof header) != sizeof header ||
        ReadAll(handle, &header, sizeof header) != sizeof header ||
        header.command != kEchoResponse) {
      return -1;
    }
#ifdef NACL_STANDALONE  // not in Chrome
    HANDLE target = OpenProcess(PROCESS_DUP_HANDLE, FALSE, header.pid);
#else
    HANDLE target = NaClHandlePassLookupHandle(header.pid);
#endif
    if (target == NULL) {
      return -1;
    }
    for (size_t i = 0; i < message->handle_count; ++i) {
      if (DuplicateHandle(GetCurrentProcess(), message->handles[i],
                          target, &remote_handles[i],
                          0, FALSE, DUPLICATE_SAME_ACCESS) == FALSE) {
        // Send the kCancel message to revoke the handles duplicated
        // so far in the remote peer.
        header.command = kCancel;
        header.handle_count = i;
        if (0 < i) {
          WriteAll(handle, &header, sizeof header);
          WriteAll(handle, remote_handles, sizeof(Handle) * i);
        }
#ifdef NACL_STANDALONE
        CloseHandle(target);
#endif
        return -1;
      }
    }
#ifdef NACL_STANDALONE
    CloseHandle(target);
#endif
  }
  header.command = kMessage;
  header.handle_count = message->handle_count;
  for (size_t i = 0; i < message->iov_length; ++i) {
    header.message_length += message->iov[i].length;
  }
  if (WriteAll(handle, &header, sizeof header) != sizeof header) {
    return -1;
  }
  for (size_t i = 0; i < message->iov_length; ++i) {
    if (WriteAll(handle, message->iov[i].base, message->iov[i].length) !=
        message->iov[i].length) {
      return -1;
    }
  }
  if (0 < message->handle_count && message->handles &&
      WriteAll(handle,
               remote_handles,
               sizeof(Handle) * message->handle_count) !=
          sizeof(Handle) * message->handle_count) {
    return -1;
  }
  return static_cast<int>(header.message_length);
}

int SendDatagramTo(Handle handle, const MessageHeader* message, int flags,
                   const SocketAddress* name) {
  if (kHandleCountMax < message->handle_count) {
    SetLastError(ERROR_INVALID_PARAMETER);
    return -1;
  }
  char pipe_name[kPipePathMax];
  if (!GetSocketName(name, pipe_name)) {
    return -1;
  }
  for (;;) {
    handle = CreateFileA(pipe_name,
                         GENERIC_READ | GENERIC_WRITE,
                         0,              // no sharing
                         NULL,           // default security attributes
                         OPEN_EXISTING,  // opens existing pipe
                         0,              // default attributes
                         NULL);          // no template file
    if (handle != INVALID_HANDLE_VALUE) {
      break;
    }

    // If the pipe is busy it means it exists, so we can try and wait
    if (GetLastError() != ERROR_PIPE_BUSY) {
      if (GetLastError() != ERROR_FILE_NOT_FOUND) {
        return -1;
      } else {
        // Try to find the file using name with prefix (can be created
        // by an old version of IMC library.
        if (!GetSocketNameWithOldPrefix(name, pipe_name)) {
          return -1;
        }
        handle = CreateFileA(pipe_name,
                             GENERIC_READ | GENERIC_WRITE,
                             0,              // no sharing
                             NULL,           // default security attributes
                             OPEN_EXISTING,  // opens existing pipe
                             0,              // default attributes
                             NULL);          // no template file
        if (handle != INVALID_HANDLE_VALUE) {
          break;
        }
        if (GetLastError() != ERROR_PIPE_BUSY) {
          // Could not find the pipe - nothing to do
          return -1;
        }
      }
      break;
    }
    if (flags & kDontWait) {
      SetLastError(ERROR_PIPE_LISTENING);
      return -1;
    }
    if (!WaitNamedPipeA(pipe_name, NMPWAIT_WAIT_FOREVER)) {
      return -1;
    }
  }
  int result = SendDatagram(handle, message, flags);
  CloseHandle(handle);
  return result;
}

namespace {

int ReceiveDatagram(Handle handle, MessageHeader* message, int flags,
                    bool bound_socket) {
  ControlHeader header;
  int result = -1;
  bool dontPeek = false;
 Repeat:
  if ((flags & kDontWait) && !dontPeek) {
    DWORD len;
    DWORD total;
    if (PeekNamedPipe(handle, &header, sizeof header, &len, &total, NULL)) {
      if (len < sizeof header) {
        SetLastError(ERROR_PIPE_LISTENING);
      } else {
        switch (header.command) {
        case kEchoRequest:
          // Send back the process id to the remote peer to duplicate handles.
          // TODO(shiki) : It might be better to keep remote pid by the initial
          //               handshake rather than send kEchoRequest each time
          //               before duplicating handles.
          if (ReadAll(handle, &header, sizeof header) == sizeof header) {
            header.command = kEchoResponse;
            header.pid = GetCurrentProcessId();
            WriteAll(handle, &header, sizeof header);
            if (bound_socket) {
              // We must not close this connection.
              dontPeek = true;
            }
            goto Repeat;
          }
          break;
        case kEchoResponse:
          SkipFile(handle, sizeof header);
          goto Repeat;
          break;
        case kMessage:
          if (header.message_length + sizeof header <= total) {
            if (flags & kDontWait) {
              flags &= ~kDontWait;
              goto Repeat;
            }
            result = static_cast<int>(header.message_length);
          } else {
            SetLastError(ERROR_PIPE_LISTENING);
          }
          break;
        case kCancel:
          if (sizeof header + sizeof(Handle) * header.handle_count <= len &&
              ReadAll(handle, &header, sizeof header) == sizeof header) {
            SkipHandles(handle, header.handle_count);
            goto Repeat;
          }
          break;
        default:
          break;
        }
      }
    }
  } else if (ReadAll(handle, &header, sizeof header) == sizeof header) {
    dontPeek = false;
    switch (header.command) {
    case kEchoRequest:
      header.command = kEchoResponse;
      header.pid = GetCurrentProcessId();
      WriteAll(handle, &header, sizeof header);
      goto Repeat;
      break;
    case kEchoResponse:
      goto Repeat;
      break;
    case kMessage: {
      size_t total_message_bytes = header.message_length;
      size_t count = 0;
      message->flags = 0;
      for (size_t i = 0;
           i < message->iov_length && count < header.message_length;
           ++i) {
        IOVec* iov = &message->iov[i];
        size_t len = std::min(iov->length, total_message_bytes);
        if (ReadAll(handle, iov->base, len) != len) {
          break;
        }
        total_message_bytes -= len;
        count += len;
      }
      if (count < header.message_length) {
        if (SkipFile(handle, header.message_length - count) == FALSE) {
          break;
        }
        message->flags |= kMessageTruncated;
      }
      if (0 < message->handle_count && message->handles) {
        message->handle_count = std::min(message->handle_count,
                                         header.handle_count);
        if (ReadAll(handle, message->handles,
                    message->handle_count * sizeof(Handle)) !=
            message->handle_count * sizeof(Handle)) {
          break;
        }
      } else {
        message->handle_count = 0;
      }
      if (message->handle_count < header.handle_count) {
        if (SkipHandles(handle, header.handle_count - message->handle_count) ==
            FALSE) {
          break;
        }
        message->flags |= kHandlesTruncated;
      }
      result = static_cast<int>(count);
      break;
    }
    case kCancel:
      SkipHandles(handle, header.handle_count);
      goto Repeat;
      break;
    default:
      break;
    }
  }
  return result;
}

}  // namespace

int ReceiveDatagram(Handle handle, MessageHeader* message, int flags) {
  // If handle is a bound socket, it is a named pipe in non-blocking mode.
  // Set is_bound_socket to true if handle has been created by BoundSocket().
  DWORD state;
  if (!GetNamedPipeHandleState(handle, &state, NULL, NULL, NULL, NULL, NULL)) {
    return -1;
  }

  if (!(state & PIPE_NOWAIT)) {
    // handle is a connected socket.
    return ReceiveDatagram(handle, message, flags, false);
  }

  // handle is a bound socket.
  for (;;) {
    if (ConnectNamedPipe(handle, NULL)) {
      // Note ConnectNamedPipe() for a handle in non-blocking mode returns a
      // nonzero value just to indicate that the pipe is now available to be
      // connected.
      continue;
    }
    switch (GetLastError()) {
    case ERROR_PIPE_LISTENING: {
      if (flags & kDontWait) {
        return -1;
      }
      // Set handle to blocking mode
      DWORD mode = PIPE_READMODE_BYTE | PIPE_WAIT;
      SetNamedPipeHandleState(handle, &mode, NULL, NULL);
      break;
    }
    case ERROR_PIPE_CONNECTED: {
      // Set handle to blocking mode
      DWORD mode = PIPE_READMODE_BYTE | PIPE_WAIT;
      SetNamedPipeHandleState(handle, &mode, NULL, NULL);
      int result = ReceiveDatagram(handle, message, flags, true);
      FlushFileBuffers(handle);
      // Set handle back to non-blocking mode
      mode = PIPE_READMODE_BYTE | PIPE_NOWAIT;
      SetNamedPipeHandleState(handle, &mode, NULL, NULL);
      DisconnectNamedPipe(handle);
      if (result == -1 && GetLastError() == ERROR_BROKEN_PIPE) {
        if (flags & kDontWait) {
          SetLastError(ERROR_PIPE_LISTENING);
          return result;
        }
      } else {
        return result;
      }
      break;
    }
    default:
      return -1;
      break;
    }
  }
}

}  // namespace nacl
