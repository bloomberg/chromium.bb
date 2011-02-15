# Copyright (c) 2011 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import subprocess
import struct
import sys

import naclimc


# Descriptor for a bound socket that the NaCl browser plugin sets up
NACL_PLUGIN_BOUND_SOCK = 3

# Descriptors for connected sockets that the NaCl browser plugin sets up
NACL_PLUGIN_ASYNC_FROM_CHILD_FD = 6
NACL_PLUGIN_ASYNC_TO_CHILD_FD = 7


def PackStringList(strings):
  return "".join(arg + "\0" for arg in strings)


def PackArgsMessage(argv, envv):
  return (struct.pack("4sII", "ARGS", len(argv), len(envv))
          + PackStringList(argv)
          + PackStringList(envv))


def SpawnSelLdrWithSockets(args, fd_slots, **kwargs):
  sockets = [(fd_slot, naclimc.os_socketpair())
             for fd_slot in fd_slots]
  extra_args = []
  for fd_slot, (child_fd, parent_fd) in sockets:
    extra_args.extend(["-i", "%i:%i" % (fd_slot, child_fd)])

  def PreExec():
    for fd_slot, (child_fd, parent_fd) in sockets:
      os.close(parent_fd)

  cmd = [os.environ["NACL_SEL_LDR"]] + extra_args + args
  proc = subprocess.Popen(cmd, preexec_fn=PreExec, **kwargs)
  for fd_slot, (child_fd, parent_fd) in sockets:
    os.close(child_fd)
  result_sockets = [naclimc.from_os_socket(parent_fd)
                    for fd_slot, (child_fd, parent_fd) in sockets]
  return proc, result_sockets


def FileServer(recv_socket, send_socket):
  while True:
    try:
      message, fds = recv_socket.imc_recvmsg(1024)
    # TODO(mseaborn): When the Python bindings raise a specific
    # exception type, we should test for that for EOF instead.
    except Exception:
      break
    method_id = message[:4]
    message_body = message[4:]
    if method_id == "Open":
      # TODO(mseaborn): When we handle more types of request, we can
      # factor out the unmarshalling code.
      format = "ii"
      flags, mode = struct.unpack_from(format, message_body)
      filename = message_body[struct.calcsize(format):]
      try:
        fd = os.open(filename, flags)
      except OSError:
        send_socket.imc_sendmsg("Fail", tuple([]))
      else:
        desc = naclimc.from_os_file_descriptor(fd)
        send_socket.imc_sendmsg("Okay", tuple([desc]))


def Main(args):
  lib_dir = os.environ["NACL_LIBRARY_DIR"]
  sel_ldr_args = [
      # TODO(mseaborn): Fix validation errors so that we do not need
      # to use -s (stubout mode) with nacl-glibc.
      "-s",
      "--", os.path.join(lib_dir, "runnable-ld.so")]
  proc, [fd1, recv_socket, send_socket] = SpawnSelLdrWithSockets(
      sel_ldr_args,
      [NACL_PLUGIN_BOUND_SOCK,
       NACL_PLUGIN_ASYNC_FROM_CHILD_FD,
       NACL_PLUGIN_ASYNC_TO_CHILD_FD])
  argv = ["unused-argv0", "--library-path", lib_dir] + args
  envv = ["NACL_FILE_RPC=1"]
  send_socket.imc_sendmsg(PackArgsMessage(argv, envv), tuple([]))
  FileServer(recv_socket, send_socket)
  sys.exit(proc.wait())


if __name__ == "__main__":
  Main(sys.argv[1:])
