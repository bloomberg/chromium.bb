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
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>

#include "native_client/src/shared/platform/nacl_log.h"

namespace nacl {

namespace {

// The pathname prefix for bound sockets created by BoundSocket().
const char kNamePrefix[] = "google-nacl-";

// Get a pointer to sockaddr that corresponds to address.
// The sa parameter must be a valid pointer to struct sockaddr_un.
// On return it contains the name that can be used with the bind() system call.
// On success, a pointer to sa type-casted to sockaddr is returned. On error,
// NULL is returned.
struct sockaddr* GetSocketAddress(const SocketAddress* address,
                                  struct sockaddr_un* sa) {
  // Create a pathname for the abstract namespace. Note the abstract namespace
  // was introduced with Linux 2.2.
  if (address == NULL || !isalnum(address->path[0])) {
    return NULL;
  }
  memset(sa, 0, sizeof(struct sockaddr_un));
  sa->sun_family = AF_UNIX;
  memmove(sa->sun_path + 1, kNamePrefix, sizeof kNamePrefix - 1);
  char* p = &sa->sun_path[sizeof kNamePrefix];
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

}  // namespace

Handle BoundSocket(const SocketAddress* address) {
  int s = socket(AF_UNIX, SOCK_DGRAM, 0);
  if (s == -1) {
    return -1;
  }
  struct sockaddr_un sa;
  if (bind(s, GetSocketAddress(address, &sa), sizeof sa) == 0) {
    return s;
  }
  close(s);
  return -1;
}

int SocketPair(Handle pair[2]) {
  // The read operation for a SOCK_SEQPACKET socket returns zero when the
  // remote peer closed the connection unlike a SOCK_DGRAM socket. Note
  // SOCK_SEQPACKET was introduced with Linux 2.6.4.
  return socketpair(AF_UNIX, SOCK_SEQPACKET, 0, pair);
}

int Close(Handle handle) {
  return close(handle);
}

int SendDatagram(Handle handle, const MessageHeader* message, int flags) {
  return SendDatagramTo(handle, message, flags, NULL);
}

int SendDatagramTo(Handle handle, const MessageHeader* message, int flags,
                   const SocketAddress* name) {
  struct msghdr msg;
  struct sockaddr_un sa;
  unsigned char buf[CMSG_SPACE(kHandleCountMax * sizeof(int))];

  if (kHandleCountMax < message->handle_count) {
    errno = EMSGSIZE;
    return -1;
  }
  if (name != NULL) {
    msg.msg_name = GetSocketAddress(name, &sa);
    if (msg.msg_name == NULL) {
      errno = EINVAL;
      return -1;
    }
    msg.msg_namelen = sizeof sa;
  } else {
    msg.msg_name = 0;
    msg.msg_namelen = 0;
  }
  /*
   * The following assert was an earlier attempt to remember/check the
   * assumption that our struct IOVec -- which we must define to be
   * cross platform -- is compatible with struct iovec on *x systems.
   * The length field of IOVec was switched to be uint32_t at one point
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

  msg.msg_iov = reinterpret_cast<struct iovec*>(message->iov);
  msg.msg_iovlen = message->iov_length;

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
  } else {
    msg.msg_control = 0;
    msg.msg_controllen = 0;
  }
  msg.msg_flags = 0;
  return sendmsg(handle, &msg,
                 MSG_NOSIGNAL | ((flags & kDontWait) ? MSG_DONTWAIT : 0));
}

int ReceiveDatagram(Handle handle, MessageHeader* message, int flags) {
  struct msghdr msg;
  unsigned char buf[CMSG_SPACE(kHandleCountMax * sizeof(int))];

  if (kHandleCountMax < message->handle_count) {
    errno = EMSGSIZE;
    return -1;
  }
  msg.msg_name = 0;
  msg.msg_namelen = 0;

  /*
   * Make sure we cannot receive more than 2**32-1 bytes.
   */
  if (!MessageSizeIsValid(message)) {
    errno = EMSGSIZE;
    return -1;
  }

  msg.msg_iov = reinterpret_cast<struct iovec*>(message->iov);
  msg.msg_iovlen = message->iov_length;
  if (0 < message->handle_count && message->handles != NULL) {
    msg.msg_control = buf;
    msg.msg_controllen = CMSG_SPACE(message->handle_count * sizeof(int));
  } else {
    msg.msg_control = 0;
    msg.msg_controllen = 0;
  }
  msg.msg_flags = 0;
  message->flags = 0;
  int count = recvmsg(handle, &msg, (flags & kDontWait) ? MSG_DONTWAIT : 0);
  if (0 <= count) {
    message->handle_count = GetRights(&msg, message->handles);
    if (msg.msg_flags & MSG_TRUNC) {
      message->flags |= kMessageTruncated;
    }
    if (msg.msg_flags & MSG_CTRUNC) {
      message->flags |= kHandlesTruncated;
    }
  }
  return count;
}

}  // namespace nacl
