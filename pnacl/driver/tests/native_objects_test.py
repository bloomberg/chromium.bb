#!/usr/bin/python
# Copyright (c) 2013 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

from driver_env import env
import driver_test_utils
import driver_tools
import elftools
import filetype
import re

class TestNativeDriverOptions(driver_test_utils.DriverTesterCommon):
  def setUp(self):
    super(TestNativeDriverOptions, self).setUp()
    driver_test_utils.ApplyTestEnvOverrides(env)
    if not driver_test_utils.CanRunHost():
      self.skipTest("Cannot run host binaries")

  def getFakeSourceFile(self):
    with self.getTemp(suffix='.c', close=False) as s:
      s.write('int main(int argc, char* argv[]) { return 0; }')
      s.close()
      return s

  def getBitcodeArch(self, filename):
    with self.getTemp(suffix='.ll') as ll:
      driver_tools.RunDriver('dis', [filename, '-o', ll.name])
      with open(ll.name) as f:
        disassembly = f.read()
        match = re.search(r'target triple = "(.+)"', disassembly)
        if not match:
          return None
        triple = match.group(1)
        return driver_tools.ParseTriple(triple)

  def test_bc_objects(self):
    s = self.getFakeSourceFile()
    with self.getTemp(suffix='.o') as obj:
      # Test that clang with "normal" args results in a portable bitcode object
      driver_tools.RunDriver('clang', [s.name, '-c', '-o', obj.name])
      self.assertTrue(filetype.IsLLVMBitcode(obj.name))
      self.assertEqual(self.getBitcodeArch(obj.name), 'le32')

      # Test that the --target flag produces biased bitcode objects
      test_args = [
          ('armv7',  'ARM',    '-gnueabihf', ['-mfloat-abi=hard']),
          ('i686',   'X8632',  '',           []),
          ('x86_64', 'X8664',  '',           []),
          ('mips',   'MIPS32', '',           []),
      ]
      for (target, arch, target_extra, cmd_extra) in test_args:
        target_arg = '--target=%s-unknown-nacl%s' % (target, target_extra)
        driver_tools.RunDriver('clang',
          [s.name, target_arg, '-c', '-o', obj.name] + cmd_extra)
        self.assertTrue(filetype.IsLLVMBitcode(obj.name))
        self.assertEqual(self.getBitcodeArch(obj.name), arch)

  def test_compile_native_objects(self):
    s = self.getFakeSourceFile()
    with self.getTemp(suffix='.o') as obj:
      # TODO(dschuff): Use something more descriptive instead of -arch
      # (i.e. something that indicates that a translation is requested)
      # and remove pnacl-allow-translate
      driver_tools.RunDriver('clang',
          [s.name, '-c', '-o', obj.name, '--target=x86_64-unknown-nacl',
           '-arch', 'x86-64', '--pnacl-allow-translate'])
      self.assertTrue(filetype.IsNativeObject(obj.name))
      self.assertEqual(elftools.GetELFHeader(obj.name).arch, 'X8664')

      driver_tools.RunDriver('clang',
          [s.name, '-c', '-o', obj.name, '--target=i686-unknown-nacl',
           '-arch', 'x86-32', '--pnacl-allow-translate'])
      self.assertTrue(filetype.IsNativeObject(obj.name))
      self.assertEqual(elftools.GetELFHeader(obj.name).arch, 'X8632')

      driver_tools.RunDriver('clang',
          [s.name, '-c', '-o', obj.name,
           '--target=armv7-unknown-nacl-gnueabihf', '-mfloat-abi=hard',
           '-arch', 'arm', '--pnacl-allow-translate'])
      self.assertTrue(filetype.IsNativeObject(obj.name))
      self.assertEqual(elftools.GetELFHeader(obj.name).arch, 'ARM')

      # TODO(dschuff): This should be an error.
      driver_tools.RunDriver('clang',
          [s.name, '-c', '-o', obj.name, '--target=x86_64-unknown-nacl',
           '-arch', 'x86-32', '--pnacl-allow-translate'])
      self.assertTrue(filetype.IsNativeObject(obj.name))
      self.assertEqual(elftools.GetELFHeader(obj.name).arch, 'X8632')
