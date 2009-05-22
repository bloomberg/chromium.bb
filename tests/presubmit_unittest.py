#!/usr/bin/python
# Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for presubmit.py and presubmit_canned_checks.py."""

import os
import StringIO
import sys
import unittest

# Local imports
import gcl
import gclient
import presubmit_support as presubmit
import presubmit_canned_checks


class PresubmitTestsBase(unittest.TestCase):
  """Setups and tear downs the mocks but doesn't test anything as-is."""
  def setUp(self):
    self.original_IsFile = os.path.isfile
    def MockIsFile(f):
      dir = os.path.dirname(f)
      return dir.endswith('haspresubmit') or dir == ''
    os.path.isfile = MockIsFile

    self.original_CaptureSVNInfo = gclient.CaptureSVNInfo
    def MockCaptureSVNInfo(path):
      if path.count('notfound'):
        return {}
      results = {
        'Path': path[len('svn:/foo/'):],
        'URL': 'svn:/foo/%s' % path.replace('\\', '/'),
      }
      if path.endswith('isdir'):
        results['Node Kind'] = 'directory'
      else:
        results['Node Kind'] = 'file'
      return results
    gclient.CaptureSVNInfo = MockCaptureSVNInfo

    self.original_GetSVNFileProperty = gcl.GetSVNFileProperty
    def MockGetSVNFileProperty(path, property_name):
      if property_name == 'svn:secret-property':
        return 'secret-property-value'
      elif path.count('binary'):
        return 'application/octet-stream'
      else:
        if len(path) % 2:
          return 'text/plain'
        else:
          return ''
    gcl.GetSVNFileProperty = MockGetSVNFileProperty

    self.original_ReadFile = gcl.ReadFile
    def MockReadFile(path, dummy='r'):
      if path.count('nosuchfile'):
        return None
      elif path.endswith('isdir'):
        self.fail('Should not attempt to read file that is directory.')
      elif path.endswith('PRESUBMIT.py'):
        # used in testDoPresubmitChecks
        return """
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
      else:
        return 'one:%s\r\ntwo:%s' % (path, path)
    gcl.ReadFile = MockReadFile

    self.original_GetRepositoryRoot = gcl.GetRepositoryRoot
    def MockGetRepositoryRoot():
      return ''
    gcl.GetRepositoryRoot = MockGetRepositoryRoot
    self._sys_stdout = sys.stdout
    sys.stdout = StringIO.StringIO()

  def tearDown(self):
    os.path.isfile = self.original_IsFile
    gclient.CaptureSVNInfo = self.original_CaptureSVNInfo
    gcl.GetSVNFileProperty = self.original_GetSVNFileProperty
    gcl.ReadFile = self.original_ReadFile
    gcl.GetRepositoryRoot = self.original_GetRepositoryRoot
    sys.stdout = self._sys_stdout

  @staticmethod
  def MakeBasicChange(name, description):
    ci = gcl.ChangeInfo(name=name,
                        description=description,
                        files=[])
    change = presubmit.GclChange(ci)
    return change

  def compareMembers(self, object, members):
    """If you add a member, be sure to add the relevant test!"""
    # Skip over members starting with '_' since they are usually not meant to
    # be for public use.
    actual_members = [x for x in sorted(dir(object))
                      if not x.startswith('_')]
    self.assertEqual(actual_members, sorted(members))


class PresubmitUnittest(PresubmitTestsBase):
  """General presubmit.py tests (excluding InputApi and OutputApi)."""
  def testMembersChanged(self):
    members = [
      'AffectedFile', 'DoPresubmitChecks', 'GclChange', 'InputApi',
      'ListRelevantPresubmitFiles', 'Main', 'NotImplementedException',
      'OutputApi', 'ParseFiles', 'PresubmitExecuter', 'SPECIAL_KEYS',
      'ScanSubDirs', 'cPickle', 'cStringIO', 'exceptions',
      'fnmatch', 'gcl', 'gclient', 'glob', 'marshal', 'normpath', 'optparse',
      'os', 'pickle', 'presubmit_canned_checks', 're', 'subprocess', 'sys',
      'tempfile', 'types', 'urllib2',
    ]
    # If this test fails, you should add the relevant test.
    self.compareMembers(presubmit, members)

  def testListRelevantPresubmitFiles(self):
    presubmit_files = presubmit.ListRelevantPresubmitFiles([
        'blat.cc',
        'foo/haspresubmit/yodle/smart.h',
        'moo/mat/gat/yo.h',
        'foo/luck.h'])
    self.failUnless(len(presubmit_files) == 2)
    self.failUnless(presubmit.normpath('PRESUBMIT.py') in presubmit_files)
    self.failUnless(presubmit.normpath('foo/haspresubmit/PRESUBMIT.py') in
                    presubmit_files)

  def testTagLineRe(self):
    m = presubmit._tag_line_re.match(' BUG =1223, 1445  \t')
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
      ['M', 'flop/notfound.txt'],  # not found in SVN, still exists locally
      ['D', 'boo/flap.h'],
    ]

    ci = gcl.ChangeInfo(name='mychange',
                        description='\n'.join(description_lines),
                        files=files)
    change = presubmit.GclChange(ci)

    self.failUnless(change.Change() == 'mychange')
    self.failUnless(change.Changelist() == 'mychange')
    self.failUnless(change.DescriptionText() ==
                    'Hello there\nthis is a change\nand some more regular text')
    self.failUnless(change.FullDescriptionText() ==
                    '\n'.join(description_lines))

    self.failUnless(change.BugIDs == '123')
    self.failUnless(change.BUG == '123')
    self.failUnless(change.STORY == 'http://foo/')

    self.failUnless(len(change.AffectedFiles()) == 4)
    self.failUnless(len(change.AffectedFiles(include_dirs=True)) == 5)
    self.failUnless(len(change.AffectedFiles(include_deletes=False)) == 3)
    self.failUnless(len(change.AffectedFiles(include_dirs=True,
                                             include_deletes=False)) == 4)

    affected_text_files = change.AffectedTextFiles(include_deletes=True)
    self.failUnless(len(affected_text_files) == 3)
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
    self.failUnless(
      len(filter(lambda x: x in expected_paths, server_paths)) == 4)

    files = [[x[0], presubmit.normpath(x[1])] for x in files]

    rhs_lines = []
    for line in change.RightHandSideLines():
      rhs_lines.append(line)
    self.failUnless(rhs_lines[0][0].LocalPath() == files[0][1])
    self.failUnless(rhs_lines[0][1] == 1)
    self.failUnless(rhs_lines[0][2] == 'one:%s' % files[0][1])

    self.failUnless(rhs_lines[1][0].LocalPath() == files[0][1])
    self.failUnless(rhs_lines[1][1] == 2)
    self.failUnless(rhs_lines[1][2] == 'two:%s' % files[0][1])

    self.failUnless(rhs_lines[2][0].LocalPath() == files[3][1])
    self.failUnless(rhs_lines[2][1] == 1)
    self.failUnless(rhs_lines[2][2] == 'one:%s' % files[3][1])

    self.failUnless(rhs_lines[3][0].LocalPath() == files[3][1])
    self.failUnless(rhs_lines[3][1] == 2)
    self.failUnless(rhs_lines[3][2] == 'two:%s' % files[3][1])

  def testAffectedFile(self):
    af = presubmit.AffectedFile('foo/blat.cc', 'M')
    self.failUnless(af.ServerPath() == 'svn:/foo/foo/blat.cc')
    self.failUnless(af.LocalPath() == presubmit.normpath('foo/blat.cc'))
    self.failUnless(af.Action() == 'M')
    self.failUnless(af.NewContents() == ['one:%s' % af.LocalPath(),
                                         'two:%s' % af.LocalPath()])

    af = presubmit.AffectedFile('notfound.cc', 'A')
    self.failUnless(af.ServerPath() == '')

  def testExecPresubmitScript(self):
    description_lines = ('Hello there',
                         'this is a change',
                         'STORY=http://tracker/123')
    files = [
      ['A', 'foo\\blat.cc'],
    ]
    ci = gcl.ChangeInfo(name='mychange',
                        description='\n'.join(description_lines),
                        files=files)

    executer = presubmit.PresubmitExecuter(ci, False)
    self.failIf(executer.ExecPresubmitScript('', 'PRESUBMIT.py'))
    # No error if no on-upload entry point
    self.failIf(executer.ExecPresubmitScript(
      ('def CheckChangeOnCommit(input_api, output_api):\n'
       '  return (output_api.PresubmitError("!!"))\n'),
      'PRESUBMIT.py'
    ))

    executer = presubmit.PresubmitExecuter(ci, True)
    # No error if no on-commit entry point
    self.failIf(executer.ExecPresubmitScript(
      ('def CheckChangeOnUpload(input_api, output_api):\n'
       '  return (output_api.PresubmitError("!!"))\n'),
      'PRESUBMIT.py'
    ))

    self.failIf(executer.ExecPresubmitScript(
      ('def CheckChangeOnUpload(input_api, output_api):\n'
       '  if not input_api.change.STORY:\n'
       '    return (output_api.PresubmitError("!!"))\n'
       '  else:\n'
       '    return ()'),
      'PRESUBMIT.py'
    ))

    self.failUnless(executer.ExecPresubmitScript(
      ('def CheckChangeOnCommit(input_api, output_api):\n'
       '  if not input_api.change.NOSUCHKEY:\n'
       '    return [output_api.PresubmitError("!!")]\n'
       '  else:\n'
       '    return ()'),
      'PRESUBMIT.py'
    ))

    try:
      executer.ExecPresubmitScript(
        ('def CheckChangeOnCommit(input_api, output_api):\n'
         '  return "foo"'),
        'PRESUBMIT.py')
      self.fail()
    except:
      pass  # expected case

    try:
      executer.ExecPresubmitScript(
        ('def CheckChangeOnCommit(input_api, output_api):\n'
         '  return ["foo"]'),
        'PRESUBMIT.py')
      self.fail()
    except:
      pass  # expected case

  def testDoPresubmitChecks(self):
    description_lines = ('Hello there',
                         'this is a change',
                         'STORY=http://tracker/123')
    files = [
      ['A', 'haspresubmit\\blat.cc'],
    ]
    ci = gcl.ChangeInfo(name='mychange',
                        description='\n'.join(description_lines),
                        files=files)

    output = StringIO.StringIO()
    input = StringIO.StringIO('y\n')

    self.failIf(presubmit.DoPresubmitChecks(ci, False, True, output, input,
                                            None))
    self.assertEqual(output.getvalue().count('!!'), 2)

  def testDoPresubmitChecksPromptsAfterWarnings(self):
    description_lines = ('Hello there',
                         'this is a change',
                         'NOSUCHKEY=http://tracker/123')
    files = [
      ['A', 'haspresubmit\\blat.cc'],
    ]
    ci = gcl.ChangeInfo(name='mychange',
                        description='\n'.join(description_lines),
                        files=files)

    output = StringIO.StringIO()
    input = StringIO.StringIO('n\n')  # say no to the warning

    self.failIf(presubmit.DoPresubmitChecks(ci, False, True, output, input,
                                            None))
    self.assertEqual(output.getvalue().count('??'), 2)

    output = StringIO.StringIO()
    input = StringIO.StringIO('y\n')  # say yes to the warning

    self.failUnless(presubmit.DoPresubmitChecks(ci,
                                                False,
                                                True,
                                                output,
                                                input,
                                                None))
    self.failUnless(output.getvalue().count('??'))

  def testDoPresubmitChecksNoWarningPromptIfErrors(self):
    description_lines = ('Hello there',
                         'this is a change',
                         'NOSUCHKEY=http://tracker/123',
                         'REALLYNOSUCHKEY=http://tracker/123')
    files = [
      ['A', 'haspresubmit\\blat.cc'],
    ]
    ci = gcl.ChangeInfo(name='mychange',
                        description='\n'.join(description_lines),
                        files=files)

    output = StringIO.StringIO()
    input = StringIO.StringIO()  # should be unused

    self.failIf(presubmit.DoPresubmitChecks(ci, False, True, output, input,
                                            None))
    self.assertEqual(output.getvalue().count('??'), 2)
    self.assertEqual(output.getvalue().count('XX!!XX'), 2)
    self.assertEqual(output.getvalue().count('(y/N)'), 0)

  def testDoDefaultPresubmitChecks(self):
    description_lines = ('Hello there',
                         'this is a change',
                         'STORY=http://tracker/123')
    files = [
      ['A', 'haspresubmit\\blat.cc'],
    ]
    ci = gcl.ChangeInfo(name='mychange',
                        description='\n'.join(description_lines),
                        files=files)

    output = StringIO.StringIO()
    input = StringIO.StringIO('y\n')
    # Always fail.
    DEFAULT_SCRIPT = """
def CheckChangeOnUpload(input_api, output_api):
  print 'This is a test'
  return [output_api.PresubmitError("!!")]
"""
    def MockReadFile(dummy):
      return ''
    gcl.ReadFile = MockReadFile
    def MockIsFile(dummy):
      return False
    os.path.isfile = MockIsFile
    self.failIf(presubmit.DoPresubmitChecks(ci, False, True, output, input,
                                            DEFAULT_SCRIPT))
    self.assertEquals(output.getvalue().count('!!'), 1)

  def testDirectoryHandling(self):
    files = [
      ['A', 'isdir'],
      ['A', 'isdir\\blat.cc'],
    ]
    ci = gcl.ChangeInfo(name='mychange',
                        description='foo',
                        files=files)
    change = presubmit.GclChange(ci)

    affected_files = change.AffectedFiles(include_dirs=False)
    self.failUnless(len(affected_files) == 1)
    self.failUnless(affected_files[0].LocalPath().endswith('blat.cc'))

    affected_files_and_dirs = change.AffectedFiles(include_dirs=True)
    self.failUnless(len(affected_files_and_dirs) == 2)

  def testSvnProperty(self):
    affected_file = presubmit.AffectedFile('foo.cc', 'A')
    self.failUnless(affected_file.SvnProperty('svn:secret-property') ==
                    'secret-property-value')


class InputApiUnittest(PresubmitTestsBase):
  """Tests presubmit.InputApi."""
  def testMembersChanged(self):
    members = [
      'AbsoluteLocalPaths', 'AffectedFiles', 'AffectedTextFiles',
      'DepotToLocalPath', 'FilterTextFiles', 'LocalPaths', 'LocalToDepotPath',
      'PresubmitLocalPath', 'RightHandSideLines', 'ServerPaths',
      'basename', 'cPickle', 'cStringIO', 'canned_checks', 'change',
      'marshal', 'os_path', 'pickle', 'platform',
      're', 'subprocess', 'tempfile', 'urllib2',
    ]
    # If this test fails, you should add the relevant test.
    self.compareMembers(presubmit.InputApi(None, './.'), members)

  def testDepotToLocalPath(self):
    path = presubmit.InputApi.DepotToLocalPath('svn:/foo/smurf')
    self.failUnless(path == 'smurf')
    path = presubmit.InputApi.DepotToLocalPath('svn:/foo/notfound/burp')
    self.failUnless(path == None)

  def testLocalToDepotPath(self):
    path = presubmit.InputApi.LocalToDepotPath('smurf')
    self.failUnless(path == 'svn:/foo/smurf')
    path = presubmit.InputApi.LocalToDepotPath('notfound-food')
    self.failUnless(path == None)

  def testInputApiConstruction(self):
    # Fudge the change object, it's not used during construction anyway
    api = presubmit.InputApi(change=42, presubmit_path='foo/path/PRESUBMIT.py')
    self.failUnless(api.PresubmitLocalPath() == 'foo/path')
    self.failUnless(api.change == 42)

  def testFilterTextFiles(self):
    class MockAffectedFile(object):
      def __init__(self, path, action):
        self.path = path
        self.action = action
      def Action(self):
        return self.action
      def LocalPath(self):
        return self.path
      def AbsoluteLocalPath(self):
        return self.path

    list = [MockAffectedFile('foo/blat.txt', 'M'),
            MockAffectedFile('foo/binary.blob', 'M'),
            MockAffectedFile('blat/flop.txt', 'D')]

    output = presubmit.InputApi.FilterTextFiles(list, include_deletes=True)
    self.failUnless(len(output) == 2)
    self.failUnless(list[0] in output and list[2] in output)

    output = presubmit.InputApi.FilterTextFiles(list, include_deletes=False)
    self.failUnless(len(output) == 1)
    self.failUnless(list[0] in output)

  def testInputApiPresubmitScriptFiltering(self):
    description_lines = ('Hello there',
                         'this is a change',
                         'BUG=123',
                         ' STORY =http://foo/  \t',
                         'and some more regular text')
    files = [
      ['A', os.path.join('foo', 'blat.cc')],
      ['M', os.path.join('foo', 'blat', 'binary.dll')],
      ['D', 'foo/mat/beingdeleted.txt'],
      ['M', 'flop/notfound.txt'],
      ['A', 'boo/flap.h'],
    ]

    ci = gcl.ChangeInfo(name='mychange',
                        description='\n'.join(description_lines),
                        files=files)
    change = presubmit.GclChange(ci)

    api = presubmit.InputApi(change, 'foo/PRESUBMIT.py')

    affected_files = api.AffectedFiles()
    self.assertEquals(len(affected_files), 3)
    self.assertEquals(affected_files[0].LocalPath(),
                      presubmit.normpath('foo/blat.cc'))
    self.assertEquals(affected_files[1].LocalPath(),
                      presubmit.normpath('foo/blat/binary.dll'))
    self.assertEquals(affected_files[2].LocalPath(),
                      presubmit.normpath('foo/mat/beingdeleted.txt'))

    rhs_lines = []
    for line in api.RightHandSideLines():
      rhs_lines.append(line)
    self.failUnless(len(rhs_lines) == 2)
    self.failUnless(rhs_lines[0][0].LocalPath() ==
                    presubmit.normpath('foo/blat.cc'))

  def testGetAbsoluteLocalPath(self):
    # Regression test for bug of presubmit stuff that relies on invoking
    # SVN (e.g. to get mime type of file) not working unless gcl invoked
    # from the client root (e.g. if you were at 'src' and did 'cd base' before
    # invoking 'gcl upload' it would fail because svn wouldn't find the files
    # the presubmit script was asking about).
    files = [
      ['A', 'isdir'],
      ['A', os.path.join('isdir', 'blat.cc')]
    ]
    ci = gcl.ChangeInfo(name='mychange',
                        description='',
                        files=files)
    # It doesn't make sense on non-Windows platform. This is somewhat hacky,
    # but it is needed since we can't just use os.path.join('c:', 'temp').
    change = presubmit.GclChange(ci, 'c:' + os.sep + 'temp')
    affected_files = change.AffectedFiles(include_dirs=True)
    # Local paths should remain the same
    self.failUnless(affected_files[0].LocalPath() ==
                    presubmit.normpath('isdir'))
    self.failUnless(affected_files[1].LocalPath() ==
                    presubmit.normpath('isdir/blat.cc'))
    # Absolute paths should be prefixed
    self.failUnless(affected_files[0].AbsoluteLocalPath() ==
                    presubmit.normpath('c:/temp/isdir'))
    self.failUnless(affected_files[1].AbsoluteLocalPath() ==
                    presubmit.normpath('c:/temp/isdir/blat.cc'))

    # New helper functions need to work
    absolute_paths_from_change = change.AbsoluteLocalPaths(include_dirs=True)
    api = presubmit.InputApi(change=change, presubmit_path='isdir/PRESUBMIT.py')
    absolute_paths_from_api = api.AbsoluteLocalPaths(include_dirs=True)
    for absolute_paths in [absolute_paths_from_change,
                           absolute_paths_from_api]:
      self.failUnless(absolute_paths[0] == presubmit.normpath('c:/temp/isdir'))
      self.failUnless(absolute_paths[1] ==
                      presubmit.normpath('c:/temp/isdir/blat.cc'))


class OuputApiUnittest(PresubmitTestsBase):
  """Tests presubmit.OutputApi."""
  def testMembersChanged(self):
    members = [
      'MailTextResult', 'PresubmitError', 'PresubmitNotifyResult',
      'PresubmitPromptWarning', 'PresubmitResult',
    ]
    # If this test fails, you should add the relevant test.
    self.compareMembers(presubmit.OutputApi(), members)

  def testOutputApiBasics(self):
    self.failUnless(presubmit.OutputApi.PresubmitError('').IsFatal())
    self.failIf(presubmit.OutputApi.PresubmitError('').ShouldPrompt())

    self.failIf(presubmit.OutputApi.PresubmitPromptWarning('').IsFatal())
    self.failUnless(
        presubmit.OutputApi.PresubmitPromptWarning('').ShouldPrompt())

    self.failIf(presubmit.OutputApi.PresubmitNotifyResult('').IsFatal())
    self.failIf(presubmit.OutputApi.PresubmitNotifyResult('').ShouldPrompt())

    # TODO(joi) Test MailTextResult once implemented.

  def testOutputApiHandling(self):
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


class CannedChecksUnittest(PresubmitTestsBase):
  """Tests presubmit_canned_checks.py."""
  class MockInputApi(object):
    class MockUrllib2(object):
      class urlopen(object):
        def __init__(self, url):
          if url == 'url_to_open':
            self.result = '1'
          else:
            self.result = '0'
        def read(self):
          return self.result
        def close(self):
          pass
    def __init__(self, lines=None):
      self.lines = lines
      self.basename = lambda x: x
      self.urllib2 = self.MockUrllib2()
      self.re = presubmit.re

    def RightHandSideLines(self):
      for line in self.lines:
        yield (presubmit.AffectedFile('bingo', 'M'), 1, line)

  def testMembersChanged(self):
    members = [
      'CheckChangeHasBugField', 'CheckChangeHasNoTabs',
      'CheckChangeHasQaField', 'CheckChangeHasTestedField',
      'CheckChangeHasTestField', 'CheckDoNotSubmit',
      'CheckDoNotSubmitInDescription', 'CheckDoNotSubmitInFiles',
      'CheckLongLines', 'CheckTreeIsOpen', 'RunPythonUnitTests',
    ]
    # If this test fails, you should add the relevant test.
    self.compareMembers(presubmit_canned_checks, members)

  def testCannedCheckChangeHasBugField(self):
    change = self.MakeBasicChange('foo',
                                  'Foo\nBUG=1234')
    api = presubmit.InputApi(change, './PRESUBMIT.py')
    self.failIf(presubmit_canned_checks.CheckChangeHasBugField(
        api, presubmit.OutputApi))

    change = self.MakeBasicChange('foo',
                                  'Foo\nNEVERTESTED=did some stuff')
    api = presubmit.InputApi(change, './PRESUBMIT.py')
    self.failUnless(presubmit_canned_checks.CheckChangeHasBugField(
        api, presubmit.OutputApi))

  def testCannedCheckChangeHasTestField(self):
    change = self.MakeBasicChange('foo',
                                  'Foo\nTEST=did some stuff')
    api = presubmit.InputApi(change, './PRESUBMIT.py')
    self.failIf(presubmit_canned_checks.CheckChangeHasTestField(
        api, presubmit.OutputApi))

    change = self.MakeBasicChange('foo',
                                  'Foo\nNOTEST=did some stuff')
    api = presubmit.InputApi(change, './PRESUBMIT.py')
    self.failUnless(presubmit_canned_checks.CheckChangeHasTestField(
        api, presubmit.OutputApi))

  def testCannedCheckChangeHasTestedField(self):
    change = self.MakeBasicChange('foo',
                                  'Foo\nTESTED=did some stuff')
    api = presubmit.InputApi(change, './PRESUBMIT.py')
    self.failIf(presubmit_canned_checks.CheckChangeHasTestedField(
        api, presubmit.OutputApi))

    change = self.MakeBasicChange('foo',
                                  'Foo\nNEVERTESTED=did some stuff')
    api = presubmit.InputApi(change, './PRESUBMIT.py')
    self.failUnless(presubmit_canned_checks.CheckChangeHasTestedField(
        api, presubmit.OutputApi))

  def testCannedCheckChangeHasQAField(self):
    change = self.MakeBasicChange('foo',
                                  'Foo\nQA=test floop feature very well')
    api = presubmit.InputApi(change, './PRESUBMIT.py')
    self.failIf(presubmit_canned_checks.CheckChangeHasQaField(
        api, presubmit.OutputApi))

    change = self.MakeBasicChange('foo',
                                  'Foo\nNOTFORQA=test floop feature very well')
    api = presubmit.InputApi(change, './PRESUBMIT.py')
    self.failUnless(presubmit_canned_checks.CheckChangeHasQaField(
        api, presubmit.OutputApi))

  def testCannedCheckDoNotSubmitInDescription(self):
    change = self.MakeBasicChange('foo', 'hello')
    api = presubmit.InputApi(change, './PRESUBMIT.py')
    self.failIf(presubmit_canned_checks.CheckDoNotSubmitInDescription(
        api, presubmit.OutputApi))

    change = self.MakeBasicChange('foo',
                                  'DO NOT ' + 'SUBMIT')
    api = presubmit.InputApi(change, './PRESUBMIT.py')
    self.failUnless(presubmit_canned_checks.CheckDoNotSubmitInDescription(
        api, presubmit.OutputApi))

  def testCannedCheckDoNotSubmitInFiles(self):
    self.failIf(presubmit_canned_checks.CheckDoNotSubmitInFiles(
      self.MockInputApi(['hello', 'there']), presubmit.OutputApi
    ))
    self.failUnless(presubmit_canned_checks.CheckDoNotSubmitInFiles(
      self.MockInputApi(['hello', 'yo, DO NOT ' + 'SUBMIT']),
                      presubmit.OutputApi))

  def testCannedCheckChangeHasNoTabs(self):
    self.failIf(presubmit_canned_checks.CheckChangeHasNoTabs(
      self.MockInputApi(['hello', 'there']), presubmit.OutputApi
    ))
    self.failUnless(presubmit_canned_checks.CheckChangeHasNoTabs(
      self.MockInputApi(['hello', 'there\tit is']), presubmit.OutputApi
    ))

  def testCannedCheckLongLines(self):
    self.failIf(presubmit_canned_checks.CheckLongLines(
      self.MockInputApi(['hello', 'there']), presubmit.OutputApi, 5
    ))
    self.failUnless(presubmit_canned_checks.CheckLongLines(
      self.MockInputApi(['hello', 'there!']), presubmit.OutputApi, 5
    ))

  def testCannedCheckTreeIsOpen(self):
    self.failIf(presubmit_canned_checks.CheckTreeIsOpen(
      self.MockInputApi(), presubmit.OutputApi, url='url_to_open', closed='0'
    ))
    self.failUnless(presubmit_canned_checks.CheckTreeIsOpen(
      self.MockInputApi(), presubmit.OutputApi, url='url_to_closed', closed='0'
    ))

  def RunPythonUnitTests(self):
    # TODO(maruel): Add real tests.
    self.failIf(presubmit_canned_checks.RunPythonUnitTests(
        self.MockInputApi(),
        presubmit.OutputApi, []))
    self.failUnless(presubmit_canned_checks.RunPythonUnitTests(
        self.MockInputApi(),
        presubmit.OutputApi, ['non_existent_module']))

if __name__ == '__main__':
  unittest.main()
