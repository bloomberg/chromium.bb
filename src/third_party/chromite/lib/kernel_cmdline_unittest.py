# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Test the kernel_cmdline module."""

from __future__ import print_function

import collections

from chromite.lib import cros_test_lib
from chromite.lib import kernel_cmdline

# pylint: disable=protected-access

# A partial, but sufficiently complicated command line.  DmConfigTest uses
# more complicated configs, with multiple devices.  DM is split out here for
# CommandLineTest.test*DmConfig().
DM = (
    '1 vroot none ro 1,0 3891200 verity payload=PARTUUID=%U/PARTNROFF=1 '
    'hashtree=PARTUUID=%U/PARTNROFF=1 hashstart=3891200 alg=sha1 '
    'root_hexdigest=9999999999999999999999999999999999999999 '
    'salt=bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb')
CMDLINE = (
    'console= loglevel=7 init=/sbin/init cros_secure oops=panic panic=-1 '
    'root=/dev/dm-0 rootwait ro dm_verity.error_behavior=3 '
    'dm_verity.max_bios=-1 dm_verity.dev_wait=1 dm="' + DM +
    '" noinitrd cros_debug vt.global_cursor_default=0 kern_guid=%U '
    '-- run_level=3')


class KernelArgTest(cros_test_lib.TestCase):
  """Test KernelArg."""

  def testKeyOnly(self):
    """Expands arg-only arg."""
    kv = kernel_cmdline.KernelArg('arg', None)
    self.assertEqual('arg', kv.arg)
    self.assertEqual(None, kv.value)
    self.assertEqual('arg', kv.Format())

  def testKeyEqual(self):
    """Expands arg= arg."""
    kv = kernel_cmdline.KernelArg('arg', '')
    self.assertEqual('arg', kv.arg)
    self.assertEqual('', kv.value)
    self.assertEqual('arg=', kv.Format())

  def testKernelArg(self):
    """Expands arg=value arg."""
    kv = kernel_cmdline.KernelArg('arg', 'value')
    self.assertEqual('arg', kv.arg)
    self.assertEqual('value', kv.value)
    self.assertEqual('arg=value', kv.Format())

  def testRejectsBadValue(self):
    """Rejects non-string values."""
    kv = [kernel_cmdline.KernelArg('b', None)]
    with self.assertRaises(ValueError):
      kernel_cmdline.KernelArg('a', kv)
    with self.assertRaises(ValueError):
      kernel_cmdline.KernelArg('a', 1)
    # Test various forms of bad quoting.
    with self.assertRaises(ValueError):
      kernel_cmdline.KernelArg('a', '"aaa"aaaa"')
    with self.assertRaises(ValueError):
      kernel_cmdline.KernelArg('a', 'aaa"aaaa')
    with self.assertRaises(ValueError):
      kernel_cmdline.KernelArg('a', 'aaa "a" aaa')
    with self.assertRaises(ValueError):
      kernel_cmdline.KernelArg('a', 'aaa"')
    with self.assertRaises(ValueError):
      kernel_cmdline.KernelArg('a', '"aaaa')

  def testAddsQuotes(self):
    """Test that init adds double-quotes when needed."""
    kv1 = kernel_cmdline.KernelArg('d', '9 9')
    self.assertEqual('d', kv1.arg)
    # Value is unmodified.
    self.assertEqual('9 9', kv1.value)
    self.assertEqual('d="9 9"', kv1.Format())

    kv2 = kernel_cmdline.KernelArg('d', '"a a"')
    self.assertEqual('d', kv2.arg)
    # Value is unmodified.
    self.assertEqual('"a a"', kv2.value)
    self.assertEqual('d="a a"', kv2.Format())

  def testEqual(self):
    """Test __eq__()."""
    kv1 = kernel_cmdline.KernelArg('a', 'b')
    kv2 = kernel_cmdline.KernelArg('a', 'b')
    kv3 = kernel_cmdline.KernelArg('b', 'c')
    kv4 = kernel_cmdline.KernelArg('a', '9 9')
    kv5 = kernel_cmdline.KernelArg('a', '"9 9"')
    self.assertTrue(kv1 == kv2)
    self.assertFalse(kv1 == kv3)
    self.assertFalse(kv1 == kv4)
    self.assertTrue(kv4 == kv5)

  def testNotEqual(self):
    """Test __ne__()."""
    kv1 = kernel_cmdline.KernelArg('a', 'b')
    kv2 = kernel_cmdline.KernelArg('a', 'b')
    kv3 = kernel_cmdline.KernelArg('b', 'c')
    kv4 = kernel_cmdline.KernelArg('a', '9 9')
    kv5 = kernel_cmdline.KernelArg('a', '"9 9"')
    self.assertFalse(kv1 != kv2)
    self.assertTrue(kv1 != kv3)
    self.assertTrue(kv1 != kv4)
    self.assertFalse(kv4 != kv5)


class KernelArgListTest(cros_test_lib.TestCase):
  """Test KernelArgList()."""

  def testSimple(self):
    """Test a simple command line string."""
    expected = [
        kernel_cmdline.KernelArg('a', None),
        kernel_cmdline.KernelArg('b', ''),
        kernel_cmdline.KernelArg('c', 'd'),
        kernel_cmdline.KernelArg('e.f', '"d e"'),
        kernel_cmdline.KernelArg('a', None),
        kernel_cmdline.KernelArg('--', None),
        kernel_cmdline.KernelArg('x', None),
        kernel_cmdline.KernelArg('y', ''),
        kernel_cmdline.KernelArg('z', 'zz'),
        kernel_cmdline.KernelArg('y', None)]
    kl = kernel_cmdline.KernelArgList('a b= c=d e.f="d e" a -- x y= z=zz y')
    self.assertEqual(kl, expected)
    self.assertEqual(len(expected), len(kl))
    self.assertEqual(expected, [x for x in kl])

  def testSetDefaultValue(self):
    """Test that we can create an instance with no value given."""
    kl = kernel_cmdline.KernelArgList()
    self.assertEqual([], kl._data)
    self.assertEqual(0, len(kl))

  def testReturnsEmptyList(self):
    """Test that strings with no arguments return an empty list."""
    self.assertEqual([], kernel_cmdline.KernelArgList('  ')._data)

  def testForcesInternalType(self):
    """Test that the internal type is correctly forced."""
    expected = [
        kernel_cmdline.KernelArg('c', 'd'),
        kernel_cmdline.KernelArg('a', 'b'),
    ]
    # Pass in an OrderedDict with |expected| being the keys.
    kl = kernel_cmdline.KernelArgList(
        collections.OrderedDict((x, 0) for x in expected))
    self.assertEqual(type(kl._data), list)
    self.assertEqual(kl, expected)
    # Test len().
    self.assertEqual(2, len(kl))
    # Test __iter__().
    self.assertEqual(expected, [x for x in kl])

  def testRejectsInvalidInput(self):
    """Test that invalid command line strings are rejected."""
    # Non-KernelArg values.
    with self.assertRaises(ValueError):
      kernel_cmdline.KernelArgList({1:2, 3:4})
    # The first and non-first arguments are different in the re.
    with self.assertRaises(ValueError):
      kernel_cmdline.KernelArgList('=3')
    with self.assertRaises(ValueError):
      kernel_cmdline.KernelArgList('a =3')
    # Various bad quote usages.
    with self.assertRaises(ValueError):
      kernel_cmdline.KernelArgList('a b="foo"3 c')
    with self.assertRaises(ValueError):
      kernel_cmdline.KernelArgList('a b=" c')

  def testEqual(self):
    """Test __eq__()."""
    kv1 = kernel_cmdline.KernelArgList('a b= c')
    kv2 = kernel_cmdline.KernelArgList('a b= c')
    kv3 = kernel_cmdline.KernelArgList('a b= c d')
    self.assertTrue(kv1 == kv2)
    self.assertTrue(kv1 == kv2._data)
    self.assertFalse(kv1 == kv3)

  def testNotEqual(self):
    """Test __ne__()."""
    kv1 = kernel_cmdline.KernelArgList('a b= c')
    kv2 = kernel_cmdline.KernelArgList('a b= c')
    kv3 = kernel_cmdline.KernelArgList('a b= c d')
    self.assertFalse(kv1 != kv2)
    self.assertFalse(kv1 != kv2._data)
    self.assertTrue(kv1 != kv3)

  def testAdd(self):
    """Adding two KernelArgLists yields the correct KernelArgList."""
    kv1 = kernel_cmdline.KernelArgList('a b')
    kv2 = kernel_cmdline.KernelArgList('a d')
    res = kv1 + kv2
    self.assertEqual(type(res), kernel_cmdline.KernelArgList)
    self.assertEqual(res, kernel_cmdline.KernelArgList('a b a d'))

  def testIadd(self):
    """Test that += yields the correct KernelArgList."""
    kv1 = kernel_cmdline.KernelArgList('a b')
    kv2 = kernel_cmdline.KernelArgList('a d')
    kv1 += kv2
    self.assertEqual(type(kv1), kernel_cmdline.KernelArgList)
    self.assertEqual(kv1, kernel_cmdline.KernelArgList('a b a d'))

  def testContains(self):
    """Accepts KernelArg."""
    kv1 = kernel_cmdline.KernelArg('a', None)
    kv2 = kernel_cmdline.KernelArg('arg', 'value')
    kv3 = kernel_cmdline.KernelArg('z', None)
    kl = kernel_cmdline.KernelArgList('a arg=value b c')
    self.assertTrue(kv1 in kl)
    self.assertTrue(kv2 in kl)
    self.assertFalse(kv3 in kl)

  def testContainsAcceptsString(self):
    """Accepts KernelArg."""
    kl = kernel_cmdline.KernelArgList('a arg=value b c')
    self.assertTrue('a' in kl)
    self.assertTrue('arg' in kl)
    self.assertFalse('z' in kl)

  def testDelitem(self):
    """Test del."""
    kl = kernel_cmdline.KernelArgList('a b=3 c d e')
    del kl[0]
    del kl['b']
    with self.assertRaises(KeyError):
      del kl['z']
    self.assertEqual(kl, kernel_cmdline.KernelArgList('c d e'))

  def testDelslice(self):
    """Test del."""
    kl = kernel_cmdline.KernelArgList('a b=3 c d e')
    del kl[1:3]
    self.assertEqual(kl, kernel_cmdline.KernelArgList('a d e'))

  def testGetslice(self):
    """Test that __getslice__ works."""
    kl = kernel_cmdline.KernelArgList('a b c d')
    sl = kl[1:3]
    self.assertEqual(type(sl), kernel_cmdline.KernelArgList)
    self.assertEqual(kernel_cmdline.KernelArgList('a b c d')[1:3],
                     kernel_cmdline.KernelArgList('b c'))

  def testGetitemAcceptsInt(self):
    """Test that __getitem__ works with an integer index."""
    self.assertEqual(
        kernel_cmdline.KernelArgList('a b=3 c d').__getitem__(1),
        kernel_cmdline.KernelArg('b', '3'))

  def testGetitemAcceptsStr(self):
    """Test that __getitem__ works with a str."""
    self.assertEqual(
        kernel_cmdline.KernelArgList('a b=3 c d').__getitem__('b'),
        kernel_cmdline.KernelArg('b', '3'))

  def testGetItemRaisesKeyError(self):
    """Test that __getitem__ raises KeyError on invalid key."""
    kv = kernel_cmdline.KernelArgList('a x y d')
    with self.assertRaises(KeyError):
      kv.__getitem__('z')

  def testSetslice(self):
    """Test that __setslice__ works."""
    kv = kernel_cmdline.KernelArgList('a x y d')
    # Test setting a slice to a KernelArgList.
    ins = kernel_cmdline.KernelArgList('b c')
    kv[1:3] = ins
    self.assertEqual(kernel_cmdline.KernelArgList('a b c d'), kv)
    # Test setting a slice to a list of KernelArg.
    kv[1:2] = [kernel_cmdline.KernelArg('a', '3')]
    self.assertEqual(kernel_cmdline.KernelArgList('a a=3 c d'), kv)
    # Test setting a slice to a string.
    kv[1:2] = 'x y=4'
    self.assertEqual(kernel_cmdline.KernelArgList('a x y=4 c d'), kv)

  def testSetitemAcceptsInt(self):
    """Test that __setitem__ works with an integer index."""
    kv = kernel_cmdline.KernelArgList('a b=3 c d')
    new_val = kernel_cmdline.KernelArg('b', '4')
    kv[1] = new_val
    self.assertEqual(kv[1], new_val)

  def testSetitemAcceptsStr(self):
    """Test that __setitem__ works with a str."""
    kv = kernel_cmdline.KernelArgList('a b=3 c d')
    new_val = kernel_cmdline.KernelArg('b', '4')
    kv['b'] = new_val
    self.assertEqual(kv[1], new_val)

  def testSetitemAppendsWithNewKeyStr(self):
    """Test that __setitem__ appends with a new key (str)."""
    kv = kernel_cmdline.KernelArgList('a b=3 c d')
    new_val = kernel_cmdline.KernelArg('y', '4')
    kv.__setitem__('y', new_val)
    self.assertEqual(kv[4], new_val)

  def testSetitemRejectsBadValues(self):
    """Test that __setitem__ rejects bad values."""
    kv = kernel_cmdline.KernelArgList('a b=3 c d')
    with self.assertRaises(ValueError):
      # Strings are not KernelArgs
      kv['foo'] = 'bar'
    with self.assertRaises(ValueError):
      # Int is not KernelArg
      kv['foo'] = 1

  def testInsert(self):
    """Test that insert() works."""
    kv = kernel_cmdline.KernelArgList('a b=3 c d')
    kv.insert(1, kernel_cmdline.KernelArg('f', None))
    kv.insert('c', kernel_cmdline.KernelArg('g', None))
    with self.assertRaises(KeyError):
      kv.insert('z', kernel_cmdline.KernelArg('h', None))
    expected = kernel_cmdline.KernelArgList([
        kernel_cmdline.KernelArg('a', None),
        kernel_cmdline.KernelArg('f', None),
        kernel_cmdline.KernelArg('b', '3'),
        kernel_cmdline.KernelArg('g', None),
        kernel_cmdline.KernelArg('c', None),
        kernel_cmdline.KernelArg('d', None)])
    self.assertEqual(expected, kv)
    with self.assertRaises(ValueError):
      kv.insert('z', 'badval')
    # Verify that KernelArgList is a bad value.
    with self.assertRaises(ValueError):
      kv.insert('z', expected)

  def testFormat(self):
    """Test that Format works."""
    self.assertEqual('a x= b=c d',
                     kernel_cmdline.KernelArgList('a x= b=c d').Format())

  def testIndex(self):
    """Test that index finds the correct thing."""
    kv = kernel_cmdline.KernelArgList('a b=c d e')
    self.assertEqual(1, kv.index(1))
    self.assertEqual(1, kv.index('b'))
    self.assertEqual(None, kv.index('z'))

  def testget(self):
    """Test that Get returns the correct value for all key types."""
    kv = kernel_cmdline.KernelArgList('a b=c d e')
    self.assertEqual(kernel_cmdline.KernelArg('d', None), kv.get(2))
    self.assertEqual(kernel_cmdline.KernelArg('d', None), kv.get('d'))
    self.assertEqual('default', kv.get('z', default='default'))

  def testUpdateAcceptsKwargs(self):
    """Test update() with kwargs."""
    kv1 = kernel_cmdline.KernelArgList('a b c')
    kv1.update(b='f')
    kv2 = kernel_cmdline.KernelArgList('a b=f c')
    self.assertEqual(kv2, kv1)

  def testUpdateAcceptsDict(self):
    """Test update() accepts a dict."""
    kl1 = kernel_cmdline.KernelArgList('a=b')
    other = {'arg': 'value'}
    kl1.update(other)
    expected = kernel_cmdline.KernelArgList('a=b arg=value')
    self.assertEqual(expected, kl1)

  def testUpdateAcceptsKernelArgList(self):
    """Test update() accepts a KernelArg."""
    kl1 = kernel_cmdline.KernelArgList('a=b')
    kl2 = kernel_cmdline.KernelArgList('arg=value')
    kl1.update(kl2)
    expected = kernel_cmdline.KernelArgList('a=b arg=value')
    self.assertEqual(expected, kl1)

  def testUpdateAcceptsSetKernelArgUsage(self):
    kl1 = kernel_cmdline.KernelArgList('a dm="foo baz" b')
    kl1.update({'dm': DM})
    expected = kernel_cmdline.KernelArgList('a dm="%s" b' % DM)
    self.assertEqual(expected, kl1)

  def testUpdateAppends(self):
    """Test update() appends new arg."""
    kv = kernel_cmdline.KernelArgList('a b c')
    kv.update(kernel_cmdline.KernelArgList('d=99'), e='zz')
    expected = kernel_cmdline.KernelArgList('a b c d=99 e=zz')
    self.assertEqual(expected, kv)


class CommandLineTest(cros_test_lib.MockTestCase):
  """Test the CommandLine class."""

  def testSimple(self):
    """Test a simple command line string."""
    expected_kern = [
        kernel_cmdline.KernelArg('a', None),
        kernel_cmdline.KernelArg('b', ''),
        kernel_cmdline.KernelArg('c', 'd'),
        kernel_cmdline.KernelArg('e.f', 'd'),
        kernel_cmdline.KernelArg('a', None)]
    expected_init = [
        kernel_cmdline.KernelArg('x', None),
        kernel_cmdline.KernelArg('y', ''),
        kernel_cmdline.KernelArg('z', 'zz'),
        kernel_cmdline.KernelArg('y', None)]
    cmd = kernel_cmdline.CommandLine('a b= c=d e.f=d a -- x y= z=zz y')
    self.assertEqual(cmd.kern_args, expected_kern)
    self.assertEqual(cmd.init_args, expected_init)

  def testEmptyInit(self):
    """Test that 'a --' behaves as expected."""
    expected_kern = [kernel_cmdline.KernelArg('a', None)]
    expected_init = []
    cmd = kernel_cmdline.CommandLine('a --')
    self.assertEqual(expected_kern, cmd.kern_args)
    self.assertEqual(expected_init, cmd.init_args)

  def testEmptyKern(self):
    """Test that '-- a' behaves as expected."""
    expected_kern = []
    expected_init = [kernel_cmdline.KernelArg('a', None)]
    cmd = kernel_cmdline.CommandLine('-- a')
    self.assertEqual(expected_kern, cmd.kern_args)
    self.assertEqual(expected_init, cmd.init_args)

  def testEmptyArg(self):
    """Test that '' behaves as expected."""
    expected_kern = []
    expected_init = []
    cmd = kernel_cmdline.CommandLine('')
    self.assertEqual(expected_kern, cmd.kern_args)
    self.assertEqual(expected_init, cmd.init_args)

  def testEqual(self):
    """Test that CommandLine equal compare works."""
    # Confirm that != is False, and == is True
    self.assertFalse(kernel_cmdline.CommandLine('a b -- d e') !=
                     kernel_cmdline.CommandLine('a b -- d e'))
    self.assertTrue(kernel_cmdline.CommandLine('a b -- d e') ==
                    kernel_cmdline.CommandLine('a b -- d e'))

  def testNotEqual(self):
    """Test that CommandLine equal compare works."""
    # Confirm that == is False, and != is True
    self.assertFalse(kernel_cmdline.CommandLine('a b -- d e') ==
                     kernel_cmdline.CommandLine('a b -- d f'))
    self.assertTrue(kernel_cmdline.CommandLine('a b -- d e') !=
                    kernel_cmdline.CommandLine('a b -- d f'))

  def testDashesOnly(self):
    """Test that '--' behaves as expected."""
    expected_kern = []
    expected_init = []
    cmd = kernel_cmdline.CommandLine('--')
    self.assertEqual(expected_kern, cmd.kern_args)
    self.assertEqual(expected_init, cmd.init_args)

  def testExpandsDm(self):
    """Test that we do not expand dm="..."."""
    expected_kern = [
        kernel_cmdline.KernelArg('a', None),
        kernel_cmdline.KernelArg('dm', '"1 vroot b=c 1,0 1200"'),
        kernel_cmdline.KernelArg('c.d', 'e')]
    expected_init = [
        kernel_cmdline.KernelArg('dm', '"not split"'),
        kernel_cmdline.KernelArg('a', None)]
    cmd = kernel_cmdline.CommandLine(
        'a dm="1 vroot b=c 1,0 1200" c.d=e -- dm="not split" a')
    self.assertEqual(expected_kern, cmd.kern_args)
    self.assertEqual(expected_init, cmd.init_args)

  def testGetKernelParameter(self):
    """Test GetKernelParameter calls Get."""
    get = self.PatchObject(kernel_cmdline.KernelArgList, 'get')
    cmd = kernel_cmdline.CommandLine('a b c b=3')
    cmd.GetKernelParameter(1)
    get.assert_called_once_with(1, default=None)

  def testGetKernelParameterPassesDefault(self):
    """Test GetKernelParameter calls Get with default=."""
    get = self.PatchObject(kernel_cmdline.KernelArgList, 'get')
    cmd = kernel_cmdline.CommandLine('a b c b=3')
    cmd.GetKernelParameter(1, default=3)
    get.assert_called_once_with(1, default=3)

  def testSetKernelParameter(self):
    """Test SetKernelParameter calls update."""
    mock_update = self.PatchObject(kernel_cmdline.KernelArgList, 'update')
    cmd = kernel_cmdline.CommandLine('a b c b=3')
    cmd.SetKernelParameter('d', 'e')
    mock_update.assert_called_once_with({'d':'e'})

  def testFormat(self):
    """Test that the output is correct."""
    self.assertEqual(CMDLINE, kernel_cmdline.CommandLine(CMDLINE).Format())

  def testGetDmConfig(self):
    """Test that GetDmConfig returns the DmConfig we expect."""
    cmd = kernel_cmdline.CommandLine(CMDLINE)
    dm = kernel_cmdline.DmConfig(DM)
    self.assertEqual(dm, cmd.GetDmConfig())

  def testGetDmConfigHandlesMissing(self):
    """Test that GetDmConfig works with no dm= parameter."""
    cmd = kernel_cmdline.CommandLine('a b')
    self.assertEqual(None, cmd.GetDmConfig())

  def testSetDmConfig(self):
    """Test that SetDmConfig sets the dm= parameter."""
    cmd = kernel_cmdline.CommandLine('a -- b')
    cmd.SetDmConfig(kernel_cmdline.DmConfig(DM))
    self.assertEqual(kernel_cmdline.KernelArg('dm', DM), cmd.kern_args[1])

  def testSetDmConfigAcceptsNone(self):
    """Test that SetDmConfig deletes the dm= parameter when set to None."""
    cmd = kernel_cmdline.CommandLine('a dm="0 vroot none 0" -- b')
    cmd.SetDmConfig(None)
    self.assertEqual(None, cmd.GetDmConfig())
    self.assertFalse('dm' in cmd.kern_args)


class DmConfigTest(cros_test_lib.TestCase):
  """Test DmConfig."""

  def testParses(self):
    """Verify that DmConfig correctly parses the config from DmDevice."""
    device_data = [
        ['vboot uuid ro 1', '0 1 verity arg1 arg2'],
        ['vblah uuid2 ro 2',
         '100 9 stripe larg1=val1',
         '200 10 stripe larg2=val2'],
        ['vroot uuid3 ro 1', '99 3 linear larg1 larg2']]
    dm_arg = ', '.join(','.join(x for x in data) for data in device_data)
    devices = [kernel_cmdline.DmDevice(x) for x in device_data]
    dc = kernel_cmdline.DmConfig('%d %s' % (len(device_data), dm_arg))
    self.assertEqual(len(device_data), dc.num_devices)
    self.assertEqual(devices, list(dc.devices.values()))

  def testIgnoresQuotes(self):
    """Verify that DmConfig works with quoted string."""
    device_data = [
        ['vboot uuid ro 1', '0 1 verity arg1 arg2'],
        ['vblah uuid2 ro 2',
         '100 9 stripe larg1=val1',
         '200 10 stripe larg2=val2'],
        ['vroot uuid3 ro 1', '99 3 linear larg1 larg2']]
    dm_arg = ', '.join(','.join(x for x in data) for data in device_data)
    devices = [kernel_cmdline.DmDevice(x) for x in device_data]
    dc = kernel_cmdline.DmConfig('"%d %s"' % (len(device_data), dm_arg))
    self.assertEqual(len(device_data), dc.num_devices)
    self.assertEqual(devices, list(dc.devices.values()))

  def testFormats(self):
    """Verify that DmConfig recreates its input string."""
    device_data = [
        ['vboot uuid ro 1', '0 1 verity arg1 arg2'],
        ['vblah uuid2 ro 2',
         '100 9 stripe larg1=val1',
         '200 10 stripe larg2=val2'],
        ['vroot uuid3 ro 1', '99 3 linear larg1 larg2']]
    dm_arg = ', '.join(','.join(x for x in data) for data in device_data)
    dm_config_val = '%d %s' % (len(device_data), dm_arg)
    dc = kernel_cmdline.DmConfig(dm_config_val)
    self.assertEqual(dm_config_val, dc.Format())

  def testEqual(self):
    """Test that equal instances are equal."""
    arg = '2 v1 u1 f1 1,0 1 t1 a1, v2 u2 f2 2,3 4 t2 a2,5 6 t3 a3'
    dc = kernel_cmdline.DmConfig(arg)
    self.assertEqual(dc, kernel_cmdline.DmConfig(arg))
    self.assertTrue(dc == kernel_cmdline.DmConfig(arg))
    # Also verify that != is False.
    self.assertFalse(dc != kernel_cmdline.DmConfig(arg))

  def testNotEqual(self):
    """Test that unequal instances are unequal."""
    # Start with duplicated instances, and change fields to verify that all the
    # fields matter.
    arg = '2 v1 u1 f1 1,0 1 t1 a1, v2 u2 f2 2,3 4 t2 a2,5 6 t3 a3'
    dc1 = kernel_cmdline.DmConfig(arg)
    self.assertNotEqual(dc1, '')
    dc2 = kernel_cmdline.DmConfig(arg)
    dc2.num_devices = 1
    self.assertNotEqual(dc1, dc2)
    dc3 = kernel_cmdline.DmConfig(arg)
    dc3.devices = []
    self.assertNotEqual(dc1, dc3)

  def testGetVerityArg(self):
    """Test that GetVerityArg works."""
    arg = ['v1 u1 f1 1', '0 1 verity a1 a2=v2']
    a1 = kernel_cmdline.KernelArg('a1', None)
    a2 = kernel_cmdline.KernelArg('a2', 'v2')
    dd = kernel_cmdline.DmDevice(arg)
    self.assertEqual(a1, dd.GetVerityArg('a1'))
    self.assertEqual(a2, dd.GetVerityArg('a2'))
    self.assertEqual(None, dd.GetVerityArg('a3', default=None))
    self.assertEqual(a1, dd.GetVerityArg('a3', default=a1))

  def testGetVerityArgMultiLine(self):
    """Test that GetVerityArg works."""
    arg = ['v1 u1 f1 3', '9 9 xx a3 a4=v4',
           '0 1 verity a1 a2=v2', '9 9 verity a3 a2=v4']
    a1 = kernel_cmdline.KernelArg('a1', None)
    a2 = kernel_cmdline.KernelArg('a2', 'v2')
    dd = kernel_cmdline.DmDevice(arg)
    self.assertEqual(a1, dd.GetVerityArg('a1'))
    self.assertEqual(a2, dd.GetVerityArg('a2'))
    self.assertEqual(None, dd.GetVerityArg('a3'))
    self.assertEqual(None, dd.GetVerityArg('a9', default=None))
    self.assertEqual(a1, dd.GetVerityArg('a9', default=a1))

  def testUpdateVerityArg(self):
    """Test that UpdateVerityArg works."""
    arg = ['v1 u1 f1 1', '0 1 verity a1 a2=v2']
    dd = kernel_cmdline.DmDevice(arg)
    dd.UpdateVerityArg('a2', 'new')
    self.assertEqual(
        kernel_cmdline.KernelArg('a2', 'new'), dd.rows[0].args[1])
    dd.UpdateVerityArg('a3', None)
    self.assertEqual(
        kernel_cmdline.KernelArg('a3', None), dd.rows[0].args[2])

  def testUpdateVerityArgMultiLine(self):
    """Test that UpdateVerityArg works."""
    arg = ['v1 u1 f1 3', '9 9 xx a3 a4=v4',
           '0 1 verity a1 a2=v2', '9 9 verity a1 a2=v2 an']
    dd = kernel_cmdline.DmDevice(arg)
    a2 = kernel_cmdline.KernelArg('a2', 'new')
    dd.UpdateVerityArg('a2', 'new')
    self.assertEqual(a2, dd.rows[1].args[1])
    self.assertEqual(a2, dd.rows[2].args[1])
    a3 = kernel_cmdline.KernelArg('a3', None)
    dd.UpdateVerityArg('a3', None)
    self.assertEqual(a3, dd.rows[1].args[2])
    self.assertEqual(a3, dd.rows[2].args[3])


class DmDeviceTest(cros_test_lib.TestCase):
  """Test DmDevice."""

  def testParsesOneLine(self):
    """Verify that DmDevice correctly handles the results from DmLine."""
    lines = ['vboot none ro 1', '0 1 verity arg']
    dd = kernel_cmdline.DmDevice(lines)
    self.assertEqual('vboot', dd.name)
    self.assertEqual('none', dd.uuid)
    self.assertEqual('ro', dd.flags)
    self.assertEqual(1, dd.num_rows)
    self.assertEqual([kernel_cmdline.DmLine('0 1 verity arg')], dd.rows)

  def testParsesMultiLine(self):
    """Verify that DmDevice correctly handles multiline device."""
    lines = ['vboot none ro 2', '0 1 verity arg', '9 10 type a2']
    dd = kernel_cmdline.DmDevice(lines)
    self.assertEqual('vboot', dd.name)
    self.assertEqual('none', dd.uuid)
    self.assertEqual('ro', dd.flags)
    self.assertEqual(2, dd.num_rows)
    self.assertEqual([kernel_cmdline.DmLine('0 1 verity arg'),
                      kernel_cmdline.DmLine('9 10 type a2')], dd.rows)

  def testParsesIgnoresExcessRows(self):
    """Verify that DmDevice ignores excess rows."""
    lines = ['vboot none ro 2', '0 1 verity arg', '9 10 type a2', '4 5 t a3']
    dd = kernel_cmdline.DmDevice(lines)
    self.assertEqual('vboot', dd.name)
    self.assertEqual('none', dd.uuid)
    self.assertEqual('ro', dd.flags)
    self.assertEqual(2, dd.num_rows)
    self.assertEqual([kernel_cmdline.DmLine('0 1 verity arg'),
                      kernel_cmdline.DmLine('9 10 type a2')], dd.rows)

  def testEqual(self):
    """Test that equal instances are equal."""
    dd = kernel_cmdline.DmDevice(['v u f 1', '0 1 typ a'])
    self.assertEqual(dd, kernel_cmdline.DmDevice(['v u f 1', '0 1 typ a']))
    self.assertTrue(dd == kernel_cmdline.DmDevice(['v u f 1', '0 1 typ a']))
    self.assertFalse(dd != kernel_cmdline.DmDevice(['v u f 1', '0 1 typ a']))

  def testMultiLineEqual(self):
    """Test that equal instances are equal."""
    dd = kernel_cmdline.DmDevice(['v u f 2', '0 1 typ a', '2 3 t b'])
    self.assertEqual(
        dd, kernel_cmdline.DmDevice(['v u f 2', '0 1 typ a', '2 3 t b']))
    self.assertFalse(
        dd != kernel_cmdline.DmDevice(['v u f 2', '0 1 typ a', '2 3 t b']))

  def testNotEqual(self):
    """Test that unequal instances are unequal."""
    dd = kernel_cmdline.DmDevice(['v u f 2', '0 1 typ a', '2 3 t b'])
    self.assertNotEqual(dd, '')
    self.assertNotEqual(
        dd, kernel_cmdline.DmDevice(['x u f 2', '0 1 typ a', '2 3 t b']))
    self.assertNotEqual(
        dd, kernel_cmdline.DmDevice(['v x f 2', '0 1 typ a', '2 3 t b']))
    self.assertNotEqual(
        dd, kernel_cmdline.DmDevice(['v u x 2', '0 1 typ a', '2 3 t b']))
    self.assertNotEqual(
        dd, kernel_cmdline.DmDevice(['v u f 1', '0 1 typ a', '2 3 t b']))
    self.assertNotEqual(
        dd, kernel_cmdline.DmDevice(['v u f 2', '9 1 typ a', '2 3 t b']))


class DmLineTest(cros_test_lib.TestCase):
  """Test DmLine."""

  def testParses(self):
    """Verify that DmLine correctly parses a line, and returns it."""
    text = '0 1 verity a1 a2=v2 a3'
    dl = kernel_cmdline.DmLine(text)
    self.assertEqual(0, dl.start)
    self.assertEqual(1, dl.num)
    self.assertEqual('verity', dl.target_type)
    self.assertEqual(kernel_cmdline.KernelArgList('a1 a2=v2 a3'), dl.args)
    self.assertEqual(text, dl.Format())

  def testAllowsWhitespace(self):
    """Verify that leading/trailing whitespace is ignored."""
    text = '0 1 verity a1 a2=v2 a3'
    dl = kernel_cmdline.DmLine(' %s ' % text)
    self.assertEqual(0, dl.start)
    self.assertEqual(1, dl.num)
    self.assertEqual('verity', dl.target_type)
    self.assertEqual(kernel_cmdline.KernelArgList('a1 a2=v2 a3'), dl.args)
    self.assertEqual(text, dl.Format())

  def testEqual(self):
    """Test that equal instances are equal."""
    dl = kernel_cmdline.DmLine('0 1 verity a1 a2=v2 a3')
    self.assertEqual(dl, kernel_cmdline.DmLine('0 1 verity a1 a2=v2 a3'))
    self.assertFalse(dl != kernel_cmdline.DmLine('0 1 verity a1 a2=v2 a3'))

  def testNotEqual(self):
    """Test that unequal instances are unequal."""
    dl = kernel_cmdline.DmLine('0 1 verity a1 a2=v2 a3')
    self.assertNotEqual(dl, '')
    self.assertNotEqual(dl, kernel_cmdline.DmLine('1 1 verity a1 a2=v2 a3'))
    self.assertNotEqual(dl, kernel_cmdline.DmLine('0 2 verity a1 a2=v2 a3'))
    self.assertNotEqual(dl, kernel_cmdline.DmLine('0 1 verit  a1 a2=v2 a3'))
    self.assertNotEqual(dl, kernel_cmdline.DmLine('0 1 verity aN a2=v2 a3'))
