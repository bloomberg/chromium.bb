
# Copyright 2009  The Native Client Authors.  All rights reserved.
# Use of this source code is governed by a BSD-style license that can
# be found in the LICENSE file.

import os
import sys
sys.path.append(os.path.join(os.path.dirname(__file__),
                             "..", "..", "..", "tests"))

import re
import subprocess
import unittest

from testutils import read_file, write_file
import testutils


PARENT_DIR = os.path.dirname(__file__)


class ToolchainTests(testutils.TempDirTestCase):

  def test_ncval_returns_errors(self):
    # Check that ncval returns a non-zero return code when there is a
    # validation failure.
    source = """
int main() {
#ifdef __i386__
  __asm__("ret"); /* This comment appears in output */
#else
#  error Update this test for other architectures!
#endif
  return 0;
}
"""
    temp_dir = self.make_temp_dir()
    source_file = os.path.join(temp_dir, "code.c")
    write_file(source_file, source)
    testutils.check_call(["nacl-gcc", "-g", source_file,
                          "-o", os.path.join(temp_dir, "prog")])
    dest_file = os.path.join(self.make_temp_dir(), "file")
    rc = subprocess.call([sys.executable,
                          os.path.join(PARENT_DIR, "ncval_annotate.py"),
                          os.path.join(temp_dir, "prog")],
                         stdout=open(dest_file, "w"))
    # ncval_annotate should propagate the exit code through so that it
    # can be used as a substitute for ncval.
    self.assertEquals(rc, 1)

    # For some reason ncval produces two errors for the same instruction.
    # TODO(mseaborn): Tidy that up.
    expected = """
VALIDATOR: ADDRESS: ret instruction (not allowed)
  code: c3                   	ret    
  func: main
  file: FILENAME:4
    __asm__("ret"); /* This comment appears in output */

VALIDATOR: ADDRESS: Illegal instruction
  code: c3                   	ret    
  func: main
  file: FILENAME:4
    __asm__("ret"); /* This comment appears in output */
"""
    expected_pattern = re.escape(expected)
    expected_pattern = expected_pattern.replace("ADDRESS", "[0-9a-f]+")
    # Cygwin mixes \ and / in filenames, so be liberal in what we accept.
    expected_pattern = expected_pattern.replace("FILENAME", "\S+code.c")

    actual = read_file(dest_file).replace("\r", "")
    if re.match(expected_pattern + "$", actual) is None:
      raise AssertionError(
        "Output did not match.\n\nEXPECTED:\n%s\nACTUAL:\n%s"
        % (expected, actual))


if __name__ == "__main__":
  unittest.main()
