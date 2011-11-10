#!/usr/bin/env python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for gclient_scm.py."""

# pylint: disable=E1103

# Import before super_mox to keep valid references.
from os import rename
from shutil import rmtree
from subprocess import Popen, PIPE, STDOUT

import logging
import os
import sys
import tempfile
import unittest
import __builtin__

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from super_mox import mox, StdoutCheck, SuperMoxTestBase
from super_mox import TestCaseUtils

import gclient_scm
import subprocess2

# Shortcut since this function is used often
join = gclient_scm.os.path.join


class GCBaseTestCase(object):
  def assertRaisesError(self, msg, fn, *args, **kwargs):
    """Like unittest's assertRaises() but checks for Gclient.Error."""
    # pylint: disable=E1101
    try:
      fn(*args, **kwargs)
    except gclient_scm.gclient_utils.Error, e:
      self.assertEquals(e.args[0], msg)
    else:
      self.fail('%s not raised' % msg)


class BaseTestCase(GCBaseTestCase, SuperMoxTestBase):
  def setUp(self):
    SuperMoxTestBase.setUp(self)
    self.mox.StubOutWithMock(gclient_scm.gclient_utils, 'CheckCallAndFilter')
    self.mox.StubOutWithMock(gclient_scm.gclient_utils,
        'CheckCallAndFilterAndHeader')
    self.mox.StubOutWithMock(gclient_scm.gclient_utils, 'FileRead')
    self.mox.StubOutWithMock(gclient_scm.gclient_utils, 'FileWrite')
    self.mox.StubOutWithMock(gclient_scm.gclient_utils, 'RemoveDirectory')
    self._CaptureSVNInfo = gclient_scm.scm.SVN.CaptureInfo
    self.mox.StubOutWithMock(gclient_scm.scm.SVN, 'Capture')
    self.mox.StubOutWithMock(gclient_scm.scm.SVN, 'CaptureInfo')
    self.mox.StubOutWithMock(gclient_scm.scm.SVN, 'CaptureStatus')
    self.mox.StubOutWithMock(gclient_scm.scm.SVN, 'RunAndGetFileList')
    self.mox.StubOutWithMock(subprocess2, 'communicate')
    self.mox.StubOutWithMock(subprocess2, 'Popen')
    self._scm_wrapper = gclient_scm.CreateSCM
    gclient_scm.scm.SVN.current_version = None
    # Absolute path of the fake checkout directory.
    self.base_path = join(self.root_dir, self.relpath)

  def tearDown(self):
    SuperMoxTestBase.tearDown(self)


class SVNWrapperTestCase(BaseTestCase):
  class OptionsObject(object):
    def __init__(self, verbose=False, revision=None, force=False):
      self.verbose = verbose
      self.revision = revision
      self.manually_grab_svn_rev = True
      self.deps_os = None
      self.force = force
      self.reset = False
      self.nohooks = False
      # TODO(maruel): Test --jobs > 1.
      self.jobs = 1

  def Options(self, *args, **kwargs):
    return self.OptionsObject(*args, **kwargs)

  def setUp(self):
    BaseTestCase.setUp(self)
    self.url = self.SvnUrl()

  def testDir(self):
    members = [
        'FullUrlForRelativeUrl', 'GetRevisionDate', 'RunCommand',
        'cleanup', 'diff', 'pack', 'relpath', 'revert',
        'revinfo', 'runhooks', 'status', 'update',
        'updatesingle', 'url',
    ]

    # If you add a member, be sure to add the relevant test!
    self.compareMembers(self._scm_wrapper('svn://a'), members)

  def testUnsupportedSCM(self):
    args = ['gopher://foo', self.root_dir, self.relpath]
    exception_msg = 'No SCM found for url gopher://foo'
    self.assertRaisesError(exception_msg, self._scm_wrapper, *args)

  def testSVNFullUrlForRelativeUrl(self):
    self.url = 'svn://a/b/c/d'

    self.mox.ReplayAll()
    scm = self._scm_wrapper(url=self.url, root_dir=self.root_dir,
                            relpath=self.relpath)
    self.assertEqual(scm.FullUrlForRelativeUrl('/crap'), 'svn://a/b/crap')

  def testGITFullUrlForRelativeUrl(self):
    self.url = 'git://a/b/c/d'

    self.mox.ReplayAll()
    scm = self._scm_wrapper(url=self.url, root_dir=self.root_dir,
                            relpath=self.relpath)
    self.assertEqual(scm.FullUrlForRelativeUrl('/crap'), 'git://a/b/c/crap')

  def testRunCommandException(self):
    options = self.Options(verbose=False)
    gclient_scm.os.path.exists(join(self.base_path, '.git')).AndReturn(False)
    gclient_scm.os.path.exists(join(self.base_path, '.hg')).AndReturn(False)

    self.mox.ReplayAll()
    scm = self._scm_wrapper(url=self.url, root_dir=self.root_dir,
                            relpath=self.relpath)
    exception = "Unsupported argument(s): %s" % ','.join(self.args)
    self.assertRaisesError(exception, scm.RunCommand,
                           'update', options, self.args)

  def testRunCommandUnknown(self):
    # TODO(maruel): if ever used.
    pass

  def testRevertMissing(self):
    options = self.Options(verbose=True)
    gclient_scm.os.path.isdir(self.base_path).AndReturn(False)
    gclient_scm.os.path.exists(self.base_path).AndReturn(False)
    gclient_scm.scm.SVN.Capture(['--version']
        ).AndReturn('svn, version 1.5.1 (r32289)')
    # It'll to a checkout instead.
    gclient_scm.os.path.exists(join(self.base_path, '.git')).AndReturn(False)
    gclient_scm.os.path.exists(join(self.base_path, '.hg')).AndReturn(False)
    # Checkout.
    gclient_scm.os.path.exists(self.base_path).AndReturn(False)
    parent = gclient_scm.os.path.dirname(self.base_path)
    gclient_scm.os.path.exists(parent).AndReturn(False)
    gclient_scm.os.makedirs(parent)
    gclient_scm.os.path.exists(parent).AndReturn(True)
    files_list = self.mox.CreateMockAnything()
    gclient_scm.scm.SVN.RunAndGetFileList(
        options.verbose,
        ['checkout', self.url, self.base_path, '--force', '--ignore-externals'],
        cwd=self.root_dir,
        file_list=files_list)

    self.mox.ReplayAll()
    scm = self._scm_wrapper(url=self.url, root_dir=self.root_dir,
                            relpath=self.relpath)
    scm.revert(options, self.args, files_list)
    self.checkstdout(
        ('\n_____ %s is missing, synching instead\n' % self.relpath))

  def testRevertNoDotSvn(self):
    options = self.Options(verbose=True, force=True)
    gclient_scm.os.path.isdir(self.base_path).AndReturn(True)
    gclient_scm.os.path.isdir(join(self.base_path, '.svn')).AndReturn(False)
    gclient_scm.os.path.isdir(join(self.base_path, '.git')).AndReturn(False)
    gclient_scm.os.path.isdir(join(self.base_path, '.hg')).AndReturn(False)
    # Checkout.
    gclient_scm.os.path.exists(self.base_path).AndReturn(False)
    gclient_scm.os.path.exists(join(self.base_path, '.git')).AndReturn(False)
    gclient_scm.os.path.exists(join(self.base_path, '.hg')).AndReturn(False)
    gclient_scm.os.path.exists(self.base_path).AndReturn(False)
    parent = gclient_scm.os.path.dirname(self.base_path)
    gclient_scm.os.path.exists(parent).AndReturn(False)
    gclient_scm.os.makedirs(parent)
    gclient_scm.os.path.exists(parent).AndReturn(True)
    files_list = self.mox.CreateMockAnything()
    gclient_scm.scm.SVN.Capture(['--version']).AndReturn('svn, version 1.6')
    gclient_scm.scm.SVN.RunAndGetFileList(
        options.verbose,
        ['checkout', self.url, self.base_path, '--force', '--ignore-externals'],
        cwd=self.root_dir,
        file_list=files_list)

    self.mox.ReplayAll()
    scm = self._scm_wrapper(url=self.url, root_dir=self.root_dir,
                            relpath=self.relpath)
    scm.revert(options, self.args, files_list)
    self.checkstdout(
        '\n_____ %s is not a valid svn checkout, synching instead\n' %
        self.relpath)

  def testRevertNone(self):
    options = self.Options(verbose=True)
    gclient_scm.os.path.isdir(self.base_path).AndReturn(True)
    gclient_scm.os.path.isdir(join(self.base_path, '.svn')).AndReturn(True)
    gclient_scm.scm.SVN.CaptureStatus(self.base_path).AndReturn([])
    gclient_scm.os.path.isdir(self.base_path).AndReturn(True)
    gclient_scm.scm.SVN.RunAndGetFileList(
        options.verbose,
        ['update', '--revision', 'BASE', '--ignore-externals'],
        cwd=self.base_path,
        file_list=mox.IgnoreArg())

    self.mox.ReplayAll()
    scm = self._scm_wrapper(url=self.url, root_dir=self.root_dir,
                            relpath=self.relpath)
    file_list = []
    scm.revert(options, self.args, file_list)

  def testRevertDirectory(self):
    options = self.Options(verbose=True)
    gclient_scm.os.path.isdir(self.base_path).AndReturn(True)
    gclient_scm.os.path.isdir(join(self.base_path, '.svn')).AndReturn(True)
    items = [
      ('~      ', 'a'),
    ]
    gclient_scm.scm.SVN.CaptureStatus(self.base_path).AndReturn(items)
    file_path = join(self.base_path, 'a')
    gclient_scm.os.path.exists(file_path).AndReturn(True)
    gclient_scm.os.path.isfile(file_path).AndReturn(False)
    gclient_scm.os.path.islink(file_path).AndReturn(False)
    gclient_scm.os.path.isdir(file_path).AndReturn(True)
    gclient_scm.gclient_utils.RemoveDirectory(file_path)
    gclient_scm.os.path.isdir(self.base_path).AndReturn(True)
    gclient_scm.scm.SVN.RunAndGetFileList(
        options.verbose,
        ['update', '--revision', 'BASE', '--ignore-externals'],
        cwd=self.base_path,
        file_list=mox.IgnoreArg())

    self.mox.ReplayAll()
    scm = self._scm_wrapper(url=self.url, root_dir=self.root_dir,
                            relpath=self.relpath)
    file_list2 = []
    scm.revert(options, self.args, file_list2)
    self.checkstdout(('%s\n' % file_path))

  def testRevertDot(self):
    self.mox.StubOutWithMock(gclient_scm.SVNWrapper, 'update')
    options = self.Options(verbose=True)
    gclient_scm.os.path.isdir(self.base_path).AndReturn(True)
    gclient_scm.os.path.isdir(join(self.base_path, '.svn')).AndReturn(True)
    items = [
      ('~      ', '.'),
    ]
    gclient_scm.scm.SVN.CaptureStatus(self.base_path).AndReturn(items)
    file_path = join(self.base_path, '.')
    gclient_scm.os.path.exists(file_path).AndReturn(True)
    gclient_scm.os.path.isfile(file_path).AndReturn(False)
    gclient_scm.os.path.islink(file_path).AndReturn(False)
    gclient_scm.os.path.isdir(file_path).AndReturn(True)
    gclient_scm.gclient_utils.RemoveDirectory(file_path)
    # pylint: disable=E1120
    gclient_scm.os.path.isdir(self.base_path).AndReturn(False)
    gclient_scm.SVNWrapper.update(options, [], ['.'])

    self.mox.ReplayAll()
    scm = self._scm_wrapper(url=self.url, root_dir=self.root_dir,
                            relpath=self.relpath)
    file_list2 = []
    scm.revert(options, self.args, file_list2)
    self.checkstdout(('%s\n' % file_path))

  def testStatus(self):
    options = self.Options(verbose=True)
    gclient_scm.os.path.isdir(self.base_path).AndReturn(True)
    gclient_scm.scm.SVN.RunAndGetFileList(
        options.verbose,
        ['status'] + self.args + ['--ignore-externals'],
        cwd=self.base_path,
        file_list=[]).AndReturn(None)

    self.mox.ReplayAll()
    scm = self._scm_wrapper(url=self.url, root_dir=self.root_dir,
                            relpath=self.relpath)
    file_list = []
    self.assertEqual(scm.status(options, self.args, file_list), None)

  # TODO(maruel):  TEST REVISIONS!!!
  # TODO(maruel):  TEST RELOCATE!!!
  def testUpdateCheckout(self):
    options = self.Options(verbose=True)
    file_info = gclient_scm.gclient_utils.PrintableObject()
    file_info.root = 'blah'
    file_info.url = self.url
    file_info.uuid = 'ABC'
    file_info.revision = 42
    gclient_scm.os.path.exists(join(self.base_path, '.git')).AndReturn(False)
    gclient_scm.os.path.exists(join(self.base_path, '.hg')).AndReturn(False)
    # Checkout.
    gclient_scm.os.path.exists(self.base_path).AndReturn(False)
    parent = gclient_scm.os.path.dirname(self.base_path)
    gclient_scm.os.path.exists(parent).AndReturn(False)
    gclient_scm.os.makedirs(parent)
    gclient_scm.os.path.exists(parent).AndReturn(True)
    files_list = self.mox.CreateMockAnything()
    gclient_scm.scm.SVN.Capture(['--version']
        ).AndReturn('svn, version 1.5.1 (r32289)')
    gclient_scm.scm.SVN.RunAndGetFileList(
        options.verbose,
        ['checkout', self.url, self.base_path, '--force', '--ignore-externals'],
        cwd=self.root_dir,
        file_list=files_list)
    self.mox.ReplayAll()
    scm = self._scm_wrapper(url=self.url, root_dir=self.root_dir,
                            relpath=self.relpath)
    scm.update(options, (), files_list)

  def testUpdateUpdate(self):
    options = self.Options(verbose=True)
    options.force = True
    options.nohooks = False
    file_info = {
      'Repository Root': 'blah',
      'URL': self.url,
      'UUID': 'ABC',
      'Revision': 42,
    }
    gclient_scm.os.path.exists(join(self.base_path, '.git')).AndReturn(False)
    gclient_scm.os.path.exists(join(self.base_path, '.hg')).AndReturn(False)

    # Verify no locked files.
    dotted_path = join(self.base_path, '.')
    gclient_scm.scm.SVN.CaptureStatus(dotted_path).AndReturn([])

    # Checkout or update.
    gclient_scm.os.path.exists(self.base_path).AndReturn(True)
    gclient_scm.scm.SVN.CaptureInfo(dotted_path).AndReturn(file_info)
    # Cheat a bit here.
    gclient_scm.scm.SVN.CaptureInfo(file_info['URL']).AndReturn(file_info)
    additional_args = []
    if options.manually_grab_svn_rev:
      additional_args = ['--revision', str(file_info['Revision'])]
    gclient_scm.scm.SVN.Capture(['--version']
        ).AndReturn('svn, version 1.5.1 (r32289)')
    additional_args.extend(['--force', '--ignore-externals'])
    files_list = []
    gclient_scm.scm.SVN.RunAndGetFileList(
        options.verbose,
        ['update', self.base_path] + additional_args,
        cwd=self.root_dir, file_list=files_list)

    self.mox.ReplayAll()
    scm = self._scm_wrapper(url=self.url, root_dir=self.root_dir,
                            relpath=self.relpath)
    scm.update(options, (), files_list)

  def testUpdateSingleCheckout(self):
    options = self.Options(verbose=True)
    file_info = {
      'URL': self.url,
      'Revision': 42,
    }

    # Checks to make sure that we support svn co --depth.
    gclient_scm.scm.SVN.current_version = None
    gclient_scm.scm.SVN.Capture(['--version']
        ).AndReturn('svn, version 1.5.1 (r32289)')
    gclient_scm.os.path.exists(join(self.base_path, '.svn')).AndReturn(False)
    gclient_scm.os.path.exists(join(self.base_path, 'DEPS')).AndReturn(False)

    # Verify no locked files.
    dotted_path = join(self.base_path, '.')
    gclient_scm.scm.SVN.CaptureStatus(dotted_path).AndReturn([])

    # When checking out a single file, we issue an svn checkout and svn update.
    files_list = self.mox.CreateMockAnything()
    gclient_scm.gclient_utils.CheckCallAndFilterAndHeader(
        ['svn', 'checkout', '--depth', 'empty', self.url, self.base_path],
        always=True,
        cwd=self.root_dir)
    gclient_scm.scm.SVN.RunAndGetFileList(
        options.verbose,
        ['update', 'DEPS', '--ignore-externals'],
        cwd=self.base_path,
        file_list=files_list)

    # Now we fall back on scm.update().
    gclient_scm.os.path.exists(join(self.base_path, '.git')).AndReturn(False)
    gclient_scm.os.path.exists(join(self.base_path, '.hg')).AndReturn(False)
    gclient_scm.os.path.exists(self.base_path).AndReturn(True)
    gclient_scm.scm.SVN.CaptureInfo(dotted_path).AndReturn(file_info)
    gclient_scm.scm.SVN.CaptureInfo(file_info['URL']).AndReturn(file_info)

    self.mox.ReplayAll()
    scm = self._scm_wrapper(url=self.url, root_dir=self.root_dir,
                            relpath=self.relpath)
    scm.updatesingle(options, ['DEPS'], files_list)
    self.checkstdout('\n_____ %s at 42\n' % self.relpath)

  def testUpdateSingleCheckoutSVN14(self):
    options = self.Options(verbose=True)

    # Checks to make sure that we support svn co --depth.
    gclient_scm.scm.SVN.current_version = None
    gclient_scm.scm.SVN.Capture(['--version']
        ).AndReturn('svn, version 1.4.4 (r25188)')
    gclient_scm.os.path.exists(self.base_path).AndReturn(True)

    # When checking out a single file with svn 1.4, we use svn export
    files_list = self.mox.CreateMockAnything()
    gclient_scm.gclient_utils.CheckCallAndFilterAndHeader(
        ['svn', 'export', join(self.url, 'DEPS'), join(self.base_path, 'DEPS')],
        always=True, cwd=self.root_dir)

    self.mox.ReplayAll()
    scm = self._scm_wrapper(url=self.url, root_dir=self.root_dir,
                            relpath=self.relpath)
    scm.updatesingle(options, ['DEPS'], files_list)

  def testUpdateSingleCheckoutSVNUpgrade(self):
    options = self.Options(verbose=True)
    file_info = {
      'URL': self.url,
      'Revision': 42,
    }

    # Checks to make sure that we support svn co --depth.
    gclient_scm.scm.SVN.current_version = None
    gclient_scm.scm.SVN.Capture(['--version']
        ).AndReturn('svn, version 1.5.1 (r32289)')
    gclient_scm.os.path.exists(join(self.base_path, '.svn')).AndReturn(False)
    # If DEPS already exists, assume we're upgrading from svn1.4, so delete
    # the old DEPS file.
    gclient_scm.os.path.exists(join(self.base_path, 'DEPS')).AndReturn(True)
    gclient_scm.os.remove(join(self.base_path, 'DEPS'))

    # Verify no locked files.
    gclient_scm.scm.SVN.CaptureStatus(join(self.base_path, '.')).AndReturn([])

    # When checking out a single file, we issue an svn checkout and svn update.
    files_list = self.mox.CreateMockAnything()
    gclient_scm.gclient_utils.CheckCallAndFilterAndHeader(
        ['svn', 'checkout', '--depth', 'empty', self.url, self.base_path],
        always=True,
        cwd=self.root_dir)
    gclient_scm.scm.SVN.RunAndGetFileList(
        options.verbose,
        ['update', 'DEPS', '--ignore-externals'],
        cwd=self.base_path,
        file_list=files_list)

    # Now we fall back on scm.update().
    gclient_scm.os.path.exists(join(self.base_path, '.git')).AndReturn(False)
    gclient_scm.os.path.exists(join(self.base_path, '.hg')).AndReturn(False)
    gclient_scm.os.path.exists(self.base_path).AndReturn(True)
    gclient_scm.scm.SVN.CaptureInfo(
        join(self.base_path, ".")).AndReturn(file_info)
    gclient_scm.scm.SVN.CaptureInfo(file_info['URL']).AndReturn(file_info)

    self.mox.ReplayAll()
    scm = self._scm_wrapper(url=self.url, root_dir=self.root_dir,
                            relpath=self.relpath)
    scm.updatesingle(options, ['DEPS'], files_list)
    self.checkstdout(
        ('\n_____ %s at 42\n' % self.relpath))

  def testUpdateSingleUpdate(self):
    options = self.Options(verbose=True)
    file_info = {
      'URL': self.url,
      'Revision': 42,
    }
    # Checks to make sure that we support svn co --depth.
    gclient_scm.scm.SVN.current_version = None
    gclient_scm.scm.SVN.Capture(['--version']
        ).AndReturn('svn, version 1.5.1 (r32289)')
    gclient_scm.os.path.exists(join(self.base_path, '.svn')).AndReturn(True)

    # Verify no locked files.
    gclient_scm.scm.SVN.CaptureStatus(join(self.base_path, '.')).AndReturn([])

    # Now we fall back on scm.update().
    files_list = self.mox.CreateMockAnything()
    gclient_scm.os.path.exists(join(self.base_path, '.git')).AndReturn(False)
    gclient_scm.os.path.exists(join(self.base_path, '.hg')).AndReturn(False)
    gclient_scm.os.path.exists(self.base_path).AndReturn(True)
    gclient_scm.scm.SVN.CaptureInfo(
        join(self.base_path, '.')).AndReturn(file_info)
    gclient_scm.scm.SVN.CaptureInfo(file_info['URL']).AndReturn(file_info)

    self.mox.ReplayAll()
    scm = self._scm_wrapper(url=self.url, root_dir=self.root_dir,
                            relpath=self.relpath)
    scm.updatesingle(options, ['DEPS'], files_list)
    self.checkstdout('\n_____ %s at 42\n' % self.relpath)

  def testUpdateGit(self):
    options = self.Options(verbose=True)
    file_path = gclient_scm.os.path.join(self.root_dir, self.relpath, '.git')
    gclient_scm.os.path.exists(file_path).AndReturn(True)

    self.mox.ReplayAll()
    scm = self._scm_wrapper(url=self.url, root_dir=self.root_dir,
                            relpath=self.relpath)
    file_list = []
    scm.update(options, self.args, file_list)
    self.checkstdout(
        ('________ found .git directory; skipping %s\n' % self.relpath))

  def testUpdateHg(self):
    options = self.Options(verbose=True)
    gclient_scm.os.path.exists(join(self.base_path, '.git')).AndReturn(False)
    gclient_scm.os.path.exists(join(self.base_path, '.hg')).AndReturn(True)

    self.mox.ReplayAll()
    scm = self._scm_wrapper(url=self.url, root_dir=self.root_dir,
                            relpath=self.relpath)
    file_list = []
    scm.update(options, self.args, file_list)
    self.checkstdout(
        ('________ found .hg directory; skipping %s\n' % self.relpath))


class BaseGitWrapperTestCase(GCBaseTestCase, StdoutCheck, TestCaseUtils,
                             unittest.TestCase):
  """This class doesn't use pymox."""
  class OptionsObject(object):
    def __init__(self, verbose=False, revision=None):
      self.verbose = verbose
      self.revision = revision
      self.manually_grab_svn_rev = True
      self.deps_os = None
      self.force = False
      self.reset = False
      self.nohooks = False
      self.merge = False

  sample_git_import = """blob
mark :1
data 6
Hello

blob
mark :2
data 4
Bye

reset refs/heads/master
commit refs/heads/master
mark :3
author Bob <bob@example.com> 1253744361 -0700
committer Bob <bob@example.com> 1253744361 -0700
data 8
A and B
M 100644 :1 a
M 100644 :2 b

blob
mark :4
data 10
Hello
You

blob
mark :5
data 8
Bye
You

commit refs/heads/origin
mark :6
author Alice <alice@example.com> 1253744424 -0700
committer Alice <alice@example.com> 1253744424 -0700
data 13
Personalized
from :3
M 100644 :4 a
M 100644 :5 b

reset refs/heads/master
from :3
"""
  def Options(self, *args, **kwargs):
    return self.OptionsObject(*args, **kwargs)

  @staticmethod
  def CreateGitRepo(git_import, path):
    """Do it for real."""
    try:
      Popen(['git', 'init', '-q'], stdout=PIPE, stderr=STDOUT,
            cwd=path).communicate()
    except OSError:
      # git is not available, skip this test.
      return False
    Popen(['git', 'fast-import', '--quiet'], stdin=PIPE, stdout=PIPE,
        stderr=STDOUT, cwd=path).communicate(input=git_import)
    Popen(['git', 'checkout', '-q'], stdout=PIPE, stderr=STDOUT,
        cwd=path).communicate()
    Popen(['git', 'remote', 'add', '-f', 'origin', '.'], stdout=PIPE,
        stderr=STDOUT, cwd=path).communicate()
    Popen(['git', 'checkout', '-b', 'new', 'origin/master', '-q'], stdout=PIPE,
        stderr=STDOUT, cwd=path).communicate()
    Popen(['git', 'push', 'origin', 'origin/origin:origin/master', '-q'],
        stdout=PIPE, stderr=STDOUT, cwd=path).communicate()
    Popen(['git', 'config', '--unset', 'remote.origin.fetch'], stdout=PIPE,
        stderr=STDOUT, cwd=path).communicate()
    return True

  def setUp(self):
    TestCaseUtils.setUp(self)
    unittest.TestCase.setUp(self)
    self.url = 'git://foo'
    self.root_dir = tempfile.mkdtemp()
    self.relpath = '.'
    self.base_path = join(self.root_dir, self.relpath)
    self.enabled = self.CreateGitRepo(self.sample_git_import, self.base_path)
    StdoutCheck.setUp(self)

  def tearDown(self):
    StdoutCheck.tearDown(self)
    TestCaseUtils.tearDown(self)
    unittest.TestCase.tearDown(self)
    rmtree(self.root_dir)


class ManagedGitWrapperTestCase(BaseGitWrapperTestCase):
  def testDir(self):
    members = [
        'FullUrlForRelativeUrl', 'GetRevisionDate', 'RunCommand',
        'cleanup', 'diff', 'pack', 'relpath', 'revert',
        'revinfo', 'runhooks', 'status', 'update', 'url',
    ]

    # If you add a member, be sure to add the relevant test!
    self.compareMembers(gclient_scm.CreateSCM(url=self.url), members)

  def testRevertMissing(self):
    if not self.enabled:
      return
    options = self.Options()
    file_path = join(self.base_path, 'a')
    scm = gclient_scm.CreateSCM(url=self.url, root_dir=self.root_dir,
                                relpath=self.relpath)
    file_list = []
    scm.update(options, None, file_list)
    gclient_scm.os.remove(file_path)
    file_list = []
    scm.revert(options, self.args, file_list)
    self.assertEquals(file_list, [file_path])
    file_list = []
    scm.diff(options, self.args, file_list)
    self.assertEquals(file_list, [])
    expectation = ('\n_____ . at refs/heads/master\nUpdating 069c602..a7142dc\n'
         'Fast-forward\n a |    1 +\n b |    1 +\n'
         ' 2 files changed, 2 insertions(+), 0 deletions(-)\n\n\n'
         '________ running \'git reset --hard origin/master\' in \'%s\'\n'
         'HEAD is now at a7142dc Personalized\n') % join(self.root_dir, '.')
    self.assertTrue(sys.stdout.getvalue().startswith(expectation))
    sys.stdout.close()

  def testRevertNone(self):
    if not self.enabled:
      return
    options = self.Options()
    scm = gclient_scm.CreateSCM(url=self.url, root_dir=self.root_dir,
                                relpath=self.relpath)
    file_list = []
    scm.update(options, None, file_list)
    file_list = []
    scm.revert(options, self.args, file_list)
    self.assertEquals(file_list, [])
    self.assertEquals(scm.revinfo(options, self.args, None),
                      'a7142dc9f0009350b96a11f372b6ea658592aa95')
    self.checkstdout(
      ('\n_____ . at refs/heads/master\nUpdating 069c602..a7142dc\n'
       'Fast-forward\n a |    1 +\n b |    1 +\n'
       ' 2 files changed, 2 insertions(+), 0 deletions(-)\n\n\n'
       '________ running \'git reset --hard origin/master\' in \'%s\'\n'
       'HEAD is now at a7142dc Personalized\n') %
            join(self.root_dir, '.'))

  def testRevertModified(self):
    if not self.enabled:
      return
    options = self.Options()
    scm = gclient_scm.CreateSCM(url=self.url, root_dir=self.root_dir,
                                relpath=self.relpath)
    file_list = []
    scm.update(options, None, file_list)
    file_path = join(self.base_path, 'a')
    open(file_path, 'a').writelines('touched\n')
    file_list = []
    scm.revert(options, self.args, file_list)
    self.assertEquals(file_list, [file_path])
    file_list = []
    scm.diff(options, self.args, file_list)
    self.assertEquals(file_list, [])
    self.assertEquals(scm.revinfo(options, self.args, None),
                      'a7142dc9f0009350b96a11f372b6ea658592aa95')
    expectation = ('\n_____ . at refs/heads/master\nUpdating 069c602..a7142dc\n'
       'Fast-forward\n a |    1 +\n b |    1 +\n'
       ' 2 files changed, 2 insertions(+), 0 deletions(-)\n\n\n'
       '________ running \'git reset --hard origin/master\' in \'%s\'\n'
       'HEAD is now at a7142dc Personalized\n') % join(self.root_dir, '.')
    self.assertTrue(sys.stdout.getvalue().startswith(expectation))
    sys.stdout.close()

  def testRevertNew(self):
    if not self.enabled:
      return
    options = self.Options()
    scm = gclient_scm.CreateSCM(url=self.url, root_dir=self.root_dir,
                                relpath=self.relpath)
    file_list = []
    scm.update(options, None, file_list)
    file_path = join(self.base_path, 'c')
    f = open(file_path, 'w')
    f.writelines('new\n')
    f.close()
    Popen(['git', 'add', 'c'], stdout=PIPE,
          stderr=STDOUT, cwd=self.base_path).communicate()
    file_list = []
    scm.revert(options, self.args, file_list)
    self.assertEquals(file_list, [file_path])
    file_list = []
    scm.diff(options, self.args, file_list)
    self.assertEquals(file_list, [])
    self.assertEquals(scm.revinfo(options, self.args, None),
                      'a7142dc9f0009350b96a11f372b6ea658592aa95')
    expectation = ('\n_____ . at refs/heads/master\nUpdating 069c602..a7142dc\n'
       'Fast-forward\n a |    1 +\n b |    1 +\n'
       ' 2 files changed, 2 insertions(+), 0 deletions(-)\n\n\n'
       '________ running \'git reset --hard origin/master\' in \'%s\'\n'
       'HEAD is now at a7142dc Personalized\n') % join(self.root_dir, '.')
    self.assertTrue(sys.stdout.getvalue().startswith(expectation))
    sys.stdout.close()

  def testStatusNew(self):
    if not self.enabled:
      return
    options = self.Options()
    file_path = join(self.base_path, 'a')
    open(file_path, 'a').writelines('touched\n')
    scm = gclient_scm.CreateSCM(url=self.url, root_dir=self.root_dir,
                                relpath=self.relpath)
    file_list = []
    scm.status(options, self.args, file_list)
    self.assertEquals(file_list, [file_path])
    self.checkstdout(
        ('\n________ running \'git diff --name-status '
         '069c602044c5388d2d15c3f875b057c852003458\' in \'%s\'\nM\ta\n') %
            join(self.root_dir, '.'))

  def testStatus2New(self):
    if not self.enabled:
      return
    options = self.Options()
    expected_file_list = []
    for f in ['a', 'b']:
      file_path = join(self.base_path, f)
      open(file_path, 'a').writelines('touched\n')
      expected_file_list.extend([file_path])
    scm = gclient_scm.CreateSCM(url=self.url, root_dir=self.root_dir,
                                relpath=self.relpath)
    file_list = []
    scm.status(options, self.args, file_list)
    expected_file_list = [join(self.base_path, x) for x in ['a', 'b']]
    self.assertEquals(sorted(file_list), expected_file_list)
    self.checkstdout(
        ('\n________ running \'git diff --name-status '
         '069c602044c5388d2d15c3f875b057c852003458\' in \'%s\'\nM\ta\nM\tb\n') %
            join(self.root_dir, '.'))

  def testUpdateCheckout(self):
    if not self.enabled:
      return
    options = self.Options(verbose=True)
    root_dir = gclient_scm.os.path.realpath(tempfile.mkdtemp())
    relpath = 'foo'
    base_path = join(root_dir, relpath)
    url = join(self.base_path, '.git')
    try:
      scm = gclient_scm.CreateSCM(url=url, root_dir=root_dir,
                                  relpath=relpath)
      file_list = []
      scm.update(options, (), file_list)
      self.assertEquals(len(file_list), 2)
      self.assert_(gclient_scm.os.path.isfile(join(base_path, 'a')))
      self.assertEquals(scm.revinfo(options, (), None),
                        '069c602044c5388d2d15c3f875b057c852003458')
    finally:
      rmtree(root_dir)
    msg1 = (
        "\n_____ foo at refs/heads/master\n\n"
        "________ running 'git clone --progress -b master --verbose %s %s' "
        "in '%s'\n"
        "Initialized empty Git repository in %s\n") % (
          join(self.root_dir, '.', '.git'),
          join(root_dir, 'foo'),
          root_dir,
          join(gclient_scm.os.path.realpath(root_dir), 'foo', '.git') + '/')
    msg2 = (
        "\n_____ foo at refs/heads/master\n\n"
        "________ running 'git clone --progress -b master --verbose %s %s'"
        " in '%s'\n"
        "Cloning into %s...\ndone.\n") % (
          join(self.root_dir, '.', '.git'),
          join(root_dir, 'foo'),
          root_dir,
          join(gclient_scm.os.path.realpath(root_dir), 'foo'))
    out = sys.stdout.getvalue()
    sys.stdout.close()
    sys.stdout = self._old_stdout
    self.assertTrue(out in (msg1, msg2), (out, msg1, msg2))

  def testUpdateUpdate(self):
    if not self.enabled:
      return
    options = self.Options()
    expected_file_list = [join(self.base_path, x) for x in ['a', 'b']]
    scm = gclient_scm.CreateSCM(url=self.url, root_dir=self.root_dir,
                                relpath=self.relpath)
    file_list = []
    scm.update(options, (), file_list)
    self.assertEquals(file_list, expected_file_list)
    self.assertEquals(scm.revinfo(options, (), None),
                      'a7142dc9f0009350b96a11f372b6ea658592aa95')
    self.checkstdout(
        '\n_____ . at refs/heads/master\n'
        'Updating 069c602..a7142dc\nFast-forward\n a |    1 +\n b |    1 +\n'
        ' 2 files changed, 2 insertions(+), 0 deletions(-)\n\n')

  def testUpdateUnstagedConflict(self):
    if not self.enabled:
      return
    options = self.Options()
    scm = gclient_scm.CreateSCM(url=self.url, root_dir=self.root_dir,
                                relpath=self.relpath)
    file_path = join(self.base_path, 'b')
    open(file_path, 'w').writelines('conflict\n')
    try:
      scm.update(options, (), [])
      self.fail()
    except (gclient_scm.gclient_utils.Error, subprocess2.CalledProcessError):
      # The exact exception text varies across git versions so it's not worth
      # verifying it. It's fine as long as it throws.
      pass
    # Manually flush stdout since we can't verify it's content accurately across
    # git versions.
    sys.stdout.getvalue()
    sys.stdout.close()

  def testUpdateConflict(self):
    if not self.enabled:
      return
    options = self.Options()
    scm = gclient_scm.CreateSCM(url=self.url, root_dir=self.root_dir,
                                relpath=self.relpath)
    file_path = join(self.base_path, 'b')
    open(file_path, 'w').writelines('conflict\n')
    # pylint: disable=W0212
    scm._Run(['commit', '-am', 'test'], options)
    __builtin__.raw_input = lambda x: 'y'
    exception = ('Conflict while rebasing this branch.\n'
                 'Fix the conflict and run gclient again.\n'
                 'See \'man git-rebase\' for details.\n')
    self.assertRaisesError(exception, scm.update, options, (), [])
    exception = ('\n____ . at refs/heads/master\n'
                 '\tYou have unstaged changes.\n'
                 '\tPlease commit, stash, or reset.\n')
    self.assertRaisesError(exception, scm.update, options, (), [])
    # The hash always changes. Use a cheap trick.
    start = ('\n________ running \'git commit -am test\' in \'%s\'\n'
             '[new ') % join(self.root_dir, '.')
    end = ('] test\n 1 files changed, 1 insertions(+), '
         '1 deletions(-)\n\n_____ . at refs/heads/master\n'
         'Attempting rebase onto refs/remotes/origin/master...\n')
    self.assertTrue(sys.stdout.getvalue().startswith(start))
    self.assertTrue(sys.stdout.getvalue().endswith(end))
    self.assertEquals(len(sys.stdout.getvalue()),
                      len(start) + len(end) + 7)
    sys.stdout.close()

  def testUpdateNotGit(self):
    if not self.enabled:
      return
    options = self.Options()
    scm = gclient_scm.CreateSCM(url=self.url, root_dir=self.root_dir,
                                relpath=self.relpath)
    git_path = join(self.base_path, '.git')
    rename(git_path, git_path + 'foo')
    exception = ('\n____ . at refs/heads/master\n'
                 '\tPath is not a git repo. No .git dir.\n'
                 '\tTo resolve:\n'
                 '\t\trm -rf .\n'
                 '\tAnd run gclient sync again\n')
    self.assertRaisesError(exception, scm.update, options, (), [])

  def testRevinfo(self):
    if not self.enabled:
      return
    options = self.Options()
    scm = gclient_scm.CreateSCM(url=self.url, root_dir=self.root_dir,
                                relpath=self.relpath)
    rev_info = scm.revinfo(options, (), None)
    self.assertEquals(rev_info, '069c602044c5388d2d15c3f875b057c852003458')


class UnmanagedGitWrapperTestCase(BaseGitWrapperTestCase):
  def testUpdateCheckout(self):
    if not self.enabled:
      return
    options = self.Options(verbose=True)
    root_dir = gclient_scm.os.path.realpath(tempfile.mkdtemp())
    relpath = 'foo'
    base_path = join(root_dir, relpath)
    url = join(self.base_path, '.git')
    try:
      scm = gclient_scm.CreateSCM(url=url, root_dir=root_dir,
                                  relpath=relpath)
      file_list = []
      options.revision = 'unmanaged'
      scm.update(options, (), file_list)
      self.assertEquals(len(file_list), 2)
      self.assert_(gclient_scm.os.path.isfile(join(base_path, 'a')))
      self.assertEquals(scm.revinfo(options, (), None),
                        '069c602044c5388d2d15c3f875b057c852003458')
    finally:
      rmtree(root_dir)
    msg1 = (
        "\n_____ foo at refs/heads/master\n\n"
        "________ running 'git clone --progress -b master --verbose %s %s'"
        " in '%s'\n"
        "Initialized empty Git repository in %s\n") % (
          join(self.root_dir, '.', '.git'),
          join(root_dir, 'foo'),
          root_dir,
          join(gclient_scm.os.path.realpath(root_dir), 'foo', '.git') + '/')
    msg2 = (
        "\n_____ foo at refs/heads/master\n\n"
        "________ running 'git clone --progress -b master --verbose %s %s'"
        " in '%s'\n"
        "Cloning into %s...\ndone.\n") % (
          join(self.root_dir, '.', '.git'),
          join(root_dir, 'foo'),
          root_dir,
          join(gclient_scm.os.path.realpath(root_dir), 'foo'))
    out = sys.stdout.getvalue()
    sys.stdout.close()
    sys.stdout = self._old_stdout
    self.assertTrue(out in (msg1, msg2), (out, msg1, msg2))

  def testUpdateUpdate(self):
    if not self.enabled:
      return
    options = self.Options()
    expected_file_list = []
    scm = gclient_scm.CreateSCM(url=self.url, root_dir=self.root_dir,
                                relpath=self.relpath)
    file_list = []
    options.revision = 'unmanaged'
    scm.update(options, (), file_list)
    self.assertEquals(file_list, expected_file_list)
    self.assertEquals(scm.revinfo(options, (), None),
                      '069c602044c5388d2d15c3f875b057c852003458')
    self.checkstdout('________ unmanaged solution; skipping .\n')


if __name__ == '__main__':
  if '-v' in sys.argv:
    logging.basicConfig(
        level=logging.DEBUG,
        format='%(asctime).19s %(levelname)s %(filename)s:'
               '%(lineno)s %(message)s')
  unittest.main()

# vim: ts=2:sw=2:tw=80:et:
