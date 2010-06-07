// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_PLUGIN_HOST_CONNECTION_H_
#define REMOTING_CLIENT_PLUGIN_HOST_CONNECTION_H_

#include "base/basictypes.h"

namespace remoting {

class HostConnection {
 public:
  HostConnection();
  virtual ~HostConnection();

  bool connect(const char* ip_address);
  bool connected() {
    return connected_;
  }

  bool read_data(char *buffer, int num_bytes);
  bool write_data(const char* buffer, int num_bytes);

 private:
  // True if we have a valid connection.
  bool connected_;

  // The socket for the connection.
  int sock_;

  DISALLOW_COPY_AND_ASSIGN(HostConnection);
};

}  // namespace remoting

#endif  // REMOTING_CLIENT_PLUGIN_HOST_CONNECTION_H_
