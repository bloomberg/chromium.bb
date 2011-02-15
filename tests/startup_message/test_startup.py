# Copyright (c) 2011 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import struct
import subprocess
import tempfile
import unittest

import naclimc


# Descriptor for a bound socket that the NaCl browser plugin sets up
NACL_PLUGIN_BOUND_SOCK = 3

# Descriptor for a connected socket that the NaCl browser plugin sets up
NACL_PLUGIN_ASYNC_SEND_FD = 7


def MakeFileHandlePair():
  # Make read/write file handle pair.  This is like creating a pipe
  # FD pair, but without a pipe buffer limit.
  fd, filename = tempfile.mkstemp(prefix="shell_test_")
  try:
    write_fh = os.fdopen(fd, "w", 0)
    read_fh = open(filename, "r")
  finally:
    os.unlink(filename)
  return write_fh, read_fh


def PackStringList(strings):
  return "".join(arg + "\0" for arg in strings)


def PackArgsMessage(argv, envv):
  return (struct.pack("4sII", "ARGS", len(argv), len(envv))
          + PackStringList(argv)
          + PackStringList(envv))


class TestStartupMessage(unittest.TestCase):

  def assertIn(self, x, y):
    if x not in y:
      raise AssertionError("%r not in %r" % (x, y))

  def _SpawnSelLdrWithSockets(self, args, fd_slots, **kwargs):
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

  def test_args_message(self):
    lib_dir = os.environ["NACL_LIBRARY_DIR"]
    write_fh, read_fh = MakeFileHandlePair()
    proc, [fd1, fd2] = self._SpawnSelLdrWithSockets(
        ["-s", "-a",
         "--", os.path.join(lib_dir, "runnable-ld.so"),
         # The remaining arguments are not used, because they are
         # overridden by the message we send:
         "this-arg-is-not-used"],
        [NACL_PLUGIN_BOUND_SOCK, NACL_PLUGIN_ASYNC_SEND_FD],
        stdout=write_fh)
    argv = ["unused-argv0", "--library-path", lib_dir,
            os.environ["HELLO_WORLD_PROG"]]
    envv = []
    fd2.imc_sendmsg(PackArgsMessage(argv, envv), tuple([]))
    self.assertEquals(proc.wait(), 0)
    self.assertEquals(read_fh.read(), "Hello, World!\n")

  def _TryStartupMessage(self, message, expected_line):
    lib_dir = os.environ["NACL_LIBRARY_DIR"]
    write_fh, read_fh = MakeFileHandlePair()
    proc, [fd1, fd2] = self._SpawnSelLdrWithSockets(
        ["-s", "-a",
         "--", os.path.join(lib_dir, "runnable-ld.so"),
         "this-arg-is-not-used"],
        [NACL_PLUGIN_BOUND_SOCK, NACL_PLUGIN_ASYNC_SEND_FD],
        stderr=write_fh)
    if message is not None:
      fd2.imc_sendmsg(message, tuple([]))
    del fd2
    self.assertEquals(proc.wait(), 127)
    output_lines = read_fh.read().split("\n")
    self.assertIn(expected_line, output_lines)

  def test_error_on_eof(self):
    self._TryStartupMessage(None, "Error receiving startup message")

  def test_error_on_missing_tag(self):
    self._TryStartupMessage(
        "", "Startup message does not have the expected tag")

  def test_error_on_missing_body(self):
    self._TryStartupMessage("ARGS", "Startup message too small")

  def test_error_on_truncated_body(self):
    data = PackArgsMessage(["a", "b", "c"], [])
    self._TryStartupMessage(
        data[:-1], "Missing null terminator in argv list")
    data = PackArgsMessage(["a", "b", "c"], ["e", "f"])
    self._TryStartupMessage(
        data[:-1], "Missing null terminator in env list")

  def test_error_on_excess_data(self):
    data = PackArgsMessage(["a", "b"], ["c", "d"])
    self._TryStartupMessage(
        data + "x", "Excess data in message body")

  def test_error_on_overly_large_array_sizes(self):
    self._TryStartupMessage(
        struct.pack("4sII", "ARGS", 0x20000, 0),
        "argv/env too large")
    self._TryStartupMessage(
        struct.pack("4sII", "ARGS", 0x20000, -0x18000 & 0xffffffff),
        "argv/env too large")


if __name__ == "__main__":
  unittest.main()
