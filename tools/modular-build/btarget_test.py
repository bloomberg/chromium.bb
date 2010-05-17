
# Copyright 2010 The Native Client Authors.  All rights reserved.
# Use of this source code is governed by a BSD-style license that can
# be found in the LICENSE file.

import StringIO
import os
import shutil
import subprocess
import unittest

import dirtree
import btarget
from dirtree_test import TempDirTestCase


def PlanToString(targets):
  stream = StringIO.StringIO()
  btarget.PrintPlan(targets, stream)
  return stream.getvalue()


class BuildTargetTests(TempDirTestCase):

  def test_src(self):
    tempdir = self.MakeTempDir()
    src = btarget.SourceTarget("src", os.path.join(tempdir, "src"),
                              dirtree.CopyTree("example"))
    self.assertEquals(PlanToString([src]), "src: yes\n")
    btarget.Rebuild([src])
    self.assertEquals(PlanToString([src]), "src: no\n")

  def test_build(self):
    tempdir = self.MakeTempDir()
    src = btarget.SourceTarget("src", os.path.join(tempdir, "src"),
                              dirtree.CopyTree("example"))
    input_prefix = btarget.UnionDir("input",
                                    os.path.join(tempdir, "prefix"),
                                    [])
    install = btarget.AutoconfModule("bld",
                                     os.path.join(tempdir, "install"),
                                     os.path.join(tempdir, "build"),
                                     input_prefix,
                                     src)
    self.assertEquals(PlanToString([install]),
                      "input: yes\nsrc: yes\nbld: yes\n")
    btarget.Rebuild([install])
    self.assertEquals(PlanToString([install]),
                      "input: no\nsrc: no\nbld: no\n")

    proc = subprocess.Popen(
        [os.path.join(tempdir, "install/bin/hellow")],
        stdout=subprocess.PIPE)
    stdout = proc.communicate()[0]
    self.assertEquals(proc.wait(), 0)
    self.assertEquals(stdout, "Hello world\n")

    # Check that input/prefix gets rebuilt if it is manually
    # deleted.
    shutil.rmtree(os.path.join(tempdir, "prefix"))
    os.unlink(os.path.join(tempdir, "prefix.state"))
    os.unlink(os.path.join(tempdir, "prefix.state.log"))
    self.assertEquals(PlanToString([install]),
                      "input: yes\nsrc: no\nbld: maybe\n")
    btarget.Rebuild([install])


if __name__ == "__main__":
  unittest.main()
