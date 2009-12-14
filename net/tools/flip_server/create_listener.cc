// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <arpa/inet.h>   // for inet_ntop
#include <errno.h>       // for strerror
#include <netdb.h>       // for getaddrinfo and getnameinfo
#include <netinet/in.h>  // for IPPROTO_*, etc.
#include <stdlib.h>      // for EXIT_FAILURE
#include <sys/socket.h>  // for getaddrinfo and getnameinfo
#include <sys/types.h>   // "
#include <unistd.h>      // for exit()

#include "net/tools/flip_server/create_listener.h"

#include "base/logging.h"

namespace net {

// used to ensure we delete the addrinfo structure
// alloc'd by getaddrinfo
class AddrinfoGuard {
 protected:
  struct addrinfo * addrinfo_ptr_;
 public:

  explicit AddrinfoGuard(struct addrinfo* addrinfo_ptr) :
    addrinfo_ptr_(addrinfo_ptr) {}

  ~AddrinfoGuard() {
    freeaddrinfo(addrinfo_ptr_);
  }
};

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

// Summary:
//   Closes a socket, with option to attempt it multiple times.
//   Why do this? Well, if the system-call gets interrupted, close
//   can fail with EINTR. In that case you should just retry.. Unfortunately,
//   we can't be sure that errno is properly set since we're using a
//   multithreaded approach in the filter proxy, so we should just retry.
// Args:
//   fd - the socket to close
//   tries - the number of tries to close the socket.
// Returns:
//   true - if socket was closed
//   false - if socket was NOT closed.
// Side-effects:
//   sets *fd to -1 if socket was closed.
//
bool CloseSocket(int *fd, int tries) {
  for (int i = 0; i < tries; ++i) {
    if (!close(*fd)) {
      *fd = -1;
      return true;
    }
  }
  return false;
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

// see header for documentation of this function.
void CreateListeningSocket(const string& host,
                           const string& port,
                           bool is_numeric_host_address,
                           int backlog,
                           int * listen_fd,
                           bool reuseaddr,
                           bool reuseport,
                           ostream* error_stream) {
  // start out by assuming things will fail.
  *listen_fd = -1;

  const char* node = NULL;
  const char* service = NULL;

  if (!host.empty()) node = host.c_str();
  if (!port.empty()) service = port.c_str();

  struct addrinfo *results = 0;
  struct addrinfo hints;
  memset(&hints, 0, sizeof(hints));

  if (is_numeric_host_address) {
    hints.ai_flags = AI_NUMERICHOST;  // iff you know the name is numeric.
  }
  hints.ai_flags |= AI_PASSIVE;

  hints.ai_family = PF_INET;       // we know it'll be IPv4, but if we didn't
  // hints.ai_family = PF_UNSPEC;  // know we'd use this. <---
  hints.ai_socktype = SOCK_STREAM;

  int err = 0;
  if ((err=getaddrinfo(node, service, &hints, &results))) {
    // gai_strerror -is- threadsafe, so we get to use it here.
    *error_stream << "getaddrinfo " << " for (" << host << ":" << port
                  << ") " << gai_strerror(err) << "\n";
    return;
  }
  // this will delete the addrinfo memory when we return from this function.
  AddrinfoGuard addrinfo_guard(results);

  int sock = socket(results->ai_family,
                    results->ai_socktype,
                    results->ai_protocol);
  if (sock == -1) {
    *error_stream << "Unable to create socket for (" << host << ":"
      << port << "): " << strerror(errno) << "\n";
    return;
  }

  if (reuseaddr) {
    // set SO_REUSEADDR on the listening socket.
    int on = 1;
    int rc;
    rc = setsockopt(sock, SOL_SOCKET,  SO_REUSEADDR,
                    reinterpret_cast<char *>(&on), sizeof(on));
    if (rc < 0) {
      close(sock);
      LOG(FATAL) << "setsockopt() failed fd=" << listen_fd << "\n";
    }
  }
#ifndef SO_REUSEPORT
#define SO_REUSEPORT 15
#endif
  if (reuseport) {
    // set SO_REUSEADDR on the listening socket.
    int on = 1;
    int rc;
    rc = setsockopt(sock, SOL_SOCKET,  SO_REUSEPORT,
                    reinterpret_cast<char *>(&on), sizeof(on));
    if (rc < 0) {
      close(sock);
      LOG(FATAL) << "setsockopt() failed fd=" << listen_fd << "\n";
    }
  }

  if (bind(sock, results->ai_addr, results->ai_addrlen)) {
    *error_stream << "Bind was unsuccessful for (" << host << ":"
      << port << "): " << strerror(errno) << "\n";
    // if we knew that we were not multithreaded, we could do the following:
    // " : " << strerror(errno) << "\n";
    if (CloseSocket(&sock, 100)) {
      return;
    } else {
      // couldn't even close the dang socket?!
      *error_stream << "Unable to close the socket.. Considering this a fatal "
                      "error, and exiting\n";
      exit(EXIT_FAILURE);
    }
  }

  if (listen(sock, backlog)) {
    // listen was unsuccessful.
    *error_stream << "Listen was unsuccessful for (" << host << ":"
      << port << "): " << strerror(errno) << "\n";
    // if we knew that we were not multithreaded, we could do the following:
    // " : " << strerror(errno) << "\n";

    if (CloseSocket(&sock, 100)) {
      sock = -1;
      return;
    } else {
      // couldn't even close the dang socket?!
      *error_stream << "Unable to close the socket.. Considering this a fatal "
                      "error, and exiting\n";
      exit(EXIT_FAILURE);
    }
  }
  // If we've gotten to here, Yeay! Success!
  *listen_fd = sock;
}

}  // namespace net

