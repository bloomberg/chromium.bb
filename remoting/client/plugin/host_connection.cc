// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/plugin/host_connection.h"

#include <netinet/in.h>
#include <netdb.h>
#include <sys/socket.h>

#include <iostream>  // TODO(garykac): Replace or remove debug output.

#include "remoting/client/plugin/chromoting_plugin.h"

namespace remoting {

HostConnection::HostConnection() {
  connected_ = false;
}

HostConnection::~HostConnection() {
}

bool HostConnection::connect(const char* ip_address) {
  std::cout << "Attempting to connect to " << ip_address << std::endl;

  sock_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (sock_ < 0) {
    std::cout << "Can't open socket" << std::endl;
    return false;
  }

  hostent* server = gethostbyname(ip_address);
  if (!server) {
    std::cout << "Can't resolve address" << std::endl;
    return false;
  }

  sockaddr_in server_address;
  memset(&server_address, 0, sizeof(server_address));
  server_address.sin_family = AF_INET;
  memcpy(&server_address.sin_addr.s_addr, server->h_addr, server->h_length);
  server_address.sin_port = htons(4000);

  if (::connect(sock_, reinterpret_cast<sockaddr*>(&server_address),
                sizeof(server_address)) < 0) {
    std::cout << "Cannot connect to server" << std::endl;
    return false;
  }

  connected_ = true;
  return true;
}

// Read data from a socket to a buffer.
bool HostConnection::read_data(char* buffer, int num_bytes) {
  if (!connected_) {
    return false;
  }

  while (num_bytes > 0) {
    int num_bytes_read = read(sock_, buffer, num_bytes);
    if (num_bytes_read <= 0) {
      return false;
    }
    buffer += num_bytes_read;
    num_bytes -= num_bytes_read;
  }
  return true;
}

// Write data from a buffer to a socket.
bool HostConnection::write_data(const char* buffer, int num_bytes) {
  if (!connected_) {
    return false;
  }

  while (num_bytes > 0) {
    int num_bytes_written = write(sock_, buffer, num_bytes);
    if (num_bytes_written <= 0) {
      return false;
    }
    num_bytes -= num_bytes_written;
    buffer += num_bytes_written;
  }
  return true;
}

}  // namespace remoting
