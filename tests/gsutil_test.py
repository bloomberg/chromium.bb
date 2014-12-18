#!/usr/bin/env python
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Test gsutil.py."""


import __builtin__
import unittest
import hashlib
import zipfile
import shutil
import sys
import base64
import tempfile
import json
import os
import urllib2


# Add depot_tools to path
THIS_DIR = os.path.dirname(os.path.abspath(__file__))
DEPOT_TOOLS_DIR = os.path.dirname(THIS_DIR)
sys.path.append(DEPOT_TOOLS_DIR)

import gsutil


class TestError(Exception):
  pass


class Buffer(object):
  def __init__(self, data=None):
    self.data = data or ''

  def write(self, buf):
    self.data += buf

  def read(self, amount=None):
    if not amount:
      amount = len(self.data)
    result = self.data[:amount]
    self.data = self.data[amount:]
    return result


class FakeCall(object):
  def __init__(self):
    self.expectations = []

  def add_expectation(self, *args, **kwargs):
    returns = kwargs.pop('_returns', None)
    self.expectations.append((args, kwargs, returns))

  def __call__(self, *args, **kwargs):
    if not self.expectations:
      raise TestError('Got unexpected\n%s\n%s' % (args, kwargs))
    exp_args, exp_kwargs, exp_returns = self.expectations.pop(0)
    if args != exp_args or kwargs != exp_kwargs:
      message = 'Expected:\n  args: %s\n  kwargs: %s\n' % (exp_args, exp_kwargs)
      message += 'Got:\n  args: %s\n  kwargs: %s\n' % (args, kwargs)
      raise TestError(message)
    if isinstance(exp_returns, Exception):
      raise exp_returns
    return exp_returns


class GsutilUnitTests(unittest.TestCase):
  def setUp(self):
    self.fake = FakeCall()
    self.tempdir = tempfile.mkdtemp()
    self.old_urlopen = getattr(urllib2, 'urlopen')
    self.old_call = getattr(gsutil, 'call')
    setattr(urllib2, 'urlopen', self.fake)
    setattr(gsutil, 'call', self.fake)

  def tearDown(self):
    self.assertEqual(self.fake.expectations, [])
    shutil.rmtree(self.tempdir)
    setattr(urllib2, 'urlopen', self.old_urlopen)
    setattr(gsutil, 'call', self.old_call)

  def test_download_gsutil(self):
    version = '4.2'
    filename = 'gsutil_%s.zip' % version
    full_filename = os.path.join(self.tempdir, filename)
    fake_file = 'This is gsutil.zip'
    fake_file2 = 'This is other gsutil.zip'
    url = '%s%s' % (gsutil.GSUTIL_URL, filename)
    self.fake.add_expectation(url, _returns=Buffer(fake_file))

    self.assertEquals(
        gsutil.download_gsutil(version, self.tempdir), full_filename)
    with open(full_filename, 'r') as f:
      self.assertEquals(fake_file, f.read())

    metadata_url = gsutil.API_URL + filename
    md5_calc = hashlib.md5()
    md5_calc.update(fake_file)
    b64_md5 = base64.b64encode(md5_calc.hexdigest())
    self.fake.add_expectation(metadata_url, _returns=Buffer(json.dumps({
        'md5Hash': b64_md5
    })))
    self.assertEquals(
        gsutil.download_gsutil(version, self.tempdir), full_filename)
    with open(full_filename, 'r') as f:
      self.assertEquals(fake_file, f.read())
    self.assertEquals(self.fake.expectations, [])

    self.fake.add_expectation(metadata_url, _returns=Buffer(json.dumps({
        'md5Hash': base64.b64encode('aaaaaaa')  # Bad MD5
    })))
    self.fake.add_expectation(url, _returns=Buffer(fake_file2))
    self.assertEquals(
        gsutil.download_gsutil(version, self.tempdir), full_filename)
    with open(full_filename, 'r') as f:
      self.assertEquals(fake_file2, f.read())
    self.assertEquals(self.fake.expectations, [])

  def test_ensure_gsutil_full(self):
    version = '4.2'
    gsutil_dir = os.path.join(self.tempdir, 'gsutil_%s' % version, 'gsutil')
    gsutil_bin = os.path.join(gsutil_dir, 'gsutil')
    os.makedirs(gsutil_dir)

    self.fake.add_expectation(
        [sys.executable, gsutil_bin, 'version'], verbose=False,
        _returns=gsutil.SubprocessError())

    with open(gsutil_bin, 'w') as f:
      f.write('Foobar')
    zip_filename = 'gsutil_%s.zip' % version
    url = '%s%s' % (gsutil.GSUTIL_URL, zip_filename)
    _, tempzip = tempfile.mkstemp()
    fake_gsutil = 'Fake gsutil'
    with zipfile.ZipFile(tempzip, 'w') as zf:
      zf.writestr('gsutil/gsutil', fake_gsutil)
    with open(tempzip, 'rb') as f:
      self.fake.add_expectation(url, _returns=Buffer(f.read()))
    self.fake.add_expectation(
        [sys.executable, gsutil_bin, 'version'], verbose=False,
        _returns=gsutil.SubprocessError())

    # This should delete the old bin and rewrite it with 'Fake gsutil'
    self.assertRaises(
        gsutil.InvalidGsutilError, gsutil.ensure_gsutil, version, self.tempdir)
    self.assertTrue(os.path.isdir(os.path.join(self.tempdir, '.cache_dir')))
    self.assertTrue(os.path.exists(gsutil_bin))
    with open(gsutil_bin, 'r') as f:
      self.assertEquals(f.read(), fake_gsutil)
    self.assertEquals(self.fake.expectations, [])

  def test_ensure_gsutil_short(self):
    version = '4.2'
    gsutil_dir = os.path.join(self.tempdir, 'gsutil_%s' % version, 'gsutil')
    gsutil_bin = os.path.join(gsutil_dir, 'gsutil')
    os.makedirs(gsutil_dir)

    # Mock out call().
    self.fake.add_expectation(
        [sys.executable, gsutil_bin, 'version'], verbose=False, _returns=True)

    with open(gsutil_bin, 'w') as f:
      f.write('Foobar')
    self.assertEquals(
        gsutil.ensure_gsutil(version, self.tempdir), gsutil_bin)

if __name__ == '__main__':
  unittest.main()
