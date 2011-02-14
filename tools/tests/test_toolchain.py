
# Copyright 2009  The Native Client Authors.  All rights reserved.
# Use of this source code is governed by a BSD-style license that can
# be found in the LICENSE file.

import os
import sys
sys.path.append(os.path.join(os.path.dirname(__file__),
                             "..", "..", "tests"))

import subprocess
import unittest

from testutils import write_file
import testutils


def run_ncval(input_file):
  testutils.check_call(["ncval", input_file], stdout=open(os.devnull, "w"))


class ToolchainTests(testutils.TempDirTestCase):

  def test_ncval_returns_errors(self):
    # Check that ncval returns a non-zero return code when there is a
    # validation failure.
    code = """
int main() {
#ifdef __i386__
  __asm__("ret");
#else
#  error Update this test for other architectures!
#endif
  return 0;
}
"""
    temp_dir = self.make_temp_dir()
    write_file(os.path.join(temp_dir, "code.c"), code)
    testutils.check_call(["nacl-gcc", os.path.join(temp_dir, "code.c"),
                          "-o", os.path.join(temp_dir, "prog")])
    rc = subprocess.call(["ncval", os.path.join(temp_dir, "prog")],
                         stdout=open(os.devnull, "w"))
    self.assertEquals(rc, 1)

  def test_custom_linker_scripts_via_search_path(self):
    # Check that the linker will pick up linker scripts from the
    # "ldscripts" directory in the library search path (which is
    # specified with -L).
    # To test this, try to link to a symbol that is defined in our
    # example linker script.
    temp_dir = self.make_temp_dir()
    os.mkdir(os.path.join(temp_dir, "ldscripts"))
    write_file(os.path.join(temp_dir, "ldscripts", "elf_nacl.x"), """
foo = 0x1234;
""")
    write_file(os.path.join(temp_dir, "prog.c"), """
void foo();
int _start() {
  foo();
  return 0;
}
""")
    testutils.check_call(["nacl-gcc", "-nostartfiles", "-nostdlib",
                          "-L%s" % temp_dir,
                          os.path.join(temp_dir, "prog.c"),
                          "-o", os.path.join(temp_dir, "prog")])


if __name__ == "__main__":
  unittest.main()
