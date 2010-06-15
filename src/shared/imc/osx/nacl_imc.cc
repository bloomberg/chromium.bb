/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */


// NaCl inter-module communication primitives.

#include "native_client/src/shared/imc/nacl_imc.h"
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>

#include "native_client/src/shared/platform/nacl_log.h"

#define OSX_BLEMISH_HEURISTIC 1
/*
 * TODO(bsy,bradnelson): remove SIGPIPE_FIX.  It is needed for future
 * testing because our test framework appears to not see the SIGPIPE
 * on OSX when the fix is not in place.  We've tracked it down to the
 * Python subprocess module, where if we manually run
 * subprocess.Popen('...path-to-sigpipe_test...') the SIGPIPE doesn't
 * actually occur(!); however, when running the same sigpipe_test
 * executable from the shell it's apparent that the SIGPIPE *does*
 * occur.  Presumably it's some weird code path in subprocess that is
 * leaving the signal handler for SIGPIPE as SIG_IGN rather than
 * SIG_DFL.  Unfortunately, we could not create a simpler test of our
 * test infrastructure (writing to a pipe that's closed) -- perhaps
 * the multithreaded nature of sigpipe_test is involved.
 *
 * In production code, SIGPIPE_FIX should be 1.  The old behavior is
 * only needed to help us track down the problem in python.
 */
#define SIGPIPE_FIX           1

/*
 * The code guarded by SIGPIPE_FIX has been found to still raise
 * SIGPIPE in certain situations. Until we can boil this down to a
 * small test case and, possibly, file a bug against the OS, we need
 * to forcibly suppress these signals.
 */
#define SIGPIPE_ALT_FIX       1

#if OSX_BLEMISH_HEURISTIC || SIGPIPE_ALT_FIX
# include <signal.h>
#endif  // OSX_BLEMISH_HEURISTIC || SIGPIPE_ALT_FIX

#include <algorithm>
#include "native_client/src/trusted/service_runtime/include/sys/nacl_imc_api.h"

namespace nacl {

namespace {

#if OSX_BLEMISH_HEURISTIC
// The number of recvmsg retries to perform to determine --
// heuristically, unfortunately -- if the remote end of the socketpair
// had actually closed.  This is a (new) hacky workaround for an OSX
// blemish that replaces the older, buggier workaround.
const int kRecvMsgRetries = 8;
#endif

// The maximum number of IOVec elements sent by SendDatagram(). Plus one for
// NaClInternalHeader with the descriptor data bytes.
const size_t kIovLengthMax = NACL_ABI_IMC_IOVEC_MAX + 1;

// The IMC datagram header followed by a message_bytes of data sent over the
// a stream-oriented socket. We need to use stream-oriented socket for OS X
// since it doesn't support file descriptor transfer over SOCK_DGRAM socket
// like Linux.
struct Header {
  // The total bytes of data in the IMC datagram excluding the size of Header.
  size_t message_bytes;
  // The total number of handles to be transferred with IMC datagram.
  size_t handle_count;
};

#if !OSX_BLEMISH_HEURISTIC
// A ControlBlock entry is allocated for each socket descriptor created by
// socketpair() to amend the undesirable OS X behavior described in Close()
// function.
struct ControlBlock {
  // The file descriptor number of the other end of the socket pair.
  int pair_fd;
  // true if this descriptor has been closed.
  bool closed;
  // true if this descriptor has been transferred.
  bool transferred;
};
#endif  // !OSX_BLEMISH_HEURISTIC

// The maximum number of I/O descriptors to manage with the IMC library;
// Note the typical sysconf(_SC_OPEN_MAX) value is 256 on OS X.
const int kMaxHandles = 2048;

#if !OSX_BLEMISH_HEURISTIC
// The global ControlBlock table indexed by the I/O descriptor number.
ControlBlock control_table[kMaxHandles];
#endif  // !OSX_BLEMISH_HEURISTIC

// The pathname prefix for bound sockets created by BoundSocket().
const char kNamePrefix[] = "/tmp/nacl-";
// TODO(shiki): This is potentially unsafe. tmp cleaners can delete files out
// from under us, if user leaves browser running but idle for a while. In
// practice, it is a longish time, but something safer would be better, e.g.,
// $HOME/.nacl/, if there are no other insurmountable problems with it.
// On OS, the default is to delete everything in /Temp which hasn't been
// accessed for >3 days. This script runs once a day. So a "long weekend" might
// be a problem.
// (like .mozilla, if logged in to two different machines and on both machine
// run code that mutate files believing in having exclusive access, could cause
// problems.)


// Get a pointer to sockaddr that corresponds to address.
// The sa parameter must be a valid pointer to struct sockaddr_un.
// On return it contains the name that can be used with the bind() system call.
// On success, a pointer to sa type-casted to sockaddr is returned. On error,
// NULL is returned.
struct sockaddr* GetSocketAddress(const SocketAddress* address,
                                  struct sockaddr_un* sa) {
  if (address == NULL || !isalnum(address->path[0])) {
    return NULL;
  }
  memset(sa, 0, sizeof(struct sockaddr_un));
  sa->sun_family = AF_UNIX;
  memmove(sa->sun_path, kNamePrefix, sizeof kNamePrefix - 1);
  char* p = &sa->sun_path[sizeof kNamePrefix - 1];
  for (const char* q = address->path;
       *q != '\0' && p < &sa->sun_path[sizeof sa->sun_path];
       ++p, ++q) {
    /* do not perform case-folding in a case-sensitive environment */
    *p = *q;
  }
  return reinterpret_cast<sockaddr*>(sa);
}

// Gets an array of file descriptors stored in msg.
// The fdv parameter must be an int array of kHandleCountMax elements.
// GetRights() returns the number of file descriptors copied into fdv.
size_t GetRights(struct msghdr* msg, int* fdv) {
  if (msg->msg_controllen == 0) {
    return 0;
  }
  size_t count = 0;
  for (struct cmsghdr* cmsg = CMSG_FIRSTHDR(msg);
       cmsg != 0;
       cmsg = CMSG_NXTHDR(msg, cmsg)) {
    if (cmsg->cmsg_level == SOL_SOCKET && cmsg->cmsg_type == SCM_RIGHTS) {
      while (CMSG_LEN((1 + count) * sizeof(int)) <= cmsg->cmsg_len) {
        *fdv++ = *(reinterpret_cast<int*>(CMSG_DATA(cmsg)) + count);
        ++count;
      }
    }
  }
  return count;
}

// Skips the specified length of octets when reading from a handle. Skipped
// octets are discarded.
// On success, true is returned. On error, false is returned.
bool SkipFile(int handle, size_t length) {
  while (0 < length) {
    char scratch[1024];
    size_t count = std::min(sizeof scratch, length);
    count = read(handle, scratch, count);
    if (static_cast<ssize_t>(count) == -1 || count == 0) {
      return false;
    }
    length -= count;
  }
  return true;
}

#if !OSX_BLEMISH_HEURISTIC
// Returns true if handle is the one end of a socket pair.
bool IsSocketPair(Handle handle) {
  if (handle < 0 || kMaxHandles <= handle) {
    return false;
  }
  int pair_fd = control_table[handle].pair_fd;
  if (pair_fd == handle || pair_fd < 0 || kMaxHandles <= pair_fd) {
    return false;
  }
  return control_table[pair_fd].pair_fd == handle;
}
#endif  // !OSX_BLEMISH_HEURISTIC

// Returns true if handle is a bound socket.
bool IsBoundSocket(Handle handle, struct sockaddr_un* sa, socklen_t len) {
  return getsockname(handle, reinterpret_cast<sockaddr*>(sa), &len) == 0 &&
         sizeof sa->sun_family < len &&
         sa->sun_family == AF_UNIX &&
         strncmp(sa->sun_path, kNamePrefix, sizeof kNamePrefix - 1) == 0;
}

#if SIGPIPE_ALT_FIX
// TODO(kbr): move this to an Init() function so it isn't called all
// the time.
bool IgnoreSIGPIPE() {
  sigset_t mask;
  sigemptyset(&mask);
  struct sigaction sa;
  sa.sa_handler = SIG_IGN;
  sa.sa_mask = mask;
  sa.sa_flags = 0;
  return sigaction(SIGPIPE, &sa, NULL) == 0;
}
#endif

}  // namespace

Handle BoundSocket(const SocketAddress* address) {
  int s = socket(AF_UNIX, SOCK_STREAM, 0);
  if (s == -1) {
    return -1;
  }
#if SIGPIPE_ALT_FIX
  if (!IgnoreSIGPIPE()) {
    close(s);
    return -1;
  }
#endif
#if SIGPIPE_FIX
  int nosigpipe = 1;
  if (0 != setsockopt(s, SOL_SOCKET, SO_NOSIGPIPE,
                      &nosigpipe, sizeof nosigpipe)) {
    close(s);
    return -1;
  }
#endif
  struct sockaddr_un sa;
  if (bind(s, GetSocketAddress(address, &sa), sizeof sa) == 0 &&
      listen(s, 5) == 0) {
    int fl = fcntl(s, F_GETFL, 0);
    fl |= O_NONBLOCK;
    fcntl(s, F_SETFL, fl);
    return s;
  }
  close(s);
  return -1;
}

int SocketPair(Handle pair[2]) {
  int result = socketpair(AF_UNIX, SOCK_STREAM, 0, pair);
  if (result == 0) {
    if (kMaxHandles <= pair[0] || kMaxHandles <= pair[1]) {
      close(pair[0]);
      close(pair[1]);
      errno = EMFILE;
      return -1;
    }
#if SIGPIPE_ALT_FIX
    if (!IgnoreSIGPIPE()) {
      close(pair[0]);
      close(pair[1]);
      return -1;
    }
#endif
#if SIGPIPE_FIX
    int nosigpipe = 1;
    if (0 != setsockopt(pair[0], SOL_SOCKET, SO_NOSIGPIPE,
                        &nosigpipe, sizeof nosigpipe) ||
        0 != setsockopt(pair[1], SOL_SOCKET, SO_NOSIGPIPE,
                        &nosigpipe, sizeof nosigpipe)) {
      close(pair[0]);
      close(pair[1]);
      return -1;
    }
#endif
#if !OSX_BLEMISH_HEURISTIC
    // Allocates two control blocks that refer each other.
    ControlBlock* block;
    block = &control_table[pair[0]];
    block->pair_fd = pair[1];
    block->closed = block->transferred = false;
    block = &control_table[pair[1]];
    block->pair_fd = pair[0];
    block->closed = block->transferred = false;
#endif  // !OSX_BLEMISH_HEURISTIC
  }
  return result;
}

int Close(Handle handle) {
  struct sockaddr_un sa;

  if (IsBoundSocket(handle, &sa, sizeof sa)) {
    unlink(sa.sun_path);
#if !OSX_BLEMISH_HEURISTIC
  } else if (IsSocketPair(handle)) {
    // In OS X, after a process closes one end of a socket pair which has been
    // transferred to the other process, the socket the remote process has
    // received is treated as if it is closed, and read() returns immediately
    // with zero if there is no data while it can still receive incoming data.
    // To amend this behavior, we do not close transferred socket pair handles
    // until both ends are closed
    // TODO(shiki): Make this code block thread-safe, and make it sure it will
    // be safe to kill a thread that is executing in IMC code.
    ControlBlock* block = &control_table[handle];
    ControlBlock* pair = &control_table[block->pair_fd];
    block->closed = true;
    if (pair->pair_fd == handle) {
      if (pair->closed) {
        pair->pair_fd = -1;
        close(block->pair_fd);
      } else if (block->transferred) {
        // Close this handle when the other end is closed.
        return 0;
      }
    }
    block->pair_fd = -1;
#endif  // !OSX_BLEMISH_HEURISTIC
  }
  return close(handle);
}

int SendDatagram(Handle handle, const MessageHeader* message, int flags) {
  struct msghdr msg;
  struct iovec vec[kIovLengthMax + 1];
  unsigned char buf[CMSG_SPACE(kHandleCountMax * sizeof(int))];
  Header header = { 0, 0 };

  (void) flags;  /* BUG(shiki): unused parameter */

  /*
   * The following assert was an earlier attempt to remember/check the
   * assumption that our struct IOVec -- which we must define to be
   * cross platform -- is compatible with struct iovec on *x systems.
   * The length field of IOVec was switched to be uint32_t at oen point
   * to use concrete types, which introduced a problem on 64-bit systems.
   *
   * Clearly, the assert does not check a strong-enough condition,
   * since structure padding would make the two sizes the same.
   *
  assert(sizeof(struct iovec) == sizeof(IOVec));
   *
   * Don't do this again!
   */

  if (!MessageSizeIsValid(message)) {
    errno = EMSGSIZE;
    return -1;
  }

  if (kHandleCountMax < message->handle_count ||
      kIovLengthMax < message->iov_length) {
    errno = EMSGSIZE;
    return -1;
  }

  memmove(&vec[1], message->iov, sizeof(IOVec) * message->iov_length);

  msg.msg_name = 0;
  msg.msg_namelen = 0;
  msg.msg_iov = vec;
  msg.msg_iovlen = 1 + message->iov_length;
  if (0 < message->handle_count && message->handles != NULL) {
    int size = message->handle_count * sizeof(int);
    msg.msg_control = buf;
    msg.msg_controllen = CMSG_SPACE(size);
    struct cmsghdr* cmsg = CMSG_FIRSTHDR(&msg);
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_RIGHTS;
    cmsg->cmsg_len = CMSG_LEN(size);
    memcpy(reinterpret_cast<int*>(CMSG_DATA(cmsg)), message->handles, size);
    msg.msg_controllen = cmsg->cmsg_len;
    header.handle_count = message->handle_count;
  } else {
    msg.msg_control = 0;
    msg.msg_controllen = 0;
  }
  msg.msg_flags = 0;

  // Send data with the header atomically. Note to send file descriptors we need
  // to send at least one byte of data.
  for (size_t i = 0; i < message->iov_length; ++i) {
    header.message_bytes += message->iov[i].length;
  }
  vec[0].iov_base = &header;
  vec[0].iov_len = sizeof header;
  int result = sendmsg(handle, &msg, 0);
  if (result == -1) {
    return -1;
  }
  if (static_cast<size_t>(result) < sizeof header) {
    errno = EMSGSIZE;
    return -1;
  }
#if !OSX_BLEMISH_HEURISTIC
  if (0 < message->handle_count && message->handles != NULL) {
    for (size_t i = 0; i < message->handle_count; ++i) {
      if (IsSocketPair(message->handles[i])) {
        control_table[message->handles[i]].transferred = true;
      }
    }
  }
#endif
  return result - sizeof header;
}

int SendDatagramTo(Handle handle, const MessageHeader* message, int flags,
                   const SocketAddress* name) {
  if (name == NULL) {
    return SendDatagram(handle, message, flags);
  }
  struct sockaddr_un sa;
  int s = socket(AF_UNIX, SOCK_STREAM, 0);
  if  (s == -1) {
    return -1;
  }
#if SIGPIPE_ALT_FIX
  if (!IgnoreSIGPIPE()) {
    close(s);
    return -1;
  }
#endif
#if SIGPIPE_FIX
  int nosigpipe = 1;
  if (0 != setsockopt(s, SOL_SOCKET, SO_NOSIGPIPE,
                      &nosigpipe, sizeof nosigpipe)) {
    (void) close(s);
    return -1;
  }
#endif
  if (connect(s, GetSocketAddress(name, &sa), sizeof sa) == -1) {
    (void) close(s);
    switch (errno) {
      case ENOENT: {
        errno = ECONNREFUSED;
        break;
      }
    }
    return -1;
  }
  int result = SendDatagram(s, message, flags);
  close(s);
  return result;
}

namespace {

int Receive(Handle handle, MessageHeader* message, int flags,
            bool bound_socket) {
  struct msghdr msg;
  struct iovec vec[kIovLengthMax];
  unsigned char buf[CMSG_SPACE(kHandleCountMax * sizeof(int))];

  if (kHandleCountMax < message->handle_count ||
      kIovLengthMax < message->iov_length) {
    errno = EMSGSIZE;
    return -1;
  }

  /*
   * The following assert was an earlier attempt to remember/check the
   * assumption that our struct IOVec -- which we must define to be
   * cross platform -- is compatible with struct iovec on *x systems.
   * The length field of IOVec was switched to be uint32_t at oen point
   * to use concrete types, which introduced a problem on 64-bit systems.
   *
   * Clearly, the assert does not check a strong-enough condition,
   * since structure padding would make the two sizes the same.
   *
  assert(sizeof(struct iovec) == sizeof(IOVec));
   *
   * Don't do this again!
   */

  if (!MessageSizeIsValid(message)) {
    errno = EMSGSIZE;
    return -1;
  }

  message->flags = 0;
  // Receive the header of the message and handles first.
  Header header;
  struct iovec header_vec = { &header, sizeof header };
  msg.msg_iov = &header_vec;
  msg.msg_iovlen = 1;
  msg.msg_name = 0;
  msg.msg_namelen = 0;
  if (0 < message->handle_count && message->handles != NULL) {
    msg.msg_control = buf;
    msg.msg_controllen = CMSG_SPACE(message->handle_count * sizeof(int));
  } else {
    msg.msg_control = 0;
    msg.msg_controllen = 0;
  }
  msg.msg_flags = 0;
#if OSX_BLEMISH_HEURISTIC
  int count;
  int retry_count;
  for (retry_count = 0; retry_count < kRecvMsgRetries; ++retry_count) {
    if (0 != (count = recvmsg(handle, &msg,
                              (flags & kDontWait) ? MSG_DONTWAIT : 0))) {
      break;
    }
  }
  if (0 != retry_count && kRecvMsgRetries != retry_count) {
    printf("OSX_BLEMISH_HEURISTIC: retry_count = %d, count = %d\n",
           retry_count, count);
  }
#else
  int count = recvmsg(handle, &msg,
                      (flags & kDontWait) ? MSG_DONTWAIT : 0);
#endif
  size_t handle_count = 0;
  if (0 < count) {
    handle_count = GetRights(&msg, message->handles);
  }
  if (count != sizeof header) {
    while (0 < handle_count) {
      // Note if the sender has sent one end of a socket pair here,
      // ReceiveDatagram() for that socket will result in a zero length read
      // return henceforth.
      close(message->handles[--handle_count]);
    }
    if (count == 0) {
      if (bound_socket) {
        errno = EAGAIN;
        return -1;
      }
      message->handle_count = 0;
      return 0;
    }
    if (count != -1) {
      // TODO(shiki): We should call recvmsg() again here since it could get to
      // wake up with a partial header since the SOCK_STREAM socket does not
      // required to maintain message boundaries.
      errno = EMSGSIZE;
    }
    return -1;
  }

  message->handle_count = handle_count;

  // OS X seems not to set the MSG_CTRUNC flag in msg.msg_flags as we expect,
  // and we don't rely on it.
  if (message->handle_count < header.handle_count) {
    message->flags |= kHandlesTruncated;
  }

  if (header.message_bytes == 0) {
    return 0;
  }

  // Update message->iov to receive just message_bytes.
  memmove(vec, message->iov, sizeof(IOVec) * message->iov_length);
  msg.msg_iov = vec;
  msg.msg_iovlen = message->iov_length;
  size_t buffer_bytes = 0;
  size_t i;
  for (i = 0; i < message->iov_length; ++i) {
    buffer_bytes += vec[i].iov_len;
    if (header.message_bytes <= buffer_bytes) {
      vec[i].iov_len -= buffer_bytes - header.message_bytes;
      buffer_bytes = header.message_bytes;
      msg.msg_iovlen = i + 1;
      break;
    }
  }
  if (buffer_bytes < header.message_bytes) {
    message->flags |= kMessageTruncated;
  }

  // Receive the sent data.
  msg.msg_name = 0;
  msg.msg_namelen = 0;

  msg.msg_control = 0;
  msg.msg_controllen = 0;
  msg.msg_flags = 0;
#if OSX_BLEMISH_HEURISTIC
  for (retry_count = 0; retry_count < kRecvMsgRetries; ++retry_count) {
    if (0 != (count = recvmsg(handle, &msg,
                              (flags & kDontWait) ? MSG_DONTWAIT : 0))) {
      break;
    }
  }
  if (0 != retry_count && kRecvMsgRetries != retry_count) {
    printf("OSX_BLEMISH_HEURISTIC (2): retry_count = %d, count = %d\n",
           retry_count, count);
  }
#else  // OSX_BLEMISH_HEURISTIC
  count = recvmsg(handle, &msg, MSG_WAITALL);
#endif  // OSX_BLEMISH_HEURISTIC
  if (0 < count) {
    if (static_cast<size_t>(count) < header.message_bytes) {
      if (!SkipFile(handle, header.message_bytes - count)) {
        return -1;
      }
      message->flags |= kMessageTruncated;
    }
  }
  return count;
}

}  // namespace

int ReceiveDatagram(Handle handle, MessageHeader* message, int flags) {
  struct sockaddr_un sa;
  for (;;) {
    socklen_t address_len = sizeof sa;
    int s = accept(handle, reinterpret_cast<sockaddr*>(&sa), &address_len);
    if (s != -1) {
      int fl = fcntl(s, F_GETFL, 0);
      fl &= ~O_NONBLOCK;
      fcntl(s, F_SETFL, fl);
      int result = Receive(s, message, flags & ~kDontWait, true);
      close(s);
      if (result == -1 && !(flags & kDontWait)) {
        continue;
      }
      return result;
    }
    if (errno == EINVAL) {
      return Receive(handle, message, flags, false);
    }
    if (errno == EWOULDBLOCK) {
      if (flags & kDontWait) {
        return -1;
      }
      struct pollfd fd = { handle, POLLIN, 0 };
      if (poll(&fd, 1, -1) == -1) {
        return -1;
      }
    }
  }
}

}  // namespace nacl
