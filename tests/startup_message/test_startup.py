# Copyright (c) 2011 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import struct
import subprocess
import tempfile
import unittest

import naclimc
import nacl_launcher


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


class TestStartupMessage(unittest.TestCase):

  def assertIn(self, x, y):
    if x not in y:
      raise AssertionError("%r not in %r" % (x, y))

  def test_args_message(self):
    lib_dir = os.environ["NACL_LIBRARY_DIR"]
    write_fh, read_fh = MakeFileHandlePair()
    proc, [fd1, fd2] = nacl_launcher.SpawnSelLdrWithSockets(
        ["-s", "-a",
         "--", os.path.join(lib_dir, "runnable-ld.so"),
         # The remaining arguments are not used, because they are
         # overridden by the message we send:
         "this-arg-is-not-used"],
        [nacl_launcher.NACL_PLUGIN_BOUND_SOCK,
         nacl_launcher.NACL_PLUGIN_ASYNC_TO_CHILD_FD],
        stdout=write_fh)
    argv = ["unused-argv0", "--library-path", lib_dir,
            os.environ["HELLO_WORLD_PROG"]]
    envv = []
    fd2.imc_sendmsg(nacl_launcher.PackArgsMessage(argv, envv), tuple([]))
    self.assertEquals(proc.wait(), 0)
    self.assertEquals(read_fh.read(), "Hello, World!\n")

  def _TryStartupMessage(self, message, expected_line):
    lib_dir = os.environ["NACL_LIBRARY_DIR"]
    write_fh, read_fh = MakeFileHandlePair()
    proc, [fd1, fd2] = nacl_launcher.SpawnSelLdrWithSockets(
        ["-s", "-a",
         "--", os.path.join(lib_dir, "runnable-ld.so"),
         "this-arg-is-not-used"],
        [nacl_launcher.NACL_PLUGIN_BOUND_SOCK,
         nacl_launcher.NACL_PLUGIN_ASYNC_TO_CHILD_FD],
        stderr=write_fh)
    if message is not None:
      fd2.imc_sendmsg(message, tuple([]))
    del fd2
    self.assertEquals(proc.wait(), 127)
    output_lines = read_fh.read().split("\n")
    self.assertIn(expected_line, output_lines)

  def test_error_on_eof(self):
    self._TryStartupMessage(None, "Error receiving startup message (5)")

  def test_error_on_missing_tag(self):
    self._TryStartupMessage(
        "", "Startup message (0 bytes) lacks the expected tag")

  def test_error_on_missing_body(self):
    self._TryStartupMessage("ARGS", "Startup message too small (4 bytes)")

  def test_error_on_truncated_body(self):
    data = nacl_launcher.PackArgsMessage(["a", "b", "c"], [])
    self._TryStartupMessage(
        data[:-1], "Unterminated argument string in startup message")
    data = nacl_launcher.PackArgsMessage(["a", "b", "c"], ["e", "f"])
    self._TryStartupMessage(
        data[:-1], "Unterminated environment string in startup message")

  def test_error_on_excess_data(self):
    data = nacl_launcher.PackArgsMessage(["a", "b"], ["c", "d"])
    self._TryStartupMessage(
        data + "x", "Excess data (1 bytes) in startup message body")

  def test_error_on_overly_large_array_sizes(self):
    self._TryStartupMessage(
        struct.pack("4sII", "ARGS", 0x20000, 0),
        "Unterminated argument string in startup message")
    self._TryStartupMessage(
        struct.pack("4sII", "ARGS", 0x20000, -0x18000 & 0xffffffff),
        "Unterminated argument string in startup message")


if __name__ == "__main__":
  unittest.main()
