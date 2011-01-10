# Copyright (c) 2010 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import sys

import naclimc


def Main(args):
  sock_fd = int(args[0])
  socket = naclimc.from_os_socket(sock_fd)
  socket.imc_sendmsg("message from test_prog", ())


if __name__ == "__main__":
  Main(sys.argv[1:])
