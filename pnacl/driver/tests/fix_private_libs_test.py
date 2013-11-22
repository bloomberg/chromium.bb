#!/usr/bin/python
# Copyright (c) 2013 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

""" Test that "FixPrivateLibs" function rearranges public and private
libraries in the right way. """

import unittest

from driver_env import env
import driver_test_utils
pnacl_ld = __import__("pnacl-ld")


class TestFixPrivateLibs(driver_test_utils.DriverTesterCommon):
  def setUp(self):
    super(TestFixPrivateLibs, self).setUp()
    driver_test_utils.ApplyTestEnvOverrides(env)

  def test_nop(self):
    """Test having no private libs at all -- nothing should happen.

    NOTE: The "user_libs" parameter can have flags as well as files
    and libraries. Basically it's just the commandline for the bitcode linker.
    Test that the flags and normal files aren't disturbed.
    """
    input_libs = ['--undefined=main',
                  'toolchain/pnacl_linux_x86/lib/crti.bc',
                  'foo.bc',
                  '--start-group',
                  'scons-out/lib/libnacl.a',
                  'toolchain/pnacl_linux_x86/usr/lib/libc.a',
                  '--end-group']
    expected_libs = list(input_libs)
    output_libs = pnacl_ld.FixPrivateLibs(input_libs)
    self.assertEqual(expected_libs, output_libs)

  def test_pthread_no_nacl(self):
    """Test having pthread_private but not nacl_sys_private.

    pthread_private should replace pthread. However we still keep the
    original instance of pthread_private where it is, which shouldn't hurt.

    Also test that libnacl.a stays the same, if libnacl_sys_private.a was
    never requested by the user. We can't pull in libnacl_sys_private.a
    spuriously, because chrome never builds libnacl_sys_private.a w/ gyp.
    """
    input_libs = ['foo.bc',
                  'scons-out/lib1/libpthread_private.a',
                  '--start-group',
                  'scons-out/lib1/libpthread.a',
                  'scons-out/lib2/libnacl.a',
                  'toolchain/pnacl_linux_x86/usr/lib/libc.a',
                  '--end-group']
    expected_libs = ['foo.bc',
                     'scons-out/lib1/libpthread_private.a',
                     '--start-group',
                     'scons-out/lib1/libpthread_private.a',
                     'scons-out/lib2/libnacl.a',
                     'toolchain/pnacl_linux_x86/usr/lib/libc.a',
                     '--end-group']
    output_libs = pnacl_ld.FixPrivateLibs(input_libs)
    self.assertEqual(expected_libs, output_libs)

  def test_dyncode_no_nacl_no_pthread(self):
    """Test having dyncode_private but not pthread_private or nacl_sys_private.

    Similar to pthread_no_nacl, but dyncode isn't a special library
    like libc or libpthread. Also, a chrome test uses dyncode_private,
    but not nacl_sys_private and not pthread_private, so don't accidentally
    pull those two in.
    """
    input_libs = ['foo.bc',
                  'scons-out/lib1/libnacl_dyncode_private.a',
                  '--start-group',
                  'scons-out/lib2/libpthread.a',
                  'scons-out/lib3/libnacl.a',
                  'toolchain/pnacl_linux_x86/usr/lib/libc.a',
                  '--end-group']
    expected_libs = ['foo.bc',
                     'scons-out/lib1/libnacl_dyncode_private.a',
                     '--start-group',
                     'scons-out/lib2/libpthread.a',
                     'scons-out/lib3/libnacl.a',
                     'toolchain/pnacl_linux_x86/usr/lib/libc.a',
                     '--end-group']
    output_libs = pnacl_ld.FixPrivateLibs(input_libs)
    self.assertEqual(expected_libs, output_libs)

  def test_nacl_private_no_pthread(self):
    """Test having nacl_sys_private but not pthread_private.

    Since libpthread is added as an implicit lib for libc++, for non-IRT
    programs we need to swap pthread with pthread_private even if it wasn't
    explicitly asked for. Otherwise libpthread will try to query the IRT
    and crash. Use nacl_sys_private as the signal for non-IRT programs.
    """
    input_libs = ['foo.bc',
                  'scons-out/lib1/libnacl_sys_private.a',
                  '--start-group',
                  'scons-out/lib2/libpthread.a',
                  'scons-out/lib1/libnacl.a',
                  'toolchain/pnacl_linux_x86/usr/lib/libc.a',
                  '--end-group']
    expected_libs = ['foo.bc',
                     'scons-out/lib1/libnacl_sys_private.a',
                     '--start-group',
                     'scons-out/lib2/libpthread_private.a',
                     'scons-out/lib1/libnacl_sys_private.a',
                     'scons-out/lib1/libnacl.a',
                     'toolchain/pnacl_linux_x86/usr/lib/libc.a',
                     '--end-group']
    output_libs = pnacl_ld.FixPrivateLibs(input_libs)
    self.assertEqual(expected_libs, output_libs)


  def test_pthread_and_nacl_1(self):
    """Test having both libnacl_sys_private and libpthread_private #1.

    In this case, yes we should touch libnacl_sys_private.
    """
    input_libs = ['foo.bc',
                  'scons-out/lib1/libnacl_sys_private.a',
                  'scons-out/lib2/libpthread_private.a',
                  '--start-group',
                  'scons-out/lib2/libpthread.a',
                  'scons-out/lib1/libnacl.a',
                  'toolchain/pnacl_linux_x86/usr/lib/libc.a',
                  '--end-group']
    expected_libs = ['foo.bc',
                     'scons-out/lib1/libnacl_sys_private.a',
                     'scons-out/lib2/libpthread_private.a',
                     '--start-group',
                     'scons-out/lib2/libpthread_private.a',
                     'scons-out/lib1/libnacl_sys_private.a',
                     'scons-out/lib1/libnacl.a',
                     'toolchain/pnacl_linux_x86/usr/lib/libc.a',
                     '--end-group']
    output_libs = pnacl_ld.FixPrivateLibs(input_libs)
    self.assertEqual(expected_libs, output_libs)

  def test_pthread_and_nacl_2(self):
    """Test having both libnacl_sys_private and libpthread_private #2.

    Try flipping the order of nacl and pthread to make sure the substitution
    still works.
    """
    input_libs = ['foo.bc',
                  'scons-out/lib1/libpthread_private.a',
                  'scons-out/lib2/libnacl_sys_private.a',
                  '--start-group',
                  'scons-out/lib2/libnacl.a',
                  'scons-out/lib1/libpthread.a',
                  'toolchain/pnacl_linux_x86/usr/lib/libc.a',
                  '--end-group']
    expected_libs = ['foo.bc',
                     'scons-out/lib1/libpthread_private.a',
                     'scons-out/lib2/libnacl_sys_private.a',
                     '--start-group',
                     'scons-out/lib2/libnacl_sys_private.a',
                     'scons-out/lib2/libnacl.a',
                     'scons-out/lib1/libpthread_private.a',
                     'toolchain/pnacl_linux_x86/usr/lib/libc.a',
                     '--end-group']
    output_libs = pnacl_ld.FixPrivateLibs(input_libs)
    self.assertEqual(expected_libs, output_libs)
