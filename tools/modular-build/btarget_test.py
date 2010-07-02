
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
    btarget.Rebuild([src], open(os.devnull, "w"))
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
    btarget.Rebuild([install], open(os.devnull, "w"))
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
    btarget.Rebuild([install], open(os.devnull, "w"))
    # Try again to test idempotence of install step.
    install.DoBuild()

  def test_building_specific_targets(self):
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
    root_targets = [install]

    self.assertEquals(btarget.SubsetTargets(root_targets, ["input"]),
                      [input_prefix])
    self.assertEquals(btarget.SubsetTargets(root_targets, ["src"]),
                      [src])
    # It should work if there are multiple paths to a target.
    self.assertEquals(btarget.SubsetTargets(root_targets * 2, ["src"]),
                      [src])

    # The default is to build all targets.
    stream = StringIO.StringIO()
    btarget.BuildMain(root_targets, ["-b"], stream)
    self.assertEquals(stream.getvalue(),
                      "input: yes\nsrc: yes\nbld: yes\n"
                      "** building input\n** building src\n** building bld\n")
    # But we can also list target names to build.
    stream = StringIO.StringIO()
    btarget.BuildMain(root_targets, ["-b", "input"], stream)
    self.assertEquals(stream.getvalue(),
                      "input: no\n"
                      "** skipping input\n")

  def test_install_root(self):
    # Test package that supports "make install install_root=DIR"
    # but not the usual "make install DESTDIR=DIR".
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
                                     src, use_install_root=True)
    btarget.Rebuild([src], open(os.devnull, "w"))
    # Override a file.
    dirtree.WriteFile(
        os.path.join(tempdir, "src", "Makefile.in"),
        dirtree.ReadFile(
            os.path.join(tempdir, "src", "Makefile.in_install_root")))
    btarget.Rebuild([install], open(os.devnull, "w"))
    assert os.path.exists(os.path.join(tempdir, "install/bin/hellow"))
    # Try again to test idempotence of install step.
    install.DoBuild()
    assert os.path.exists(os.path.join(tempdir, "install/bin/hellow"))


if __name__ == "__main__":
  unittest.main()
