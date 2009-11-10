
# Copyright 2009  The Native Client Authors.  All rights reserved.
# Use of this source code is governed by a BSD-style license that can
# be found in the LICENSE file.

import os
import shutil
import subprocess
import tempfile
import unittest


# From http://lackingrhoticity.blogspot.com/2008/11/tempdirtestcase-python-unittest-helper.html
class TempDirTestCase(unittest.TestCase):

  def setUp(self):
    self._on_teardown = []

  def make_temp_dir(self):
    temp_dir = tempfile.mkdtemp(prefix="tmp-%s-" % self.__class__.__name__)
    def tear_down():
      shutil.rmtree(temp_dir)
    self._on_teardown.append(tear_down)
    return temp_dir

  def tearDown(self):
    for func in reversed(self._on_teardown):
      func()


def write_file(filename, data):
  fh = open(filename, "w")
  try:
    fh.write(data)
  finally:
    fh.close()


# TODO: use subprocess.check_call when we have Python 2.5 on Windows.
def check_call(*args, **kwargs):
  rc = subprocess.call(*args, **kwargs)
  if rc != 0:
    raise Exception("Failed with return code %i" % rc)


class ToolchainTests(TempDirTestCase):

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
    check_call(["nacl-gcc", os.path.join(temp_dir, "code.c"),
                "-o", os.path.join(temp_dir, "prog")])
    rc = subprocess.call(["ncval", os.path.join(temp_dir, "prog")],
                         stdout=open(os.devnull, "w"))
    self.assertEquals(rc, 1)

  def test_computed_gotos(self):
    # Test for toolchain bug.
    # Bug 1:  gcc outputs "jmp *%eax", fails to validate.
    # Bug 2:  gcc outputs "nacljmp *%eax", fails to assemble.
    # Correct assembly output is "nacljmp %eax".
    code = """
int foo(int i) {
  void *addr[] = { &&label1, &&label2 };
  goto *addr[i];
 label1:
  return 1234;
 label2:
  return 4568;
}

int main() {
  return 0;
}
"""
    temp_dir = self.make_temp_dir()
    write_file(os.path.join(temp_dir, "code.c"), code)
    # ncval doesn't seem to accept .o files any more.
    # TODO: fix ncval.
    check_call(["nacl-gcc", # "-c",
                os.path.join(temp_dir, "code.c"),
                "-o", os.path.join(temp_dir, "prog")])
    check_call(["ncval", os.path.join(temp_dir, "prog")],
               stdout=open(os.devnull, "w"))


if __name__ == "__main__":
  unittest.main()
