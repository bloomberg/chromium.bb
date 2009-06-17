#!/usr/bin/python
# Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for presubmit_support.py and presubmit_canned_checks.py."""

import exceptions
import StringIO
import unittest

# Local imports
import presubmit_support as presubmit
import presubmit_canned_checks
import super_mox
from super_mox import mox


class PresubmitTestsBase(super_mox.SuperMoxTestBase):
  """Setups and tear downs the mocks but doesn't test anything as-is."""
  presubmit_text = """
def CheckChangeOnUpload(input_api, output_api):
  if not input_api.change.NOSUCHKEY:
    return [output_api.PresubmitError("!!")]
  elif not input_api.change.REALLYNOSUCHKEY:
    return [output_api.PresubmitPromptWarning("??")]
  elif not input_api.change.REALLYABSOLUTELYNOSUCHKEY:
    return [output_api.PresubmitPromptWarning("??"),
            output_api.PresubmitError("XX!!XX")]
  else:
    return ()
"""

  def setUp(self):
    super_mox.SuperMoxTestBase.setUp(self)
    self.mox.StubOutWithMock(presubmit, 'warnings')
    # Stub out 'os' but keep os.path.dirname/join/normpath/splitext and os.sep.
    os_sep = presubmit.os.sep
    os_path_join = presubmit.os.path.join
    os_path_dirname = presubmit.os.path.dirname
    os_path_normpath = presubmit.os.path.normpath
    os_path_splitext = presubmit.os.path.splitext
    self.mox.StubOutWithMock(presubmit, 'os')
    self.mox.StubOutWithMock(presubmit.os, 'path')
    presubmit.os.sep = os_sep
    presubmit.os.path.join = os_path_join
    presubmit.os.path.dirname = os_path_dirname
    presubmit.os.path.normpath = os_path_normpath
    presubmit.os.path.splitext = os_path_splitext
    self.mox.StubOutWithMock(presubmit, 'sys')
    # Special mocks.
    def MockAbsPath(f):
      return f
    presubmit.os.path.abspath = MockAbsPath
    self.fake_root_dir = self.RootDir()
    self.mox.StubOutWithMock(presubmit.gclient, 'CaptureSVNInfo')
    self.mox.StubOutWithMock(presubmit.gcl, 'GetSVNFileProperty')
    self.mox.StubOutWithMock(presubmit.gcl, 'ReadFile')


class PresubmitUnittest(PresubmitTestsBase):
  """General presubmit_support.py tests (excluding InputApi and OutputApi)."""
  def testMembersChanged(self):
    self.mox.ReplayAll()
    members = [
      'AffectedFile', 'Change', 'DoPresubmitChecks', 'GitChange',
      'GitAffectedFile', 'InputApi',
      'ListRelevantPresubmitFiles', 'Main', 'NotImplementedException',
      'OutputApi', 'ParseFiles', 'PresubmitExecuter', 'ScanSubDirs',
      'SvnAffectedFile', 'SvnChange',
      'cPickle', 'cStringIO', 'exceptions',
      'fnmatch', 'gcl', 'gclient', 'glob', 'logging', 'marshal', 'normpath',
      'optparse',
      'os', 'pickle', 'presubmit_canned_checks', 're', 'subprocess', 'sys',
      'tempfile', 'traceback', 'types', 'unittest', 'urllib2', 'warnings',
    ]
    # If this test fails, you should add the relevant test.
    self.compareMembers(presubmit, members)

  def testListRelevantPresubmitFiles(self):
    join = presubmit.os.path.join
    files = [
      'blat.cc',
      join('foo', 'haspresubmit', 'yodle', 'smart.h'),
      join('moo', 'mat', 'gat', 'yo.h'),
      join('foo', 'luck.h'),
    ]
    presubmit.os.path.isfile(join(self.fake_root_dir,
                                  'PRESUBMIT.py')).AndReturn(True)
    presubmit.os.path.isfile(join(self.fake_root_dir, 'foo',
                                  'PRESUBMIT.py')).AndReturn(False)
    presubmit.os.path.isfile(join(self.fake_root_dir, 'foo', 'haspresubmit',
                                  'PRESUBMIT.py')).AndReturn(True)
    presubmit.os.path.isfile(join(self.fake_root_dir, 'foo', 'haspresubmit',
                                  'yodle', 'PRESUBMIT.py')).AndReturn(True)
    presubmit.os.path.isfile(join(self.fake_root_dir, 'moo',
                                  'PRESUBMIT.py')).AndReturn(False)
    presubmit.os.path.isfile(join(self.fake_root_dir, 'moo', 'mat',
                                  'PRESUBMIT.py')).AndReturn(False)
    presubmit.os.path.isfile(join(self.fake_root_dir, 'moo', 'mat', 'gat',
                                  'PRESUBMIT.py')).AndReturn(False)
    self.mox.ReplayAll()

    presubmit_files = presubmit.ListRelevantPresubmitFiles(files,
                                                           self.fake_root_dir)
    self.assertEqual(presubmit_files,
        [
          join(self.fake_root_dir, 'PRESUBMIT.py'),
          join(self.fake_root_dir, 'foo', 'haspresubmit', 'PRESUBMIT.py'),
          join(self.fake_root_dir, 'foo', 'haspresubmit', 'yodle',
               'PRESUBMIT.py')
        ])

  def testTagLineRe(self):
    self.mox.ReplayAll()
    m = presubmit.Change._TAG_LINE_RE.match(' BUG =1223, 1445  \t')
    self.failUnless(m)
    self.failUnlessEqual(m.group('key'), 'BUG')
    self.failUnlessEqual(m.group('value'), '1223, 1445')

  def testGclChange(self):
    description_lines = ('Hello there',
                         'this is a change',
                         'BUG=123',
                         ' STORY =http://foo/  \t',
                         'and some more regular text  \t')
    files = [
      ['A', 'foo/blat.cc'],
      ['M', 'binary.dll'],  # a binary file
      ['A', 'isdir'],  # a directory
      ['?', 'flop/notfound.txt'],  # not found in SVN, still exists locally
      ['D', 'boo/flap.h'],
    ]
    blat = presubmit.os.path.join(self.fake_root_dir, 'foo', 'blat.cc')
    notfound = presubmit.os.path.join(self.fake_root_dir, 'flop', 'notfound.txt')
    flap = presubmit.os.path.join(self.fake_root_dir, 'boo', 'flap.h')
    binary = presubmit.os.path.join(self.fake_root_dir, 'binary.dll')
    isdir = presubmit.os.path.join(self.fake_root_dir, 'isdir')
    presubmit.os.path.exists(blat).AndReturn(True)
    presubmit.os.path.isdir(blat).AndReturn(False)
    presubmit.os.path.exists(binary).AndReturn(True)
    presubmit.os.path.isdir(binary).AndReturn(False)
    presubmit.os.path.exists(isdir).AndReturn(True)
    presubmit.os.path.isdir(isdir).AndReturn(True)
    presubmit.os.path.exists(notfound).AndReturn(True)
    presubmit.os.path.isdir(notfound).AndReturn(False)
    presubmit.os.path.exists(flap).AndReturn(False)
    presubmit.gclient.CaptureSVNInfo(flap
        ).AndReturn({'Node Kind': 'file'})
    presubmit.gcl.GetSVNFileProperty(blat, 'svn:mime-type').AndReturn(None)
    presubmit.gcl.GetSVNFileProperty(
        binary, 'svn:mime-type').AndReturn('application/octet-stream')
    presubmit.gcl.GetSVNFileProperty(
        notfound, 'svn:mime-type').AndReturn('')
    presubmit.gclient.CaptureSVNInfo(blat).AndReturn(
            {'URL': 'svn:/foo/foo/blat.cc'})
    presubmit.gclient.CaptureSVNInfo(binary).AndReturn(
        {'URL': 'svn:/foo/binary.dll'})
    presubmit.gclient.CaptureSVNInfo(notfound).AndReturn({})
    presubmit.gclient.CaptureSVNInfo(flap).AndReturn(
            {'URL': 'svn:/foo/boo/flap.h'})
    presubmit.gcl.ReadFile(blat).AndReturn('boo!\nahh?')
    presubmit.gcl.ReadFile(notfound).AndReturn('look!\nthere?')
    self.mox.ReplayAll()

    change = presubmit.SvnChange('mychange', '\n'.join(description_lines),
                                 self.fake_root_dir, files, 0, 0)
    self.failUnless(change.Name() == 'mychange')
    self.failUnless(change.DescriptionText() ==
                    'Hello there\nthis is a change\nand some more regular text')
    self.failUnless(change.FullDescriptionText() ==
                    '\n'.join(description_lines))

    self.failUnless(change.BUG == '123')
    self.failUnless(change.STORY == 'http://foo/')
    self.failUnless(change.BLEH == None)

    self.failUnless(len(change.AffectedFiles()) == 4)
    self.failUnless(len(change.AffectedFiles(include_dirs=True)) == 5)
    self.failUnless(len(change.AffectedFiles(include_deletes=False)) == 3)
    self.failUnless(len(change.AffectedFiles(include_dirs=True,
                                             include_deletes=False)) == 4)

    affected_text_files = change.AffectedTextFiles()
    self.failUnless(len(affected_text_files) == 2)
    self.failIf(filter(lambda x: x.LocalPath() == 'binary.dll',
                       affected_text_files))

    local_paths = change.LocalPaths()
    expected_paths = [presubmit.normpath(f[1]) for f in files]
    self.failUnless(
        len(filter(lambda x: x in expected_paths, local_paths)) == 4)

    server_paths = change.ServerPaths()
    expected_paths = ['svn:/foo/%s' % f[1] for f in files if
                      f[1] != 'flop/notfound.txt']
    expected_paths.append('')  # one unknown file
    self.assertEqual(
      len(filter(lambda x: x in expected_paths, server_paths)), 4)

    files = [[x[0], presubmit.normpath(x[1])] for x in files]

    rhs_lines = []
    for line in change.RightHandSideLines():
      rhs_lines.append(line)
    self.assertEquals(rhs_lines[0][0].LocalPath(), files[0][1])
    self.assertEquals(rhs_lines[0][1], 1)
    self.assertEquals(rhs_lines[0][2],'boo!')

    self.assertEquals(rhs_lines[1][0].LocalPath(), files[0][1])
    self.assertEquals(rhs_lines[1][1], 2)
    self.assertEquals(rhs_lines[1][2], 'ahh?')

    self.assertEquals(rhs_lines[2][0].LocalPath(), files[3][1])
    self.assertEquals(rhs_lines[2][1], 1)
    self.assertEquals(rhs_lines[2][2], 'look!')

    self.assertEquals(rhs_lines[3][0].LocalPath(), files[3][1])
    self.assertEquals(rhs_lines[3][1], 2)
    self.assertEquals(rhs_lines[3][2], 'there?')

  def testExecPresubmitScript(self):
    description_lines = ('Hello there',
                         'this is a change',
                         'STORY=http://tracker/123')
    files = [
      ['A', 'foo\\blat.cc'],
    ]
    fake_presubmit = presubmit.os.path.join(self.fake_root_dir, 'PRESUBMIT.py')
    self.mox.ReplayAll()

    change = presubmit.Change('mychange', '\n'.join(description_lines),
                              self.fake_root_dir, files, 0, 0)
    executer = presubmit.PresubmitExecuter(change, False)
    self.failIf(executer.ExecPresubmitScript('', fake_presubmit))
    # No error if no on-upload entry point
    self.failIf(executer.ExecPresubmitScript(
      ('def CheckChangeOnCommit(input_api, output_api):\n'
       '  return (output_api.PresubmitError("!!"))\n'),
      fake_presubmit
    ))

    executer = presubmit.PresubmitExecuter(change, True)
    # No error if no on-commit entry point
    self.failIf(executer.ExecPresubmitScript(
      ('def CheckChangeOnUpload(input_api, output_api):\n'
       '  return (output_api.PresubmitError("!!"))\n'),
      fake_presubmit
    ))

    self.failIf(executer.ExecPresubmitScript(
      ('def CheckChangeOnUpload(input_api, output_api):\n'
       '  if not input_api.change.STORY:\n'
       '    return (output_api.PresubmitError("!!"))\n'
       '  else:\n'
       '    return ()'),
      fake_presubmit
    ))

    self.failUnless(executer.ExecPresubmitScript(
      ('def CheckChangeOnCommit(input_api, output_api):\n'
       '  if not input_api.change.NOSUCHKEY:\n'
       '    return [output_api.PresubmitError("!!")]\n'
       '  else:\n'
       '    return ()'),
      fake_presubmit
    ))

    self.assertRaises(exceptions.RuntimeError,
      executer.ExecPresubmitScript,
      'def CheckChangeOnCommit(input_api, output_api):\n'
      '  return "foo"',
      fake_presubmit)

    self.assertRaises(exceptions.RuntimeError,
      executer.ExecPresubmitScript,
      'def CheckChangeOnCommit(input_api, output_api):\n'
      '  return ["foo"]',
      fake_presubmit)

  def testDoPresubmitChecks(self):
    join = presubmit.os.path.join
    description_lines = ('Hello there',
                         'this is a change',
                         'STORY=http://tracker/123')
    files = [
      ['A', join('haspresubmit', 'blat.cc')],
    ]
    haspresubmit_path = join(self.fake_root_dir, 'haspresubmit', 'PRESUBMIT.py')
    root_path = join(self.fake_root_dir, 'PRESUBMIT.py')
    presubmit.os.path.isfile(root_path).AndReturn(True)
    presubmit.os.path.isfile(haspresubmit_path).AndReturn(True)
    presubmit.gcl.ReadFile(root_path,
                           'rU').AndReturn(self.presubmit_text)
    presubmit.gcl.ReadFile(haspresubmit_path,
                           'rU').AndReturn(self.presubmit_text)
    self.mox.ReplayAll()

    output = StringIO.StringIO()
    input = StringIO.StringIO('y\n')
    change = presubmit.Change('mychange', '\n'.join(description_lines),
                              self.fake_root_dir, files, 0, 0)
    self.failIf(presubmit.DoPresubmitChecks(change, False, True, output, input,
                                            None, False))
    self.assertEqual(output.getvalue().count('!!'), 2)

  def testDoPresubmitChecksPromptsAfterWarnings(self):
    join = presubmit.os.path.join
    description_lines = ('Hello there',
                         'this is a change',
                         'NOSUCHKEY=http://tracker/123')
    files = [
      ['A', join('haspresubmit', 'blat.cc')],
    ]
    presubmit_path = join(self.fake_root_dir, 'PRESUBMIT.py')
    haspresubmit_path = join(self.fake_root_dir, 'haspresubmit', 'PRESUBMIT.py')
    for i in range(2):
      presubmit.os.path.isfile(presubmit_path).AndReturn(True)
      presubmit.os.path.isfile(haspresubmit_path).AndReturn(True)
      presubmit.gcl.ReadFile(presubmit_path, 'rU'
          ).AndReturn(self.presubmit_text)
      presubmit.gcl.ReadFile(haspresubmit_path, 'rU'
          ).AndReturn(self.presubmit_text)
    self.mox.ReplayAll()

    output = StringIO.StringIO()
    input = StringIO.StringIO('n\n')  # say no to the warning
    change = presubmit.Change('mychange', '\n'.join(description_lines),
                              self.fake_root_dir, files, 0, 0)
    self.failIf(presubmit.DoPresubmitChecks(change, False, True, output, input,
                                            None, True))
    self.assertEqual(output.getvalue().count('??'), 2)

    output = StringIO.StringIO()
    input = StringIO.StringIO('y\n')  # say yes to the warning
    self.failUnless(presubmit.DoPresubmitChecks(change, False, True, output,
                                                input, None, True))
    self.assertEquals(output.getvalue().count('??'), 2)

  def testDoPresubmitChecksNoWarningPromptIfErrors(self):
    join = presubmit.os.path.join
    description_lines = ('Hello there',
                         'this is a change',
                         'NOSUCHKEY=http://tracker/123',
                         'REALLYNOSUCHKEY=http://tracker/123')
    files = [
      ['A', join('haspresubmit', 'blat.cc')],
    ]
    presubmit_path = join(self.fake_root_dir, 'PRESUBMIT.py')
    haspresubmit_path = join(self.fake_root_dir, 'haspresubmit',
                             'PRESUBMIT.py')
    presubmit.os.path.isfile(presubmit_path).AndReturn(True)
    presubmit.os.path.isfile(haspresubmit_path).AndReturn(True)
    presubmit.gcl.ReadFile(presubmit_path, 'rU').AndReturn(self.presubmit_text)
    presubmit.gcl.ReadFile(haspresubmit_path, 'rU').AndReturn(
        self.presubmit_text)
    self.mox.ReplayAll()

    output = StringIO.StringIO()
    input = StringIO.StringIO()  # should be unused
    change = presubmit.Change('mychange', '\n'.join(description_lines),
                              self.fake_root_dir, files, 0, 0)
    self.failIf(presubmit.DoPresubmitChecks(change, False, True, output, input,
                                            None, False))
    self.assertEqual(output.getvalue().count('??'), 2)
    self.assertEqual(output.getvalue().count('XX!!XX'), 2)
    self.assertEqual(output.getvalue().count('(y/N)'), 0)

  def testDoDefaultPresubmitChecks(self):
    join = presubmit.os.path.join
    description_lines = ('Hello there',
                         'this is a change',
                         'STORY=http://tracker/123')
    files = [
      ['A', join('haspresubmit', 'blat.cc')],
    ]
    DEFAULT_SCRIPT = """
def CheckChangeOnUpload(input_api, output_api):
  return [output_api.PresubmitError("!!")]
def CheckChangeOnCommit(input_api, output_api):
  raise Exception("Test error")
"""
    presubmit.os.path.isfile(join(self.fake_root_dir, 'PRESUBMIT.py')
        ).AndReturn(False)
    presubmit.os.path.isfile(join(self.fake_root_dir,
                                  'haspresubmit',
                                  'PRESUBMIT.py')).AndReturn(False)
    self.mox.ReplayAll()
    
    output = StringIO.StringIO()
    input = StringIO.StringIO('y\n')
    # Always fail.
    change = presubmit.Change('mychange', '\n'.join(description_lines),
                              self.fake_root_dir, files, 0, 0)
    self.failIf(presubmit.DoPresubmitChecks(change, False, True, output, input,
                                            DEFAULT_SCRIPT, False))
    self.assertEquals(output.getvalue().count('!!'), 1)

  def testDirectoryHandling(self):
    files = [
      ['A', 'isdir'],
      ['A', 'isdir\\blat.cc'],
    ]
    isdir = presubmit.os.path.join(self.fake_root_dir, 'isdir')
    blat = presubmit.os.path.join(isdir, 'blat.cc')
    presubmit.os.path.exists(isdir).AndReturn(True)
    presubmit.os.path.isdir(isdir).AndReturn(True)
    presubmit.os.path.exists(blat).AndReturn(True)
    presubmit.os.path.isdir(blat).AndReturn(False)
    self.mox.ReplayAll()

    change = presubmit.Change('mychange', 'foo', self.fake_root_dir, files,
                              0, 0)
    affected_files = change.AffectedFiles(include_dirs=False)
    self.failUnless(len(affected_files) == 1)
    self.failUnless(affected_files[0].LocalPath().endswith('blat.cc'))
    affected_files_and_dirs = change.AffectedFiles(include_dirs=True)
    self.failUnless(len(affected_files_and_dirs) == 2)

  def testTags(self):
    DEFAULT_SCRIPT = """
def CheckChangeOnUpload(input_api, output_api):
  if input_api.change.tags['BUG'] != 'boo':
    return [output_api.PresubmitError('Tag parsing failed. 1')]
  if input_api.change.tags['STORY'] != 'http://tracker.com/42':
    return [output_api.PresubmitError('Tag parsing failed. 2')]
  if input_api.change.BUG != 'boo':
    return [output_api.PresubmitError('Tag parsing failed. 6')]
  if input_api.change.STORY != 'http://tracker.com/42':
    return [output_api.PresubmitError('Tag parsing failed. 7')]
  try:
    y = False
    x = input_api.change.invalid
  except AttributeError:
    y = True
  if not y:
    return [output_api.PresubmitError('Tag parsing failed. 8')]
  if 'TEST' in input_api.change.tags:
    return [output_api.PresubmitError('Tag parsing failed. 3')]
  if input_api.change.DescriptionText() != 'Blah Blah':
    return [output_api.PresubmitError('Tag parsing failed. 4 ' + 
                                      input_api.change.DescriptionText())]
  if (input_api.change.FullDescriptionText() !=
      'Blah Blah\\n\\nSTORY=http://tracker.com/42\\nBUG=boo\\n'):
    return [output_api.PresubmitError('Tag parsing failed. 5 ' +
                                      input_api.change.FullDescriptionText())]
  return [output_api.PresubmitNotifyResult(input_api.change.tags['STORY'])]
def CheckChangeOnCommit(input_api, output_api):
  raise Exception("Test error")
"""
    self.mox.ReplayAll()

    output = StringIO.StringIO()
    input = StringIO.StringIO('y\n')
    change = presubmit.Change(
        'foo', "Blah Blah\n\nSTORY=http://tracker.com/42\nBUG=boo\n",
        self.fake_root_dir, None, 0, 0)
    self.failUnless(presubmit.DoPresubmitChecks(change, False, True, output,
                                                input, DEFAULT_SCRIPT, False))
    self.assertEquals(output.getvalue(),
                      ('Warning, no presubmit.py found.\n'
                       'Running default presubmit script.\n'
                       '** Presubmit Messages **\n'
                       'http://tracker.com/42\n\n'))

  def testMain(self):
    self.mox.StubOutWithMock(presubmit, 'DoPresubmitChecks')
    self.mox.StubOutWithMock(presubmit, 'ParseFiles')
    presubmit.os.path.isdir(presubmit.os.path.join(self.fake_root_dir, '.git')
        ).AndReturn(False)
    presubmit.os.path.isdir(presubmit.os.path.join(self.fake_root_dir, '.svn')
        ).AndReturn(False)
    #presubmit.ParseFiles([], None).AndReturn([])
    presubmit.DoPresubmitChecks(mox.IgnoreArg(), False, False,
                                mox.IgnoreArg(),
                                mox.IgnoreArg(),
                                None, False).AndReturn(False)
    self.mox.ReplayAll()
    
    self.assertEquals(True,
                      presubmit.Main(['presubmit', '--root',
                                      self.fake_root_dir]))


class InputApiUnittest(PresubmitTestsBase):
  """Tests presubmit.InputApi."""
  def testMembersChanged(self):
    self.mox.ReplayAll()
    members = [
      'AbsoluteLocalPaths', 'AffectedFiles', 'AffectedSourceFiles',
      'AffectedTextFiles',
      'DEFAULT_BLACK_LIST', 'DEFAULT_WHITE_LIST',
      'DepotToLocalPath', 'FilterSourceFile', 'LocalPaths',
      'LocalToDepotPath',
      'PresubmitLocalPath', 'ReadFile', 'RightHandSideLines', 'ServerPaths',
      'basename', 'cPickle', 'cStringIO', 'canned_checks', 'change',
      'is_committing', 'marshal', 'os_path', 'pickle', 'platform',
      're', 'subprocess', 'tempfile', 'traceback', 'unittest', 'urllib2',
      'version',
    ]
    # If this test fails, you should add the relevant test.
    self.compareMembers(presubmit.InputApi(None, './.', False), members)

  def testDepotToLocalPath(self):
    presubmit.gclient.CaptureSVNInfo('svn://foo/smurf').AndReturn(
        {'Path': 'prout'})
    presubmit.gclient.CaptureSVNInfo('svn:/foo/notfound/burp').AndReturn({})
    self.mox.ReplayAll()

    path = presubmit.InputApi(None, './p', False).DepotToLocalPath(
        'svn://foo/smurf')
    self.failUnless(path == 'prout')
    path = presubmit.InputApi(None, './p', False).DepotToLocalPath(
        'svn:/foo/notfound/burp')
    self.failUnless(path == None)

  def testLocalToDepotPath(self):
    presubmit.gclient.CaptureSVNInfo('smurf').AndReturn({'URL': 'svn://foo'})
    presubmit.gclient.CaptureSVNInfo('notfound-food').AndReturn({})
    self.mox.ReplayAll()

    path = presubmit.InputApi(None, './p', False).LocalToDepotPath('smurf')
    self.assertEqual(path, 'svn://foo')
    path = presubmit.InputApi(None, './p', False).LocalToDepotPath(
        'notfound-food')
    self.failUnless(path == None)

  def testInputApiConstruction(self):
    self.mox.ReplayAll()
    # Fudge the change object, it's not used during construction anyway
    api = presubmit.InputApi(change=42, presubmit_path='foo/path/PRESUBMIT.py',
                             is_committing=False)
    self.assertEquals(api.PresubmitLocalPath(), 'foo/path')
    self.assertEquals(api.change, 42)

  def testInputApiPresubmitScriptFiltering(self):
    join = presubmit.os.path.join
    description_lines = ('Hello there',
                         'this is a change',
                         'BUG=123',
                         ' STORY =http://foo/  \t',
                         'and some more regular text')
    files = [
      ['A', join('foo', 'blat.cc')],
      ['M', join('foo', 'blat', 'READ_ME2')],
      ['M', join('foo', 'blat', 'binary.dll')],
      ['M', join('foo', 'blat', 'weird.xyz')],
      ['M', join('foo', 'blat', 'another.h')],
      ['M', join('foo', 'third_party', 'third.cc')],
      ['D', 'foo/mat/beingdeleted.txt'],
      ['M', 'flop/notfound.txt'],
      ['A', 'boo/flap.h'],
    ]
    blat = presubmit.normpath(join(self.fake_root_dir, files[0][1]))
    readme = presubmit.normpath(join(self.fake_root_dir, files[1][1]))
    binary = presubmit.normpath(join(self.fake_root_dir, files[2][1]))
    weird = presubmit.normpath(join(self.fake_root_dir, files[3][1]))
    another = presubmit.normpath(join(self.fake_root_dir, files[4][1]))
    third_party = presubmit.normpath(join(self.fake_root_dir, files[5][1]))
    beingdeleted = presubmit.normpath(join(self.fake_root_dir, files[6][1]))
    notfound = presubmit.normpath(join(self.fake_root_dir, files[7][1]))
    flap = presubmit.normpath(join(self.fake_root_dir, files[8][1]))
    for i in (blat, readme, binary, weird, another, third_party):
      presubmit.os.path.exists(i).AndReturn(True)
      presubmit.os.path.isdir(i).AndReturn(False)
    presubmit.os.path.exists(beingdeleted).AndReturn(False)
    presubmit.os.path.exists(notfound).AndReturn(False)
    presubmit.os.path.exists(flap).AndReturn(True)
    presubmit.os.path.isdir(flap).AndReturn(False)
    presubmit.gclient.CaptureSVNInfo(beingdeleted).AndReturn({})
    presubmit.gclient.CaptureSVNInfo(notfound).AndReturn({})
    presubmit.gcl.GetSVNFileProperty(blat, 'svn:mime-type').AndReturn(None)
    presubmit.gcl.GetSVNFileProperty(readme, 'svn:mime-type').AndReturn(None)
    presubmit.gcl.GetSVNFileProperty(binary, 'svn:mime-type').AndReturn(
        'application/octet-stream')
    presubmit.gcl.GetSVNFileProperty(weird, 'svn:mime-type').AndReturn(None)
    presubmit.gcl.GetSVNFileProperty(another, 'svn:mime-type').AndReturn(None)
    presubmit.gcl.GetSVNFileProperty(third_party, 'svn:mime-type'
        ).AndReturn(None)
    presubmit.gcl.ReadFile(blat).AndReturn('whatever\ncookie')
    presubmit.gcl.ReadFile(another).AndReturn('whatever\ncookie2')
    self.mox.ReplayAll()

    change = presubmit.SvnChange('mychange', '\n'.join(description_lines),
                                 self.fake_root_dir, files, 0, 0)
    input_api = presubmit.InputApi(change,
                                   join(self.fake_root_dir, 'foo',
                                        'PRESUBMIT.py'),
                                   False)
    # Doesn't filter much
    got_files = input_api.AffectedFiles()
    self.assertEquals(len(got_files), 7)
    self.assertEquals(got_files[0].LocalPath(), presubmit.normpath(files[0][1]))
    self.assertEquals(got_files[1].LocalPath(), presubmit.normpath(files[1][1]))
    self.assertEquals(got_files[2].LocalPath(), presubmit.normpath(files[2][1]))
    self.assertEquals(got_files[3].LocalPath(), presubmit.normpath(files[3][1]))
    self.assertEquals(got_files[4].LocalPath(), presubmit.normpath(files[4][1]))
    self.assertEquals(got_files[5].LocalPath(), presubmit.normpath(files[5][1]))
    self.assertEquals(got_files[6].LocalPath(), presubmit.normpath(files[6][1]))
    # Ignores weird because of whitelist, third_party because of blacklist,
    # binary isn't a text file and beingdeleted doesn't exist. The rest is
    # outside foo/.
    rhs_lines = [x for x in input_api.RightHandSideLines(None)]
    self.assertEquals(len(rhs_lines), 4)
    self.assertEqual(rhs_lines[0][0].LocalPath(),
                     presubmit.normpath(files[0][1]))
    self.assertEqual(rhs_lines[1][0].LocalPath(),
                     presubmit.normpath(files[0][1]))
    self.assertEqual(rhs_lines[2][0].LocalPath(),
                     presubmit.normpath(files[4][1]))
    self.assertEqual(rhs_lines[3][0].LocalPath(),
                     presubmit.normpath(files[4][1]))

  def testDefaultWhiteListBlackListFilters(self):
    def f(x):
      return presubmit.AffectedFile(x, 'M')
    files = [
      (
        [
          # To be tested.
          f('a/experimental/b'),
          f('experimental/b'),
          f('a/experimental'),
          f('a/experimental.cc'),
        ],
        [
          # Expected.
          'a/experimental',
          'a/experimental.cc',
        ],
      ),
      (
        [
          # To be tested.
          f('a/third_party/b'),
          f('third_party/b'),
          f('a/third_party'),
          f('a/third_party.cc'),
        ],
        [
          # Expected.
          'a/third_party',
          'a/third_party.cc',
        ],
      ),
      (
        [
          # To be tested.
          f('a/LOL_FILE/b'),
          f('b.c/LOL_FILE'),
          f('a/PRESUBMIT.py'),
        ],
        [
          # Expected.
          'a/LOL_FILE/b',
          'a/PRESUBMIT.py',
        ],
      ),
      (
        [
          # To be tested.
          f('a/.git'),
          f('b.c/.git'),
          f('a/.git/bleh.py'),
          f('.git/bleh.py'),
        ],
        [
          # Expected.
          'b.c/.git',
        ],
      ),
    ]
    input_api = presubmit.InputApi(None, './PRESUBMIT.py', False)
    self.mox.ReplayAll()

    self.assertEqual(len(input_api.DEFAULT_BLACK_LIST), 9)
    for item in files:
      results = filter(input_api.FilterSourceFile, item[0])
      for i in range(len(results)):
        self.assertEquals(results[i].LocalPath(),
                          presubmit.normpath(item[1][i]))
      # Same number of expected results.
      self.assertEquals(len(results), len(item[1]))

  def testCustomFilter(self):
    def FilterSourceFile(affected_file):
      return 'a' in affected_file.LocalPath()
    files = [('A', 'eeaee'), ('M', 'eeabee'), ('M', 'eebcee')]
    for (action, item) in files:
      item = presubmit.os.path.join(self.fake_root_dir, item)
      presubmit.os.path.exists(item).AndReturn(True)
      presubmit.os.path.isdir(item).AndReturn(False)
      presubmit.gcl.GetSVNFileProperty(item, 'svn:mime-type').AndReturn(None)
    self.mox.ReplayAll()

    change = presubmit.SvnChange('mychange', '', self.fake_root_dir, files, 0,
                                 0)
    input_api = presubmit.InputApi(change,
                                   presubmit.os.path.join(self.fake_root_dir,
                                                          'PRESUBMIT.py'),
                                   False)
    got_files = input_api.AffectedSourceFiles(FilterSourceFile)
    self.assertEquals(len(got_files), 2)
    self.assertEquals(got_files[0].LocalPath(), 'eeaee')
    self.assertEquals(got_files[1].LocalPath(), 'eeabee')

  def testLambdaFilter(self):
    white_list = presubmit.InputApi.DEFAULT_BLACK_LIST + (r".*?a.*?",)
    black_list = [r".*?b.*?"]
    files = [('A', 'eeaee'), ('M', 'eeabee'), ('M', 'eebcee'), ('M', 'eecaee')]
    for (action, item) in files:
      item = presubmit.os.path.join(self.fake_root_dir, item)
      presubmit.os.path.exists(item).AndReturn(True)
      presubmit.os.path.isdir(item).AndReturn(False)
      presubmit.gcl.GetSVNFileProperty(item, 'svn:mime-type').AndReturn(None)
    self.mox.ReplayAll()

    change = presubmit.SvnChange('mychange', '', self.fake_root_dir, files, 0,
                                 0)
    input_api = presubmit.InputApi(change, './PRESUBMIT.py', False)
    # Sample usage of overiding the default white and black lists.
    got_files = input_api.AffectedSourceFiles(
        lambda x: input_api.FilterSourceFile(x, white_list, black_list))
    self.assertEquals(len(got_files), 2)
    self.assertEquals(got_files[0].LocalPath(), 'eeaee')
    self.assertEquals(got_files[1].LocalPath(), 'eecaee')

  def testGetAbsoluteLocalPath(self):
    join = presubmit.os.path.join
    normpath = presubmit.normpath
    # Regression test for bug of presubmit stuff that relies on invoking
    # SVN (e.g. to get mime type of file) not working unless gcl invoked
    # from the client root (e.g. if you were at 'src' and did 'cd base' before
    # invoking 'gcl upload' it would fail because svn wouldn't find the files
    # the presubmit script was asking about).
    files = [
      ['A', 'isdir'],
      ['A', join('isdir', 'blat.cc')],
      ['M', join('elsewhere', 'ouf.cc')],
    ]
    self.mox.ReplayAll()

    change = presubmit.Change('mychange', '', self.fake_root_dir, files, 0, 0)
    affected_files = change.AffectedFiles(include_dirs=True)
    # Local paths should remain the same
    self.assertEquals(affected_files[0].LocalPath(), normpath('isdir'))
    self.assertEquals(affected_files[1].LocalPath(), normpath('isdir/blat.cc'))
    # Absolute paths should be prefixed
    self.assertEquals(affected_files[0].AbsoluteLocalPath(),
                      normpath(join(self.fake_root_dir, 'isdir')))
    self.assertEquals(affected_files[1].AbsoluteLocalPath(),
                      normpath(join(self.fake_root_dir, 'isdir/blat.cc')))

    # New helper functions need to work
    paths_from_change = change.AbsoluteLocalPaths(include_dirs=True)
    self.assertEqual(len(paths_from_change), 3)
    presubmit_path = join(self.fake_root_dir, 'isdir', 'PRESUBMIT.py')
    api = presubmit.InputApi(change=change,
                             presubmit_path=presubmit_path,
                             is_committing=True)
    paths_from_api = api.AbsoluteLocalPaths(include_dirs=True)
    self.assertEqual(len(paths_from_api), 2)
    for absolute_paths in [paths_from_change, paths_from_api]:
      self.assertEqual(absolute_paths[0],
                       normpath(join(self.fake_root_dir, 'isdir')))
      self.assertEqual(absolute_paths[1],
                       normpath(join(self.fake_root_dir, 'isdir', 'blat.cc')))

  def testDeprecated(self):
    presubmit.warnings.warn(mox.IgnoreArg(), category=mox.IgnoreArg(),
                            stacklevel=2)
    self.mox.ReplayAll()

    change = presubmit.Change('mychange', '', self.fake_root_dir, [], 0, 0)
    api = presubmit.InputApi(
        change,
        presubmit.os.path.join(self.fake_root_dir, 'foo', 'PRESUBMIT.py'), True)
    api.AffectedTextFiles(include_deletes=False)

  def testReadFileStringDenied(self):
    self.mox.ReplayAll()

    change = presubmit.Change('foo', 'foo', self.fake_root_dir, [('M', 'AA')],
                              0, 0)
    input_api = presubmit.InputApi(
        change, presubmit.os.path.join(self.fake_root_dir, '/p'), False)
    self.assertRaises(IOError, input_api.ReadFile, 'boo', 'x')

  def testReadFileStringAccepted(self):
    path = presubmit.os.path.join(self.fake_root_dir, 'AA/boo')
    presubmit.gcl.ReadFile(path, 'x').AndReturn(None)
    self.mox.ReplayAll()

    change = presubmit.Change('foo', 'foo', self.fake_root_dir, [('M', 'AA')],
                              0, 0)
    input_api = presubmit.InputApi(
        change, presubmit.os.path.join(self.fake_root_dir, '/p'), False)
    input_api.ReadFile(path, 'x')

  def testReadFileAffectedFileDenied(self):
    file = presubmit.AffectedFile('boo', 'M', 'Unrelated')
    self.mox.ReplayAll()

    change = presubmit.Change('foo', 'foo', self.fake_root_dir, [('M', 'AA')],
                              0, 0)
    input_api = presubmit.InputApi(
        change, presubmit.os.path.join(self.fake_root_dir, '/p'), False)
    self.assertRaises(IOError, input_api.ReadFile, file, 'x')

  def testReadFileAffectedFileAccepted(self):
    file = presubmit.AffectedFile('AA/boo', 'M', self.fake_root_dir)
    presubmit.gcl.ReadFile(file.AbsoluteLocalPath(), 'x').AndReturn(None)
    self.mox.ReplayAll()

    change = presubmit.Change('foo', 'foo', self.fake_root_dir, [('M', 'AA')],
                              0, 0)
    input_api = presubmit.InputApi(
        change, presubmit.os.path.join(self.fake_root_dir, '/p'), False)
    input_api.ReadFile(file, 'x')


class OuputApiUnittest(PresubmitTestsBase):
  """Tests presubmit.OutputApi."""
  def testMembersChanged(self):
    self.mox.ReplayAll()
    members = [
      'MailTextResult', 'PresubmitError', 'PresubmitNotifyResult',
      'PresubmitPromptWarning', 'PresubmitResult',
    ]
    # If this test fails, you should add the relevant test.
    self.compareMembers(presubmit.OutputApi(), members)

  def testOutputApiBasics(self):
    self.mox.ReplayAll()
    self.failUnless(presubmit.OutputApi.PresubmitError('').IsFatal())
    self.failIf(presubmit.OutputApi.PresubmitError('').ShouldPrompt())

    self.failIf(presubmit.OutputApi.PresubmitPromptWarning('').IsFatal())
    self.failUnless(
        presubmit.OutputApi.PresubmitPromptWarning('').ShouldPrompt())

    self.failIf(presubmit.OutputApi.PresubmitNotifyResult('').IsFatal())
    self.failIf(presubmit.OutputApi.PresubmitNotifyResult('').ShouldPrompt())

    # TODO(joi) Test MailTextResult once implemented.

  def testOutputApiHandling(self):
    self.mox.ReplayAll()
    output = StringIO.StringIO()
    unused_input = StringIO.StringIO()
    error = presubmit.OutputApi.PresubmitError('!!!')
    self.failIf(error._Handle(output, unused_input))
    self.failUnless(output.getvalue().count('!!!'))

    output = StringIO.StringIO()
    notify = presubmit.OutputApi.PresubmitNotifyResult('?see?')
    self.failUnless(notify._Handle(output, unused_input))
    self.failUnless(output.getvalue().count('?see?'))

    output = StringIO.StringIO()
    input = StringIO.StringIO('y')
    warning = presubmit.OutputApi.PresubmitPromptWarning('???')
    self.failUnless(warning._Handle(output, input))
    self.failUnless(output.getvalue().count('???'))

    output = StringIO.StringIO()
    input = StringIO.StringIO('n')
    warning = presubmit.OutputApi.PresubmitPromptWarning('???')
    self.failIf(warning._Handle(output, input))
    self.failUnless(output.getvalue().count('???'))

    output = StringIO.StringIO()
    input = StringIO.StringIO('\n')
    warning = presubmit.OutputApi.PresubmitPromptWarning('???')
    self.failIf(warning._Handle(output, input))
    self.failUnless(output.getvalue().count('???'))


class AffectedFileUnittest(PresubmitTestsBase):
  def testMembersChanged(self):
    self.mox.ReplayAll()
    members = [
      'AbsoluteLocalPath', 'Action', 'IsDirectory', 'IsTextFile', 'LocalPath',
      'NewContents', 'OldContents', 'OldFileTempPath', 'Property', 'ServerPath',
      'scm',
    ]
    # If this test fails, you should add the relevant test.
    self.compareMembers(presubmit.AffectedFile('a', 'b'), members)
    self.compareMembers(presubmit.SvnAffectedFile('a', 'b'), members)

  def testAffectedFile(self):
    path = presubmit.os.path.join('foo', 'blat.cc')
    presubmit.os.path.exists(path).AndReturn(True)
    presubmit.os.path.isdir(path).AndReturn(False)
    presubmit.gcl.ReadFile(path).AndReturn('whatever\ncookie')
    presubmit.gclient.CaptureSVNInfo(path).AndReturn(
        {'URL': 'svn:/foo/foo/blat.cc'})
    self.mox.ReplayAll()
    af = presubmit.SvnAffectedFile('foo/blat.cc', 'M')
    self.failUnless(af.ServerPath() == 'svn:/foo/foo/blat.cc')
    self.failUnless(af.LocalPath() == presubmit.normpath('foo/blat.cc'))
    self.failUnless(af.Action() == 'M')
    self.assertEquals(af.NewContents(), ['whatever', 'cookie'])
    af = presubmit.AffectedFile('notfound.cc', 'A')
    self.failUnless(af.ServerPath() == '')

  def testProperty(self):
    presubmit.gcl.GetSVNFileProperty('foo.cc', 'svn:secret-property'
        ).AndReturn('secret-property-value')
    self.mox.ReplayAll()
    affected_file = presubmit.SvnAffectedFile('foo.cc', 'A')
    # Verify cache coherency.
    self.failUnless(affected_file.Property('svn:secret-property') ==
                    'secret-property-value')
    self.failUnless(affected_file.Property('svn:secret-property') ==
                    'secret-property-value')

  def testIsDirectoryNotExists(self):
    presubmit.os.path.exists('foo.cc').AndReturn(False)
    presubmit.gclient.CaptureSVNInfo('foo.cc').AndReturn({})
    self.mox.ReplayAll()
    affected_file = presubmit.SvnAffectedFile('foo.cc', 'A')
    # Verify cache coherency.
    self.failIf(affected_file.IsDirectory())
    self.failIf(affected_file.IsDirectory())

  def testIsDirectory(self):
    presubmit.os.path.exists('foo.cc').AndReturn(True)
    presubmit.os.path.isdir('foo.cc').AndReturn(True)
    self.mox.ReplayAll()
    affected_file = presubmit.SvnAffectedFile('foo.cc', 'A')
    # Verify cache coherency.
    self.failUnless(affected_file.IsDirectory())
    self.failUnless(affected_file.IsDirectory())

  def testIsTextFile(self):
    list = [presubmit.SvnAffectedFile('foo/blat.txt', 'M'),
            presubmit.SvnAffectedFile('foo/binary.blob', 'M'),
            presubmit.SvnAffectedFile('blat/flop.txt', 'D')]
    blat = presubmit.os.path.join('foo', 'blat.txt')
    blob = presubmit.os.path.join('foo', 'binary.blob')
    presubmit.os.path.exists(blat).AndReturn(True)
    presubmit.os.path.isdir(blat).AndReturn(False)
    presubmit.os.path.exists(blob).AndReturn(True)
    presubmit.os.path.isdir(blob).AndReturn(False)
    presubmit.gcl.GetSVNFileProperty(blat, 'svn:mime-type').AndReturn(None)
    presubmit.gcl.GetSVNFileProperty(blob, 'svn:mime-type'
        ).AndReturn('application/octet-stream')
    self.mox.ReplayAll()

    output = filter(lambda x: x.IsTextFile(), list)
    self.failUnless(len(output) == 1)
    self.failUnless(list[0] == output[0])


class GclChangeUnittest(PresubmitTestsBase):
  def testMembersChanged(self):
    members = [
        'AbsoluteLocalPaths', 'AffectedFiles', 'AffectedTextFiles',
        'DescriptionText', 'FullDescriptionText', 'LocalPaths', 'Name',
        'RepositoryRoot', 'RightHandSideLines', 'ServerPaths',
        'issue', 'patchset', 'tags',
    ]
    # If this test fails, you should add the relevant test.
    self.mox.ReplayAll()

    change = presubmit.Change('foo', 'foo', self.fake_root_dir, [('M', 'AA')],
                              0, 0)
    self.compareMembers(change, members)


class CannedChecksUnittest(PresubmitTestsBase):
  """Tests presubmit_canned_checks.py."""

  def setUp(self):
    PresubmitTestsBase.setUp(self)
    self.mox.StubOutWithMock(presubmit_canned_checks,
                         '_RunPythonUnitTests_LoadTests')

  def MockInputApi(self, change, committing):
    input_api = self.mox.CreateMock(presubmit.InputApi)
    input_api.cStringIO = presubmit.cStringIO
    input_api.re = presubmit.re
    input_api.traceback = presubmit.traceback
    input_api.urllib2 = self.mox.CreateMock(presubmit.urllib2)
    input_api.unittest = unittest
    input_api.change = change
    input_api.is_committing = committing
    return input_api

  def testMembersChanged(self):
    self.mox.ReplayAll()
    members = [
      'CheckChangeHasBugField', 'CheckChangeHasDescription',
      'CheckChangeHasNoStrayWhitespace',
      'CheckChangeHasOnlyOneEol', 'CheckChangeHasNoCR',
      'CheckChangeHasNoCrAndHasOnlyOneEol', 'CheckChangeHasNoTabs',
      'CheckChangeHasQaField', 'CheckChangeHasTestedField',
      'CheckChangeHasTestField', 'CheckChangeSvnEolStyle',
      'CheckDoNotSubmit',
      'CheckDoNotSubmitInDescription', 'CheckDoNotSubmitInFiles',
      'CheckLongLines', 'CheckTreeIsOpen', 'RunPythonUnitTests',
    ]
    # If this test fails, you should add the relevant test.
    self.compareMembers(presubmit_canned_checks, members)

  def DescriptionTest(self, check, description1, description2, error_type,
                      committing):
    change1 = presubmit.Change('foo1', description1, self.fake_root_dir, None,
                               0, 0)
    input_api1 = self.MockInputApi(change1, committing)
    change2 = presubmit.Change('foo2', description2, self.fake_root_dir, None,
                               0, 0)
    input_api2 = self.MockInputApi(change2, committing)
    self.mox.ReplayAll()

    results1 = check(input_api1, presubmit.OutputApi)
    self.assertEquals(results1, [])
    results2 = check(input_api2, presubmit.OutputApi)
    self.assertEquals(len(results2), 1)
    self.assertEquals(results2[0].__class__, error_type)

  def ContentTest(self, check, content1, content2, error_type):
    change1 = presubmit.Change('foo1', 'foo1\n', self.fake_root_dir, None,
                               0, 0)
    input_api1 = self.MockInputApi(change1, False)
    affected_file = self.mox.CreateMock(presubmit.SvnAffectedFile)
    affected_file.LocalPath().AndReturn('foo.cc')
    output1 = [
      (affected_file, 42, 'yo, ' + content1),
      (affected_file, 43, 'yer'),
      (affected_file, 23, 'ya'),
    ]
    input_api1.RightHandSideLines(mox.IgnoreArg()).AndReturn(output1)
    change2 = presubmit.Change('foo2', 'foo2\n', self.fake_root_dir, None,
                               0, 0)
    input_api2 = self.MockInputApi(change2, False)
    output2 = [
      (affected_file, 42, 'yo, ' + content2),
      (affected_file, 43, 'yer'),
      (affected_file, 23, 'ya'),
    ]
    input_api2.RightHandSideLines(mox.IgnoreArg()).AndReturn(output2)
    self.mox.ReplayAll()

    results1 = check(input_api1, presubmit.OutputApi, None)
    self.assertEquals(results1, [])
    results2 = check(input_api2, presubmit.OutputApi, None)
    self.assertEquals(len(results2), 1)
    self.assertEquals(results2[0].__class__, error_type)

  def ReadFileTest(self, check, content1, content2, error_type):
    self.mox.StubOutWithMock(presubmit.InputApi, 'ReadFile')
    change1 = presubmit.Change('foo1', 'foo1\n', self.fake_root_dir, None,
                               0, 0)
    input_api1 = self.MockInputApi(change1, False)
    affected_file1 = self.mox.CreateMock(presubmit.SvnAffectedFile)
    input_api1.AffectedSourceFiles(None).AndReturn([affected_file1])
    input_api1.ReadFile(affected_file1, 'rb').AndReturn(content1)
    change2 = presubmit.Change('foo2', 'foo2\n', self.fake_root_dir, None,
                               0, 0)
    input_api2 = self.MockInputApi(change2, False)
    affected_file2 = self.mox.CreateMock(presubmit.SvnAffectedFile)
    input_api2.AffectedSourceFiles(None).AndReturn([affected_file2])
    input_api2.ReadFile(affected_file2, 'rb').AndReturn(content2)
    affected_file2.LocalPath().AndReturn('bar.cc')
    self.mox.ReplayAll()

    results = check(input_api1, presubmit.OutputApi)
    self.assertEquals(results, [])
    results2 = check(input_api2, presubmit.OutputApi)
    self.assertEquals(len(results2), 1)
    self.assertEquals(results2[0].__class__, error_type)

  def SvnPropertyTest(self, check, property, value1, value2, committing,
                      error_type):
    input_api1 = self.MockInputApi(None, committing)
    files1 = [
      presubmit.SvnAffectedFile('foo/bar.cc', 'A'),
      presubmit.SvnAffectedFile('foo.cc', 'M'),
    ]
    input_api1.AffectedSourceFiles(None).AndReturn(files1)
    presubmit.gcl.GetSVNFileProperty(presubmit.normpath('foo/bar.cc'),
                                     property).AndReturn(value1)
    presubmit.gcl.GetSVNFileProperty(presubmit.normpath('foo.cc'),
                                     property).AndReturn(value1)
    input_api2 = self.MockInputApi(None, committing)
    files2 = [
      presubmit.SvnAffectedFile('foo/bar.cc', 'A'),
      presubmit.SvnAffectedFile('foo.cc', 'M'),
    ]
    input_api2.AffectedSourceFiles(None).AndReturn(files2)
    presubmit.gcl.GetSVNFileProperty(presubmit.normpath('foo/bar.cc'),
                                     property).AndReturn(value2)
    presubmit.gcl.GetSVNFileProperty(presubmit.normpath('foo.cc'),
                                     property).AndReturn(value2)
    self.mox.ReplayAll()

    results1 = check(input_api1, presubmit.OutputApi, None)
    self.assertEquals(results1, [])
    results2 = check(input_api2, presubmit.OutputApi, None)
    self.assertEquals(len(results2), 1)
    self.assertEquals(results2[0].__class__, error_type)

  def testCannedCheckChangeHasBugField(self):
    self.DescriptionTest(presubmit_canned_checks.CheckChangeHasBugField,
                         'Foo\nBUG=1234', 'Foo\n',
                         presubmit.OutputApi.PresubmitNotifyResult,
                         False)

  def testCheckChangeHasDescription(self):
    self.DescriptionTest(presubmit_canned_checks.CheckChangeHasDescription,
                         'Bleh', '',
                         presubmit.OutputApi.PresubmitNotifyResult,
                         False)
    self.mox.VerifyAll()
    self.DescriptionTest(presubmit_canned_checks.CheckChangeHasDescription,
                         'Bleh', '',
                         presubmit.OutputApi.PresubmitError,
                         True)
  
  def testCannedCheckChangeHasTestField(self):
    self.DescriptionTest(presubmit_canned_checks.CheckChangeHasTestField,
                         'Foo\nTEST=did some stuff', 'Foo\n',
                         presubmit.OutputApi.PresubmitNotifyResult,
                         False)

  def testCannedCheckChangeHasTestedField(self):
    self.DescriptionTest(presubmit_canned_checks.CheckChangeHasTestedField,
                         'Foo\nTESTED=did some stuff', 'Foo\n',
                         presubmit.OutputApi.PresubmitError,
                         False)

  def testCannedCheckChangeHasQAField(self):
    self.DescriptionTest(presubmit_canned_checks.CheckChangeHasQaField,
                         'Foo\nQA=BSOD your machine', 'Foo\n',
                         presubmit.OutputApi.PresubmitError,
                         False)

  def testCannedCheckDoNotSubmitInDescription(self):
    self.DescriptionTest(presubmit_canned_checks.CheckDoNotSubmitInDescription,
                         'Foo\nDO NOTSUBMIT', 'Foo\nDO NOT ' + 'SUBMIT',
                         presubmit.OutputApi.PresubmitError,
                         False)

  def testCannedCheckDoNotSubmitInFiles(self):
    self.ContentTest(
        lambda x,y,z: presubmit_canned_checks.CheckDoNotSubmitInFiles(x, y),
        'DO NOTSUBMIT', 'DO NOT ' + 'SUBMIT',
        presubmit.OutputApi.PresubmitError)

  def testCheckChangeHasNoStrayWhitespace(self):
    self.ContentTest(
        lambda x,y,z:
            presubmit_canned_checks.CheckChangeHasNoStrayWhitespace(x, y),
        'Foo', 'Foo ',
        presubmit.OutputApi.PresubmitPromptWarning)
    

  def testCheckChangeHasOnlyOneEol(self):
    self.ReadFileTest(presubmit_canned_checks.CheckChangeHasOnlyOneEol,
                      "Hey!\nHo!\n", "Hey!\nHo!\n\n",
                      presubmit.OutputApi.PresubmitPromptWarning)

  def testCheckChangeHasNoCR(self):
    self.ReadFileTest(presubmit_canned_checks.CheckChangeHasNoCR,
                      "Hey!\nHo!\n", "Hey!\r\nHo!\r\n",
                      presubmit.OutputApi.PresubmitPromptWarning)
 
  def testCheckChangeHasNoCrAndHasOnlyOneEol(self):
    self.ReadFileTest(
        presubmit_canned_checks.CheckChangeHasNoCrAndHasOnlyOneEol,
        "Hey!\nHo!\n", "Hey!\nHo!\n\n",
        presubmit.OutputApi.PresubmitPromptWarning)
    self.mox.VerifyAll()
    self.ReadFileTest(
        presubmit_canned_checks.CheckChangeHasNoCrAndHasOnlyOneEol,
        "Hey!\nHo!\n", "Hey!\r\nHo!\r\n",
        presubmit.OutputApi.PresubmitPromptWarning)

  def testCannedCheckChangeHasNoTabs(self):
    self.ContentTest(presubmit_canned_checks.CheckChangeHasNoTabs,
                     'blah blah', 'blah\tblah',
                     presubmit.OutputApi.PresubmitPromptWarning)

  def testCannedCheckLongLines(self):
    check = lambda x,y,z: presubmit_canned_checks.CheckLongLines(x, y, 10, z)
    self.ContentTest(check, '', 'blah blah blah',
                     presubmit.OutputApi.PresubmitPromptWarning)

  def testCheckChangeSvnEolStyleCommit(self):
    self.SvnPropertyTest(presubmit_canned_checks.CheckChangeSvnEolStyle,
                         'svn:eol-style', 'LF', '', True,
                         presubmit.OutputApi.PresubmitError)

  def testCheckChangeSvnEolStyleUpload(self):
    self.SvnPropertyTest(presubmit_canned_checks.CheckChangeSvnEolStyle,
                         'svn:eol-style', 'LF', '', False,
                         presubmit.OutputApi.PresubmitNotifyResult)

  def testCannedCheckTreeIsOpenOpen(self):
    input_api = self.MockInputApi(None, True)
    connection = self.mox.CreateMockAnything()
    input_api.urllib2.urlopen('url_to_open').AndReturn(connection)
    connection.read().AndReturn('1')
    connection.close()
    self.mox.ReplayAll()
    results = presubmit_canned_checks.CheckTreeIsOpen(
        input_api, presubmit.OutputApi, url='url_to_open', closed='0')
    self.assertEquals(results, [])

  def testCannedCheckTreeIsOpenClosed(self):
    input_api = self.MockInputApi(None, True)
    connection = self.mox.CreateMockAnything()
    input_api.urllib2.urlopen('url_to_closed').AndReturn(connection)
    connection.read().AndReturn('0')
    connection.close()
    self.mox.ReplayAll()
    results = presubmit_canned_checks.CheckTreeIsOpen(
        input_api, presubmit.OutputApi, url='url_to_closed', closed='0')
    self.assertEquals(len(results), 1)
    self.assertEquals(results[0].__class__,
                      presubmit.OutputApi.PresubmitPromptWarning)

  def testRunPythonUnitTestsNoTest(self):
    input_api = self.MockInputApi(None, False)
    self.mox.ReplayAll()
    results = presubmit_canned_checks.RunPythonUnitTests(
        input_api, presubmit.OutputApi, [])
    self.assertEquals(results, [])

  def testRunPythonUnitTestsNonExistentUpload(self):
    input_api = self.MockInputApi(None, False)
    presubmit_canned_checks._RunPythonUnitTests_LoadTests(
        input_api, '_non_existent_module').AndRaise(
            exceptions.ImportError('Blehh'))
    self.mox.ReplayAll()

    results = presubmit_canned_checks.RunPythonUnitTests(
        input_api, presubmit.OutputApi, ['_non_existent_module'])
    self.assertEquals(len(results), 1)
    self.assertEquals(results[0].__class__,
                      presubmit.OutputApi.PresubmitNotifyResult)

  def testRunPythonUnitTestsNonExistentCommitting(self):
    input_api = self.MockInputApi(None, True)
    presubmit_canned_checks._RunPythonUnitTests_LoadTests(
        input_api, '_non_existent_module').AndRaise(
            exceptions.ImportError('Blehh'))
    self.mox.ReplayAll()
    results = presubmit_canned_checks.RunPythonUnitTests(
        input_api, presubmit.OutputApi, ['_non_existent_module'])
    self.assertEquals(len(results), 1)
    self.assertEquals(results[0].__class__, presubmit.OutputApi.PresubmitError)

  def testRunPythonUnitTestsEmptyUpload(self):
    input_api = self.MockInputApi(None, False)
    test_module = self.mox.CreateMockAnything()
    presubmit_canned_checks._RunPythonUnitTests_LoadTests(
        input_api, 'test_module').AndReturn([])
    self.mox.ReplayAll()

    results = presubmit_canned_checks.RunPythonUnitTests(
        input_api, presubmit.OutputApi, ['test_module'])
    self.assertEquals(results, [])

  def testRunPythonUnitTestsEmptyCommitting(self):
    input_api = self.MockInputApi(None, True)
    test_module = self.mox.CreateMockAnything()
    presubmit_canned_checks._RunPythonUnitTests_LoadTests(
        input_api, 'test_module').AndReturn([])
    self.mox.ReplayAll()

    results = presubmit_canned_checks.RunPythonUnitTests(
        input_api, presubmit.OutputApi, ['test_module'])
    self.assertEquals(results, [])

  def testRunPythonUnitTestsFailureUpload(self):
    input_api = self.MockInputApi(None, False)
    input_api.unittest = self.mox.CreateMock(unittest)
    input_api.cStringIO = self.mox.CreateMock(presubmit.cStringIO)
    test = self.mox.CreateMockAnything()
    presubmit_canned_checks._RunPythonUnitTests_LoadTests(
        input_api, 'test_module').AndReturn([test])
    runner = self.mox.CreateMockAnything()
    buffer = self.mox.CreateMockAnything()
    input_api.cStringIO.StringIO().AndReturn(buffer)
    buffer.getvalue().AndReturn('BOO HOO!')
    input_api.unittest.TextTestRunner(stream=buffer, verbosity=0
        ).AndReturn(runner)
    suite = self.mox.CreateMockAnything()
    input_api.unittest.TestSuite([test]).AndReturn(suite)
    test_result = self.mox.CreateMockAnything()
    runner.run(suite).AndReturn(test_result)
    test_result.wasSuccessful().AndReturn(False)
    test_result.failures = [None, None]
    test_result.errors = [None, None, None]
    self.mox.ReplayAll()

    results = presubmit_canned_checks.RunPythonUnitTests(
        input_api, presubmit.OutputApi, ['test_module'])
    self.assertEquals(len(results), 1)
    self.assertEquals(results[0].__class__,
                      presubmit.OutputApi.PresubmitNotifyResult)
    self.assertEquals(results[0]._long_text, 'BOO HOO!')

  def testRunPythonUnitTestsFailureCommitting(self):
    input_api = self.MockInputApi(None, True)
    input_api.unittest = self.mox.CreateMock(unittest)
    input_api.cStringIO = self.mox.CreateMock(presubmit.cStringIO)
    test = self.mox.CreateMockAnything()
    presubmit_canned_checks._RunPythonUnitTests_LoadTests(
        input_api, 'test_module').AndReturn([test])
    runner = self.mox.CreateMockAnything()
    buffer = self.mox.CreateMockAnything()
    input_api.cStringIO.StringIO().AndReturn(buffer)
    buffer.getvalue().AndReturn('BOO HOO!')
    input_api.unittest.TextTestRunner(stream=buffer, verbosity=0
        ).AndReturn(runner)
    suite = self.mox.CreateMockAnything()
    input_api.unittest.TestSuite([test]).AndReturn(suite)
    test_result = self.mox.CreateMockAnything()
    runner.run(suite).AndReturn(test_result)
    test_result.wasSuccessful().AndReturn(False)
    test_result.failures = [None, None]
    test_result.errors = [None, None, None]
    self.mox.ReplayAll()

    results = presubmit_canned_checks.RunPythonUnitTests(
        input_api, presubmit.OutputApi, ['test_module'])
    self.assertEquals(len(results), 1)
    self.assertEquals(results[0].__class__, presubmit.OutputApi.PresubmitError)
    self.assertEquals(results[0]._long_text, 'BOO HOO!')

  def testRunPythonUnitTestsSuccess(self):
    input_api = self.MockInputApi(None, False)
    input_api.cStringIO = self.mox.CreateMock(presubmit.cStringIO)
    input_api.unittest = self.mox.CreateMock(unittest)
    test = self.mox.CreateMockAnything()
    presubmit_canned_checks._RunPythonUnitTests_LoadTests(
        input_api, 'test_module').AndReturn([test])
    runner = self.mox.CreateMockAnything()
    buffer = self.mox.CreateMockAnything()
    input_api.cStringIO.StringIO().AndReturn(buffer)
    input_api.unittest.TextTestRunner(stream=buffer, verbosity=0
        ).AndReturn(runner)
    suite = self.mox.CreateMockAnything()
    input_api.unittest.TestSuite([test]).AndReturn(suite)
    test_result = self.mox.CreateMockAnything()
    runner.run(suite).AndReturn(test_result)
    test_result.wasSuccessful().AndReturn(True)
    test_result.failures = 0
    test_result.errors = 0
    self.mox.ReplayAll()

    results = presubmit_canned_checks.RunPythonUnitTests(
        input_api, presubmit.OutputApi, ['test_module'])
    self.assertEquals(len(results), 0)


if __name__ == '__main__':
  unittest.main()
