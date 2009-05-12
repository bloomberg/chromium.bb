#!/usr/bin/python
#
# Copyright 2008-2009 Google Inc.  All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""Unit tests for gclient.py."""

__author__ = 'stephen5.ng@gmail.com (Stephen Ng)'

import __builtin__
import copy
import os
import random
import string
import subprocess
import sys
import unittest

directory, _file = os.path.split(__file__)
if directory:
  directory += os.sep
sys.path.append(os.path.abspath(directory + '../pymox'))

import gclient
import mox


## Some utilities for generating arbitrary arguments.


def String(max_length):
  return ''.join([random.choice(string.letters)
                  for x in xrange(random.randint(1, max_length))])


def Strings(max_arg_count, max_arg_length):
  return [String(max_arg_length) for x in xrange(max_arg_count)]


def Args(max_arg_count=8, max_arg_length=16):
  return Strings(max_arg_count, random.randint(1, max_arg_length))


def _DirElts(max_elt_count=4, max_elt_length=8):
  return os.sep.join(Strings(max_elt_count, max_elt_length))


def Dir(max_elt_count=4, max_elt_length=8):
  return random.choice((os.sep, '')) + _DirElts(max_elt_count, max_elt_length)

def Url(max_elt_count=4, max_elt_length=8):
  return ('svn://random_host:port/a' +
          _DirElts(max_elt_count, max_elt_length).replace(os.sep, '/'))

def RootDir(max_elt_count=4, max_elt_length=8):
  return os.sep + _DirElts(max_elt_count, max_elt_length)


class BaseTestCase(unittest.TestCase):
  # Like unittest's assertRaises, but checks for Gclient.Error.
  def assertRaisesError(self, msg, fn, *args, **kwargs):
    try:
      fn(*args, **kwargs)
    except gclient.Error, e:
      self.assertEquals(e.args[0], msg)
    else:
      self.fail('%s not raised' % msg)

  def Options(self, *args, **kwargs):
    return self.OptionsObject(self, *args, **kwargs)

  def setUp(self):
    self.mox = mox.Mox()
    # Mock them to be sure nothing bad happens.
    self._CaptureSVN = gclient.CaptureSVN
    gclient.CaptureSVN = self.mox.CreateMockAnything()
    self._CaptureSVNInfo = gclient.CaptureSVNInfo
    gclient.CaptureSVNInfo = self.mox.CreateMockAnything()
    self._CaptureSVNStatus = gclient.CaptureSVNStatus
    gclient.CaptureSVNStatus = self.mox.CreateMockAnything()
    self._FileRead = gclient.FileRead
    gclient.FileRead = self.mox.CreateMockAnything()
    self._FileWrite = gclient.FileWrite
    gclient.FileWrite = self.mox.CreateMockAnything()
    self._RemoveDirectory = gclient.RemoveDirectory
    gclient.RemoveDirectory = self.mox.CreateMockAnything()
    self._RunSVN = gclient.RunSVN
    gclient.RunSVN = self.mox.CreateMockAnything()
    self._RunSVNAndGetFileList = gclient.RunSVNAndGetFileList
    gclient.RunSVNAndGetFileList = self.mox.CreateMockAnything()
    # Doesn't seem to work very well:
    self._os = gclient.os
    gclient.os = self.mox.CreateMock(os)
    self._sys = gclient.sys
    gclient.sys = self.mox.CreateMock(sys)
    self._subprocess = gclient.subprocess
    gclient.subprocess = self.mox.CreateMock(subprocess)

  def tearDown(self):
    gclient.CaptureSVN = self._CaptureSVN
    gclient.CaptureSVNInfo = self._CaptureSVNInfo
    gclient.CaptureSVNStatus = self._CaptureSVNStatus
    gclient.FileRead = self._FileRead
    gclient.FileWrite = self._FileWrite
    gclient.RemoveDirectory = self._RemoveDirectory
    gclient.RunSVN = self._RunSVN
    gclient.RunSVNAndGetFileList = self._RunSVNAndGetFileList
    # Doesn't seem to work very well:
    gclient.os = self._os
    gclient.sys = self._sys
    gclient.subprocess = self._subprocess


class GclientTestCase(BaseTestCase):
  class OptionsObject(object):
    def __init__(self, test_case, verbose=False, spec=None,
                 config_filename='a_file_name',
                 entries_filename='a_entry_file_name',
                 deps_file='a_deps_file_name', force=False):
      self.verbose = verbose
      self.spec = spec
      self.config_filename = config_filename
      self.entries_filename = entries_filename
      self.deps_file = deps_file
      self.force = force
      self.revisions = []
      self.manually_grab_svn_rev = True
      self.deps_os = None
      self.head = False

      # Mox
      self.stdout = test_case.stdout
      self.path_exists = test_case.path_exists
      self.platform = test_case.platform
      self.gclient = test_case.gclient
      self.scm_wrapper = test_case.scm_wrapper

  def setUp(self):
    BaseTestCase.setUp(self)
    self.stdout = self.mox.CreateMock(sys.stdout)
    #self.subprocess = self.mox.CreateMock(subprocess)
    # Stub os.path.exists.
    self.path_exists = self.mox.CreateMockAnything()
    self.sys = self.mox.CreateMock(sys)
    self.platform = 'darwin'

    self.gclient = self.mox.CreateMock(gclient.GClient)
    self.scm_wrapper = self.mox.CreateMock(gclient.SCMWrapper)

    self.args = Args()
    self.root_dir = Dir()
    self.url = Url()


class GClientCommandsTestCase(BaseTestCase):
  def testCommands(self):
    known_commands = [gclient.DoCleanup, gclient.DoConfig, gclient.DoDiff,
                      gclient.DoHelp, gclient.DoStatus, gclient.DoUpdate,
                      gclient.DoRevert, gclient.DoRunHooks, gclient.DoRevInfo]
    for (k,v) in gclient.gclient_command_map.iteritems():
      # If it fails, you need to add a test case for the new command.
      self.assert_(v in known_commands)
    self.mox.ReplayAll()
    self.mox.VerifyAll()

class TestDoConfig(GclientTestCase):
  def setUp(self):
    GclientTestCase.setUp(self)
    # pymox has trouble to mock the class object and not a class instance.
    self.gclient = self.mox.CreateMockAnything()

  def testMissingArgument(self):
    exception_msg = "required argument missing; see 'gclient help config'"

    self.mox.ReplayAll()
    self.assertRaisesError(exception_msg, gclient.DoConfig, self.Options(), ())
    self.mox.VerifyAll()

  def testExistingClientFile(self):
    options = self.Options()
    exception_msg = ('%s file already exists in the current directory' %
                        options.config_filename)
    self.path_exists(options.config_filename).AndReturn(True)

    self.mox.ReplayAll()
    self.assertRaisesError(exception_msg, gclient.DoConfig, options, (1,))
    self.mox.VerifyAll()

  def testFromText(self):
    options = self.Options(spec='config_source_content')
    options.path_exists(options.config_filename).AndReturn(False)
    options.gclient('.', options).AndReturn(options.gclient)
    options.gclient.SetConfig(options.spec)
    options.gclient.SaveConfig()

    self.mox.ReplayAll()
    gclient.DoConfig(options, (1,),)
    self.mox.VerifyAll()

  def testCreateClientFile(self):
    options = self.Options()
    options.path_exists(options.config_filename).AndReturn(False)
    options.gclient('.', options).AndReturn(options.gclient)
    options.gclient.SetDefaultConfig('the_name', 'http://svn/url/the_name',
                                     'other')
    options.gclient.SaveConfig()

    self.mox.ReplayAll()
    gclient.DoConfig(options,
                     ('http://svn/url/the_name', 'other', 'args', 'ignored'))
    self.mox.VerifyAll()


class TestDoHelp(GclientTestCase):
  def testGetUsage(self):
    options = self.Options()
    print >> options.stdout, gclient.COMMAND_USAGE_TEXT['config']

    self.mox.ReplayAll()
    gclient.DoHelp(options, ('config',))
    self.mox.VerifyAll()

  def testTooManyArgs(self):
    options = self.Options()
    self.mox.ReplayAll()
    self.assertRaisesError("unknown subcommand 'config'; see 'gclient help'",
                           gclient.DoHelp, options, ('config',
                                                     'another argument'))
    self.mox.VerifyAll()

  def testUnknownSubcommand(self):
    options = self.Options()
    self.mox.ReplayAll()
    self.assertRaisesError("unknown subcommand 'xyzzy'; see 'gclient help'",
                           gclient.DoHelp, options, ('xyzzy',))
    self.mox.VerifyAll()


class GenericCommandTestCase(GclientTestCase):
  def ReturnValue(self, command, function, return_value):
    options = self.Options()
    self.gclient.LoadCurrentConfig(options).AndReturn(self.gclient)
    self.gclient.RunOnDeps(command, self.args).AndReturn(return_value)

    self.mox.ReplayAll()
    result = function(options, self.args)
    self.assertEquals(result, return_value)
    self.mox.VerifyAll()

  def BadClient(self, function):
    options = self.Options()
    self.gclient.LoadCurrentConfig(options).AndReturn(None)

    self.mox.ReplayAll()
    self.assertRaisesError(
        "client not configured; see 'gclient config'",
        function, options, self.args)
    self.mox.VerifyAll()

  def Verbose(self, command, function):
    options = self.Options(verbose=True)
    self.gclient.LoadCurrentConfig(options).AndReturn(self.gclient)
    text = "# Dummy content\nclient = 'my client'"
    self.gclient.ConfigContent().AndReturn(text)
    print >>self.stdout, text
    self.gclient.RunOnDeps(command, self.args).AndReturn(0)

    self.mox.ReplayAll()
    result = function(options, self.args)
    self.assertEquals(result, 0)
    self.mox.VerifyAll()

class TestDoCleanup(GenericCommandTestCase):
  def testGoodClient(self):
    self.ReturnValue('cleanup', gclient.DoCleanup, 0)
  def testError(self):
    self.ReturnValue('cleanup', gclient.DoCleanup, 42)
  def testBadClient(self):
    self.BadClient(gclient.DoCleanup)

class TestDoStatus(GenericCommandTestCase):
  def testGoodClient(self):
    self.ReturnValue('status', gclient.DoStatus, 0)
  def testError(self):
    self.ReturnValue('status', gclient.DoStatus, 42)
  def testBadClient(self):
    self.BadClient(gclient.DoStatus)


class TestDoRunHooks(GenericCommandTestCase):
  def Options(self, verbose=False, *args, **kwargs):
    return self.OptionsObject(self, verbose=verbose, *args, **kwargs)

  def testGoodClient(self):
    self.ReturnValue('runhooks', gclient.DoRunHooks, 0)
  def testError(self):
    self.ReturnValue('runhooks', gclient.DoRunHooks, 42)
  def testBadClient(self):
    self.BadClient(gclient.DoRunHooks)


class TestDoUpdate(GenericCommandTestCase):
  def Options(self, verbose=False, *args, **kwargs):
    return self.OptionsObject(self, verbose=verbose, *args, **kwargs)

  def ReturnValue(self, command, function, return_value):
    options = self.Options()
    self.gclient.LoadCurrentConfig(options).AndReturn(self.gclient)
    self.gclient.GetVar("solutions")
    self.gclient.RunOnDeps(command, self.args).AndReturn(return_value)

    self.mox.ReplayAll()
    result = function(options, self.args)
    self.assertEquals(result, return_value)
    self.mox.VerifyAll()

  def Verbose(self, command, function):
    options = self.Options(verbose=True)
    self.gclient.LoadCurrentConfig(options).AndReturn(self.gclient)
    self.gclient.GetVar("solutions")
    text = "# Dummy content\nclient = 'my client'"
    self.gclient.ConfigContent().AndReturn(text)
    print >>self.stdout, text
    self.gclient.RunOnDeps(command, self.args).AndReturn(0)

    self.mox.ReplayAll()
    result = function(options, self.args)
    self.assertEquals(result, 0)
    self.mox.VerifyAll()

  def Options(self, verbose=False, *args, **kwargs):
    return self.OptionsObject(self, verbose=verbose, *args, **kwargs)

  def testBasic(self):
    self.ReturnValue('update', gclient.DoUpdate, 0)
  def testError(self):
    self.ReturnValue('update', gclient.DoUpdate, 42)
  def testBadClient(self):
    self.BadClient(gclient.DoUpdate)
  def testVerbose(self):
    self.Verbose('update', gclient.DoUpdate)


class TestDoDiff(GenericCommandTestCase):
  def Options(self, *args, **kwargs):
      return self.OptionsObject(self, *args, **kwargs)

  def testBasic(self):
    self.ReturnValue('diff', gclient.DoDiff, 0)
  def testError(self):
    self.ReturnValue('diff', gclient.DoDiff, 42)
  def testBadClient(self):
    self.BadClient(gclient.DoDiff)
  def testVerbose(self):
    self.Verbose('diff', gclient.DoDiff)


class TestDoRevert(GenericCommandTestCase):
  def testBasic(self):
    self.ReturnValue('revert', gclient.DoRevert, 0)
  def testError(self):
    self.ReturnValue('revert', gclient.DoRevert, 42)
  def testBadClient(self):
    self.BadClient(gclient.DoRevert)


class GClientClassTestCase(GclientTestCase):
  def testDir(self):
    members = ['ConfigContent', 'FromImpl', '_VarImpl', '_ParseAllDeps',
      '_ParseSolutionDeps', 'GetVar', '_LoadConfig', 'LoadCurrentConfig',
      '_ReadEntries', '_RunHookAction', '_RunHooks', 'RunOnDeps', 'SaveConfig',
      '_SaveEntries', 'SetConfig', 'SetDefaultConfig', 'supported_commands',
      'PrintRevInfo']

    # If you add a member, be sure to add the relevant test!
    actual_members = [x for x in sorted(dir(gclient.GClient))
                      if not x.startswith('__')]
    self.assertEqual(actual_members, sorted(members))
    self.mox.ReplayAll()
    self.mox.VerifyAll()

  def testSetConfig_ConfigContent_GetVar_SaveConfig_SetDefaultConfig(self):
    options = self.Options()
    text = "# Dummy content\nclient = 'my client'"
    gclient.FileWrite(os.path.join(self.root_dir, options.config_filename),
                      text)

    self.mox.ReplayAll()
    client = gclient.GClient(self.root_dir, options)
    client.SetConfig(text)
    self.assertEqual(client.ConfigContent(), text)
    self.assertEqual(client.GetVar('client'), 'my client')
    self.assertEqual(client.GetVar('foo'), None)
    client.SaveConfig()

    solution_name = 'solution name'
    solution_url = 'solution url'
    safesync_url = 'safesync url'
    default_text = gclient.DEFAULT_CLIENT_FILE_TEXT % (solution_name,
                                                       solution_url,
                                                       safesync_url)
    client.SetDefaultConfig(solution_name, solution_url, safesync_url)
    self.assertEqual(client.ConfigContent(), default_text)
    solutions = [{
      'name': solution_name,
      'url': solution_url,
      'custom_deps': {},
      'safesync_url': safesync_url
    }]
    self.assertEqual(client.GetVar('solutions'), solutions)
    self.assertEqual(client.GetVar('foo'), None)
    self.mox.VerifyAll()

  def testLoadCurrentConfig(self):
    # pymox has trouble to mock the class object and not a class instance.
    self.gclient = self.mox.CreateMockAnything()
    options = self.Options()
    path = os.path.realpath(self.root_dir)
    options.path_exists(os.path.join(path, options.config_filename)
        ).AndReturn(True)
    options.gclient(path, options).AndReturn(options.gclient)
    options.gclient._LoadConfig()

    self.mox.ReplayAll()
    client = gclient.GClient.LoadCurrentConfig(options, self.root_dir)
    self.mox.VerifyAll()

  def testRunOnDepsNoDeps(self):
    solution_name = 'testRunOnDepsNoDeps_solution_name'
    gclient_config = (
      "solutions = [ {\n"
      "  'name': '%s',\n"
      "  'url': '%s',\n"
      "  'custom_deps': {},\n"
      "} ]\n"
    ) % (solution_name, self.url)

    entries_content = (
      'entries = [\n'
      '  "%s",\n'
      ']\n'
    ) % solution_name

    self.scm_wrapper = self.mox.CreateMockAnything()
    scm_wrapper_sol = self.mox.CreateMock(gclient.SCMWrapper)

    options = self.Options()

    # Expect a check for the entries file and we say there is not one.
    options.path_exists(os.path.join(self.root_dir, options.entries_filename)
        ).AndReturn(False)

    # An scm will be requested for the solution.
    options.scm_wrapper(self.url, self.root_dir, solution_name
        ).AndReturn(scm_wrapper_sol)
    # Then an update will be performed.
    scm_wrapper_sol.RunCommand('update', options, self.args, [])
    # Then an attempt will be made to read its DEPS file.
    gclient.FileRead(os.path.join(self.root_dir,
                     solution_name,
                     options.deps_file)).AndRaise(IOError(2, 'No DEPS file'))

    # After everything is done, an attempt is made to write an entries
    # file.
    gclient.FileWrite(os.path.join(self.root_dir, options.entries_filename),
        entries_content)

    self.mox.ReplayAll()
    client = gclient.GClient(self.root_dir, options)
    client.SetConfig(gclient_config)
    client.RunOnDeps('update', self.args)
    self.mox.VerifyAll()

  def testRunOnDepsRelativePaths(self):
    solution_name = 'testRunOnDepsRelativePaths_solution_name'
    gclient_config = (
      "solutions = [ {\n"
      "  'name': '%s',\n"
      "  'url': '%s',\n"
      "  'custom_deps': {},\n"
      "} ]\n"
    ) % (solution_name, self.url)

    deps = (
      "use_relative_paths = True\n"
      "deps = {\n"
      "  'src/t': 'svn://scm.t/trunk',\n"
      "}\n")

    entries_content = (
      'entries = [\n'
      '  "%s",\n'
      '  "%s",\n'
      ']\n'
    ) % (os.path.join(solution_name, 'src', 't'), solution_name)

    self.scm_wrapper = self.mox.CreateMockAnything()
    scm_wrapper_sol = self.mox.CreateMock(gclient.SCMWrapper)
    scm_wrapper_t = self.mox.CreateMock(gclient.SCMWrapper)

    options = self.Options()

    # Expect a check for the entries file and we say there is not one.
    options.path_exists(os.path.join(self.root_dir, options.entries_filename)
        ).AndReturn(False)

    # An scm will be requested for the solution.
    options.scm_wrapper(self.url, self.root_dir, solution_name
        ).AndReturn(scm_wrapper_sol)
    # Then an update will be performed.
    scm_wrapper_sol.RunCommand('update', options, self.args, [])
    # Then an attempt will be made to read its DEPS file.
    gclient.FileRead(os.path.join(self.root_dir,
                     solution_name,
                     options.deps_file)).AndReturn(deps)

    # Next we expect an scm to be request for dep src/t but it should
    # use the url specified in deps and the relative path should now
    # be relative to the DEPS file.
    options.scm_wrapper(
        'svn://scm.t/trunk',
        self.root_dir,
        os.path.join(solution_name, "src", "t")).AndReturn(scm_wrapper_t)
    scm_wrapper_t.RunCommand('update', options, self.args, [])

    # After everything is done, an attempt is made to write an entries
    # file.
    gclient.FileWrite(os.path.join(self.root_dir, options.entries_filename),
        entries_content)

    self.mox.ReplayAll()
    client = gclient.GClient(self.root_dir, options)
    client.SetConfig(gclient_config)
    client.RunOnDeps('update', self.args)
    self.mox.VerifyAll()

  def testRunOnDepsCustomDeps(self):
    solution_name = 'testRunOnDepsCustomDeps_solution_name'
    gclient_config = (
      "solutions = [ {\n"
      "  'name': '%s',\n"
      "  'url': '%s',\n"
      "  'custom_deps': {\n"
      "    'src/b': None,\n"
      "    'src/n': 'svn://custom.n/trunk',\n"
      "    'src/t': 'svn://custom.t/trunk',\n"
      "  }\n} ]\n"
    ) % (solution_name, self.url)

    deps = (
      "deps = {\n"
      "  'src/b': 'svn://original.b/trunk',\n"
      "  'src/t': 'svn://original.t/trunk',\n"
      "}\n"
    )

    entries_content = (
      'entries = [\n'
      '  "%s",\n'
      '  "src/n",\n'
      '  "src/t",\n'
      ']\n'
    ) % solution_name

    self.scm_wrapper = self.mox.CreateMockAnything()
    scm_wrapper_sol = self.mox.CreateMock(gclient.SCMWrapper)
    scm_wrapper_t = self.mox.CreateMock(gclient.SCMWrapper)
    scm_wrapper_n = self.mox.CreateMock(gclient.SCMWrapper)

    options = self.Options()

    # Expect a check for the entries file and we say there is not one.
    options.path_exists(os.path.join(self.root_dir, options.entries_filename)
        ).AndReturn(False)

    # An scm will be requested for the solution.
    options.scm_wrapper(self.url, self.root_dir, solution_name
        ).AndReturn(scm_wrapper_sol)
    # Then an update will be performed.
    scm_wrapper_sol.RunCommand('update', options, self.args, [])
    # Then an attempt will be made to read its DEPS file.
    gclient.FileRead(os.path.join(self.root_dir,
                     solution_name,
                     options.deps_file)).AndReturn(deps)

    # Next we expect an scm to be request for dep src/n even though it does not
    # exist in the DEPS file.
    options.scm_wrapper('svn://custom.n/trunk',
                        self.root_dir,
                        "src/n").AndReturn(scm_wrapper_n)

    # Next we expect an scm to be request for dep src/t but it should
    # use the url specified in custom_deps.
    options.scm_wrapper('svn://custom.t/trunk',
                        self.root_dir,
                        "src/t").AndReturn(scm_wrapper_t)

    scm_wrapper_n.RunCommand('update', options, self.args, [])
    scm_wrapper_t.RunCommand('update', options, self.args, [])

    # NOTE: the dep src/b should not create an scm at all.

    # After everything is done, an attempt is made to write an entries
    # file.
    gclient.FileWrite(os.path.join(self.root_dir, options.entries_filename),
        entries_content)

    self.mox.ReplayAll()
    client = gclient.GClient(self.root_dir, options)
    client.SetConfig(gclient_config)
    client.RunOnDeps('update', self.args)
    self.mox.VerifyAll()

  # Regression test for Issue #11.
  # http://code.google.com/p/gclient/issues/detail?id=11
  def testRunOnDepsSharedDependency(self):
    name_a = 'testRunOnDepsSharedDependency_a'
    name_b = 'testRunOnDepsSharedDependency_b'

    url_a = self.url + '/a'
    url_b = self.url + '/b'

    # config declares two solutions and each has a dependency to place
    # http://svn.t/trunk at src/t.
    gclient_config = (
      "solutions = [ {\n"
      "  'name': '%s',\n"
      "  'url': '%s',\n"
      "  'custom_deps': {},\n"
      "}, {\n"
      "  'name': '%s',\n"
      "  'url': '%s',\n"
      "  'custom_deps': {},\n"
      "}\n]\n") % (name_a, url_a, name_b, url_b)

    deps_b = deps_a = (
      "deps = {\n"
      "  'src/t' : 'http://svn.t/trunk',\n"
    "}\n")

    entries_content = (
      'entries = [\n  "%s",\n'
      '  "%s",\n'
      '  "src/t",\n'
      ']\n') % (name_a, name_b)

    self.scm_wrapper = self.mox.CreateMockAnything()
    scm_wrapper_a = self.mox.CreateMock(gclient.SCMWrapper)
    scm_wrapper_b = self.mox.CreateMock(gclient.SCMWrapper)
    scm_wrapper_dep = self.mox.CreateMock(gclient.SCMWrapper)

    options = self.Options()

    # Expect a check for the entries file and we say there is not one.
    options.path_exists(os.path.join(self.root_dir, options.entries_filename)
        ).AndReturn(False)

    # An scm will be requested for the first solution.
    options.scm_wrapper(url_a, self.root_dir, name_a).AndReturn(
        scm_wrapper_a)
    # Then an attempt will be made to read it's DEPS file.
    gclient.FileRead(os.path.join(self.root_dir, name_a, options.deps_file)
        ).AndReturn(deps_a)
    # Then an update will be performed.
    scm_wrapper_a.RunCommand('update', options, self.args, [])

    # An scm will be requested for the second solution.
    options.scm_wrapper(url_b, self.root_dir, name_b).AndReturn(
        scm_wrapper_b)
    # Then an attempt will be made to read its DEPS file.
    gclient.FileRead(os.path.join(self.root_dir, name_b, options.deps_file)
        ).AndReturn(deps_b)
    # Then an update will be performed.
    scm_wrapper_b.RunCommand('update', options, self.args, [])

    # Finally, an scm is requested for the shared dep.
    options.scm_wrapper('http://svn.t/trunk', self.root_dir, 'src/t'
        ).AndReturn(scm_wrapper_dep)
    # And an update is run on it.
    scm_wrapper_dep.RunCommand('update', options, self.args, [])

    # After everything is done, an attempt is made to write an entries file.
    gclient.FileWrite(os.path.join(self.root_dir, options.entries_filename),
        entries_content)

    self.mox.ReplayAll()
    client = gclient.GClient(self.root_dir, options)
    client.SetConfig(gclient_config)
    client.RunOnDeps('update', self.args)
    self.mox.VerifyAll()

  def testRunOnDepsSuccess(self):
    # Fake .gclient file.
    name = 'testRunOnDepsSuccess_solution_name'
    gclient_config = """solutions = [ {
  'name': '%s',
  'url': '%s',
  'custom_deps': {},
}, ]""" % (name, self.url)

    # pymox has trouble to mock the class object and not a class instance.
    self.scm_wrapper = self.mox.CreateMockAnything()
    options = self.Options()
    options.path_exists(os.path.join(self.root_dir, options.entries_filename)
        ).AndReturn(False)
    options.scm_wrapper(self.url, self.root_dir, name).AndReturn(
        options.scm_wrapper)
    options.scm_wrapper.RunCommand('update', options, self.args, [])
    gclient.FileRead(os.path.join(self.root_dir, name, options.deps_file)
        ).AndReturn("Boo = 'a'")
    gclient.FileWrite(os.path.join(self.root_dir, options.entries_filename),
                      'entries = [\n  "%s",\n]\n' % name)

    self.mox.ReplayAll()
    client = gclient.GClient(self.root_dir, options)
    client.SetConfig(gclient_config)
    client.RunOnDeps('update', self.args)
    self.mox.VerifyAll()

  def testRunOnDepsRevisions(self):
    def OptIsRev(options, rev):
      if not options.revision == str(rev):
        print "options.revision = %s" % options.revision
      return options.revision == str(rev)
    def OptIsRevNone(options):
      if options.revision:
        print "options.revision = %s" % options.revision
      return options.revision == None
    def OptIsRev42(options):
      return OptIsRev(options, 42)
    def OptIsRev123(options):
      return OptIsRev(options, 123)
    def OptIsRev333(options):
      return OptIsRev(options, 333)

    # Fake .gclient file.
    gclient_config = """solutions = [ {
  'name': 'src',
  'url': '%s',
  'custom_deps': {},
}, ]""" % self.url
    # Fake DEPS file.
    deps_content = """deps = {
  'src/breakpad/bar': 'http://google-breakpad.googlecode.com/svn/trunk/src@285',
  'foo/third_party/WebKit': '/trunk/deps/third_party/WebKit',
  'src/third_party/cygwin': '/trunk/deps/third_party/cygwin@3248',
}
deps_os = {
  'win': {
    'src/foosad/asdf': 'svn://random_server:123/asd/python_24@5580',
  },
  'mac': {
    'src/third_party/python_24': 'svn://random_server:123/trunk/python_24@5580',
  },
}"""
    entries_content = (
      'entries = [\n  "src",\n'
      '  "foo/third_party/WebKit",\n'
      '  "src/third_party/cygwin",\n'
      '  "src/third_party/python_24",\n'
      '  "src/breakpad/bar",\n'
      ']\n')
    cygwin_path = 'dummy path cygwin'
    webkit_path = 'dummy path webkit'

    # pymox has trouble to mock the class object and not a class instance.
    self.scm_wrapper = self.mox.CreateMockAnything()
    scm_wrapper_bleh = self.mox.CreateMock(gclient.SCMWrapper)
    scm_wrapper_src = self.mox.CreateMock(gclient.SCMWrapper)
    scm_wrapper_src2 = self.mox.CreateMock(gclient.SCMWrapper)
    scm_wrapper_webkit = self.mox.CreateMock(gclient.SCMWrapper)
    scm_wrapper_breakpad = self.mox.CreateMock(gclient.SCMWrapper)
    scm_wrapper_cygwin = self.mox.CreateMock(gclient.SCMWrapper)
    scm_wrapper_python = self.mox.CreateMock(gclient.SCMWrapper)
    options = self.Options()
    options.revisions = [ 'src@123', 'foo/third_party/WebKit@42',
                          'src/third_party/cygwin@333' ]

    # Also, pymox doesn't verify the order of function calling w.r.t. different
    # mock objects. Pretty lame. So reorder as we wish to make it clearer.
    gclient.FileRead(os.path.join(self.root_dir, 'src', options.deps_file)
        ).AndReturn(deps_content)
    gclient.FileWrite(os.path.join(self.root_dir, options.entries_filename),
                      entries_content)

    options.path_exists(os.path.join(self.root_dir, options.entries_filename)
        ).AndReturn(False)

    options.scm_wrapper(self.url, self.root_dir, 'src').AndReturn(
        scm_wrapper_src)
    scm_wrapper_src.RunCommand('update', mox.Func(OptIsRev123), self.args, [])

    options.scm_wrapper(self.url, self.root_dir,
                        None).AndReturn(scm_wrapper_src2)
    scm_wrapper_src2.FullUrlForRelativeUrl('/trunk/deps/third_party/cygwin@3248'
        ).AndReturn(cygwin_path)

    options.scm_wrapper(self.url, self.root_dir,
                        None).AndReturn(scm_wrapper_src2)
    scm_wrapper_src2.FullUrlForRelativeUrl('/trunk/deps/third_party/WebKit'
        ).AndReturn(webkit_path)

    options.scm_wrapper(webkit_path, self.root_dir,
                        'foo/third_party/WebKit').AndReturn(scm_wrapper_webkit)
    scm_wrapper_webkit.RunCommand('update', mox.Func(OptIsRev42), self.args, [])

    options.scm_wrapper(
        'http://google-breakpad.googlecode.com/svn/trunk/src@285',
        self.root_dir, 'src/breakpad/bar').AndReturn(scm_wrapper_breakpad)
    scm_wrapper_breakpad.RunCommand('update', mox.Func(OptIsRevNone),
                                    self.args, [])

    options.scm_wrapper(cygwin_path, self.root_dir,
                        'src/third_party/cygwin').AndReturn(scm_wrapper_cygwin)
    scm_wrapper_cygwin.RunCommand('update', mox.Func(OptIsRev333), self.args,
                                  [])

    options.scm_wrapper('svn://random_server:123/trunk/python_24@5580',
                        self.root_dir,
                        'src/third_party/python_24').AndReturn(
                            scm_wrapper_python)
    scm_wrapper_python.RunCommand('update', mox.Func(OptIsRevNone), self.args,
                                  [])

    self.mox.ReplayAll()
    client = gclient.GClient(self.root_dir, options)
    client.SetConfig(gclient_config)
    client.RunOnDeps('update', self.args)
    self.mox.VerifyAll()

  def testRunOnDepsConflictingRevisions(self):
    # Fake .gclient file.
    name = 'testRunOnDepsConflictingRevisions_solution_name'
    gclient_config = """solutions = [ {
  'name': '%s',
  'url': '%s',
  'custom_deps': {},
  'custom_vars': {},
}, ]""" % (name, self.url)
    # Fake DEPS file.
    deps_content = """deps = {
  'foo/third_party/WebKit': '/trunk/deps/third_party/WebKit',
}"""

    options = self.Options()
    options.revisions = [ 'foo/third_party/WebKit@42',
                          'foo/third_party/WebKit@43' ]
    client = gclient.GClient(self.root_dir, options)
    client.SetConfig(gclient_config)
    exception = "Conflicting revision numbers specified."
    try:
      client.RunOnDeps('update', self.args)
    except gclient.Error, e:
      self.assertEquals(e.args[0], exception)
    else:
      self.fail('%s not raised' % exception)

  def testRunOnDepsSuccessVars(self):
    # Fake .gclient file.
    name = 'testRunOnDepsSuccessVars_solution_name'
    gclient_config = """solutions = [ {
  'name': '%s',
  'url': '%s',
  'custom_deps': {},
  'custom_vars': {},
}, ]""" % (name, self.url)
    # Fake DEPS file.
    deps_content = """vars = {
  'webkit': '/trunk/bar/',
}
deps = {
  'foo/third_party/WebKit': Var('webkit') + 'WebKit',
}"""
    entries_content = (
      'entries = [\n  "foo/third_party/WebKit",\n'
      '  "%s",\n'
      ']\n') % name
    webkit_path = 'dummy path webkit'

    # pymox has trouble to mock the class object and not a class instance.
    self.scm_wrapper = self.mox.CreateMockAnything()
    scm_wrapper_webkit = self.mox.CreateMock(gclient.SCMWrapper)
    scm_wrapper_src = self.mox.CreateMock(gclient.SCMWrapper)

    options = self.Options()
    gclient.FileRead(os.path.join(self.root_dir, name, options.deps_file)
        ).AndReturn(deps_content)
    gclient.FileWrite(os.path.join(self.root_dir, options.entries_filename),
                      entries_content)

    options.path_exists(os.path.join(self.root_dir, options.entries_filename)
        ).AndReturn(False)
    options.scm_wrapper(self.url, self.root_dir, name).AndReturn(
        options.scm_wrapper)
    options.scm_wrapper.RunCommand('update', options, self.args, [])

    options.scm_wrapper(self.url, self.root_dir,
                        None).AndReturn(scm_wrapper_src)
    scm_wrapper_src.FullUrlForRelativeUrl('/trunk/bar/WebKit'
        ).AndReturn(webkit_path)

    options.scm_wrapper(webkit_path, self.root_dir,
                        'foo/third_party/WebKit').AndReturn(options.scm_wrapper)
    options.scm_wrapper.RunCommand('update', options, self.args, [])

    self.mox.ReplayAll()
    client = gclient.GClient(self.root_dir, options)
    client.SetConfig(gclient_config)
    client.RunOnDeps('update', self.args)
    self.mox.VerifyAll()

  def testRunOnDepsSuccessCustomVars(self):
    # Fake .gclient file.
    name = 'testRunOnDepsSuccessCustomVars_solution_name'
    gclient_config = """solutions = [ {
  'name': '%s',
  'url': '%s',
  'custom_deps': {},
  'custom_vars': {'webkit': '/trunk/bar_custom/'},
}, ]""" % (name, self.url)
    # Fake DEPS file.
    deps_content = """vars = {
  'webkit': '/trunk/bar/',
}
deps = {
  'foo/third_party/WebKit': Var('webkit') + 'WebKit',
}"""
    entries_content = (
      'entries = [\n  "foo/third_party/WebKit",\n'
      '  "%s",\n'
      ']\n') % name
    webkit_path = 'dummy path webkit'

    # pymox has trouble to mock the class object and not a class instance.
    self.scm_wrapper = self.mox.CreateMockAnything()
    scm_wrapper_webkit = self.mox.CreateMock(gclient.SCMWrapper)
    scm_wrapper_src = self.mox.CreateMock(gclient.SCMWrapper)

    options = self.Options()
    gclient.FileRead(os.path.join(self.root_dir, name, options.deps_file)
        ).AndReturn(deps_content)
    gclient.FileWrite(os.path.join(self.root_dir, options.entries_filename),
                      entries_content)

    options.path_exists(os.path.join(self.root_dir, options.entries_filename)
        ).AndReturn(False)
    options.scm_wrapper(self.url, self.root_dir, name).AndReturn(
        options.scm_wrapper)
    options.scm_wrapper.RunCommand('update', options, self.args, [])

    options.scm_wrapper(self.url, self.root_dir,
                        None).AndReturn(scm_wrapper_src)
    scm_wrapper_src.FullUrlForRelativeUrl('/trunk/bar_custom/WebKit'
        ).AndReturn(webkit_path)

    options.scm_wrapper(webkit_path, self.root_dir,
                        'foo/third_party/WebKit').AndReturn(options.scm_wrapper)
    options.scm_wrapper.RunCommand('update', options, self.args, [])

    self.mox.ReplayAll()
    client = gclient.GClient(self.root_dir, options)
    client.SetConfig(gclient_config)
    client.RunOnDeps('update', self.args)
    self.mox.VerifyAll()

  def testRunOnDepsFailueVars(self):
    # Fake .gclient file.
    name = 'testRunOnDepsFailureVars_solution_name'
    gclient_config = """solutions = [ {
  'name': '%s',
  'url': '%s',
  'custom_deps': {},
  'custom_vars': {},
}, ]""" % (name, self.url)
    # Fake DEPS file.
    deps_content = """deps = {
  'foo/third_party/WebKit': Var('webkit') + 'WebKit',
}"""

    # pymox has trouble to mock the class object and not a class instance.
    self.scm_wrapper = self.mox.CreateMockAnything()

    options = self.Options()
    gclient.FileRead(os.path.join(self.root_dir, name, options.deps_file)
        ).AndReturn(deps_content)
    gclient.FileWrite(os.path.join(self.root_dir, options.entries_filename),
                      'dummy entries content')

    options.path_exists(os.path.join(self.root_dir, options.entries_filename)
        ).AndReturn(False)
    options.scm_wrapper(self.url, self.root_dir, name).AndReturn(
        options.scm_wrapper)
    options.scm_wrapper.RunCommand('update', options, self.args, [])

    self.mox.ReplayAll()
    client = gclient.GClient(self.root_dir, options)
    client.SetConfig(gclient_config)
    exception = "Var is not defined: webkit"
    try:
      client.RunOnDeps('update', self.args)
    except gclient.Error, e:
      self.assertEquals(e.args[0], exception)
    else:
      self.fail('%s not raised' % exception)

  def testRunOnDepsFailureInvalidCommand(self):
    options = self.Options()

    self.mox.ReplayAll()
    client = gclient.GClient(self.root_dir, options)
    exception = "'foo' is an unsupported command"
    self.assertRaisesError(exception, gclient.GClient.RunOnDeps, client, 'foo',
                           self.args)
    self.mox.VerifyAll()

  def testRunOnDepsFailureEmpty(self):
    options = self.Options()

    self.mox.ReplayAll()
    client = gclient.GClient(self.root_dir, options)
    exception = "No solution specified"
    self.assertRaisesError(exception, gclient.GClient.RunOnDeps, client,
                           'update', self.args)
    self.mox.VerifyAll()

  def testFromImpl(self):
    # TODO(maruel):  Test me!
    pass

  def test_PrintRevInfo(self):
    # TODO(aharper): no test yet for revinfo, lock it down once we've verified
    # implementation for Pulse plugin
    pass

  # No test for internal functions.
  def test_GetAllDeps(self):
    pass
  def test_GetDefaultSolutionDeps(self):
    pass
  def test_LoadConfig(self):
    pass
  def test_ReadEntries(self):
    pass
  def test_SaveEntries(self):
    pass
  def test_VarImpl(self):
    pass


class SCMWrapperTestCase(BaseTestCase):
  class OptionsObject(object):
     def __init__(self, test_case, verbose=False, revision=None):
      self.verbose = verbose
      self.revision = revision
      self.manually_grab_svn_rev = True
      self.deps_os = None
      self.force = False

      # Mox
      self.stdout = test_case.stdout
      self.path_exists = test_case.path_exists

  def setUp(self):
    BaseTestCase.setUp(self)
    self.root_dir = Dir()
    self.args = Args()
    self.url = Url()
    self.relpath = 'asf'
    self.stdout = self.mox.CreateMock(sys.stdout)
    # Stub os.path.exists.
    self.path_exists = self.mox.CreateMockAnything()

  def testDir(self):
    members = ['FullUrlForRelativeUrl', 'RunCommand',
      'cleanup', 'diff', 'revert', 'status', 'update']

    # If you add a member, be sure to add the relevant test!
    actual_members = [x for x in sorted(dir(gclient.SCMWrapper))
                      if not x.startswith('__')]
    self.assertEqual(actual_members, sorted(members))
    self.mox.ReplayAll()
    self.mox.VerifyAll()

  def testFullUrlForRelativeUrl(self):
    self.url = 'svn://a/b/c/d'

    self.mox.ReplayAll()
    scm = gclient.SCMWrapper(url=self.url, root_dir=self.root_dir,
                             relpath=self.relpath)
    self.assertEqual(scm.FullUrlForRelativeUrl('/crap'), 'svn://a/b/crap')
    self.mox.VerifyAll()

  def testRunCommandException(self):
    options = self.Options(verbose=False)
    options.path_exists(os.path.join(self.root_dir, self.relpath, '.git')
        ).AndReturn(False)

    self.mox.ReplayAll()
    scm = gclient.SCMWrapper(url=self.url, root_dir=self.root_dir,
                             relpath=self.relpath)
    exception = "Unsupported argument(s): %s" % ','.join(self.args)
    self.assertRaisesError(exception, gclient.SCMWrapper.RunCommand,
                           scm, 'update', options, self.args)
    self.mox.VerifyAll()

  def testRunCommandUnknown(self):
    # TODO(maruel): if ever used.
    pass

  def testRevertMissing(self):
    options = self.Options(verbose=True)
    gclient.os.path.isdir = self.mox.CreateMockAnything()
    base_path = os.path.join(self.root_dir, self.relpath)
    gclient.os.path.isdir(base_path).AndReturn(False)
    # It'll to a checkout instead.
    options.path_exists(os.path.join(base_path, '.git')).AndReturn(False)
    print >>options.stdout, ("\n_____ %s is missing, synching instead" %
                             self.relpath)
    # Checkout.
    options.path_exists(base_path).AndReturn(False)
    files_list = self.mox.CreateMockAnything()
    gclient.RunSVNAndGetFileList(options, ['checkout', self.url, base_path],
                                 self.root_dir, files_list)

    self.mox.ReplayAll()
    scm = gclient.SCMWrapper(url=self.url, root_dir=self.root_dir,
                             relpath=self.relpath)
    scm.revert(options, self.args, files_list)
    self.mox.VerifyAll()
    gclient.os.path.isdir = os.path.isdir

  def testRevertNone(self):
    options = self.Options(verbose=True)
    base_path = os.path.join(self.root_dir, self.relpath)
    gclient.os.path.isdir = self.mox.CreateMockAnything()
    gclient.os.path.isdir(base_path).AndReturn(True)
    gclient.CaptureSVNStatus(base_path).AndReturn([])

    self.mox.ReplayAll()
    scm = gclient.SCMWrapper(url=self.url, root_dir=self.root_dir,
                             relpath=self.relpath)
    file_list = []
    scm.revert(options, self.args, file_list)
    self.mox.VerifyAll()
    gclient.os.path.isdir = os.path.isdir

  def testRevert2Files(self):
    options = self.Options(verbose=True)
    base_path = os.path.join(self.root_dir, self.relpath)
    gclient.os.path.isdir = self.mox.CreateMockAnything()
    gclient.os.path.isdir(base_path).AndReturn(True)
    items = [
      gclient.FileStatus('a', 'M', ' ', ' ', ' '), 
      gclient.FileStatus('b', 'A', ' ', ' ', ' '),
    ]
    gclient.CaptureSVNStatus(base_path).AndReturn(items)

    print >>options.stdout, os.path.join(base_path, 'a')
    print >>options.stdout, os.path.join(base_path, 'b')
    gclient.RunSVN(['revert', 'a', 'b'], base_path)

    self.mox.ReplayAll()
    scm = gclient.SCMWrapper(url=self.url, root_dir=self.root_dir,
                             relpath=self.relpath)
    file_list = []
    scm.revert(options, self.args, file_list)
    self.mox.VerifyAll()
    gclient.os.path.isdir = os.path.isdir

  def testStatus(self):
    options = self.Options(verbose=True)
    base_path = os.path.join(self.root_dir, self.relpath)
    gclient.os.path.isdir = self.mox.CreateMockAnything()
    gclient.os.path.isdir(base_path).AndReturn(True)
    gclient.RunSVNAndGetFileList(options, ['status'] + self.args, base_path,
                                 []).AndReturn(None)

    self.mox.ReplayAll()
    scm = gclient.SCMWrapper(url=self.url, root_dir=self.root_dir,
                             relpath=self.relpath)
    file_list = []
    self.assertEqual(scm.status(options, self.args, file_list), None)
    self.mox.VerifyAll()


  # TODO(maruel):  TEST REVISIONS!!!
  # TODO(maruel):  TEST RELOCATE!!!
  def testUpdateCheckout(self):
    options = self.Options(verbose=True)
    base_path = os.path.join(self.root_dir, self.relpath)
    file_info = gclient.PrintableObject()
    file_info.root = 'blah'
    file_info.url = self.url
    file_info.uuid = 'ABC'
    file_info.revision = 42
    options.path_exists(os.path.join(base_path, '.git')).AndReturn(False)
    # Checkout.
    options.path_exists(base_path).AndReturn(False)
    files_list = self.mox.CreateMockAnything()
    gclient.RunSVNAndGetFileList(options, ['checkout', self.url, base_path],
                                 self.root_dir, files_list)
    self.mox.ReplayAll()
    scm = gclient.SCMWrapper(url=self.url, root_dir=self.root_dir,
                             relpath=self.relpath)
    scm.update(options, (), files_list)
    self.mox.VerifyAll()

  def testUpdateUpdate(self):
    options = self.Options(verbose=True)
    base_path = os.path.join(self.root_dir, self.relpath)
    options.force = True
    file_info = {
      'Repository Root': 'blah',
      'URL': self.url,
      'UUID': 'ABC',
      'Revision': 42,
    }
    options.path_exists(os.path.join(base_path, '.git')).AndReturn(False)
    # Checkout or update.
    options.path_exists(base_path).AndReturn(True)
    gclient.CaptureSVNInfo(os.path.join(base_path, "."), '.'
        ).AndReturn(file_info)
    # Cheat a bit here.
    gclient.CaptureSVNInfo(file_info['URL'], '.').AndReturn(file_info)
    additional_args = []
    if options.manually_grab_svn_rev:
      additional_args = ['--revision', str(file_info['Revision'])]
    files_list = []
    gclient.RunSVNAndGetFileList(options, ['update', base_path] + additional_args,
                                 self.root_dir, files_list)

    self.mox.ReplayAll()
    scm = gclient.SCMWrapper(url=self.url, root_dir=self.root_dir,
                             relpath=self.relpath)
    scm.update(options, (), files_list)
    self.mox.VerifyAll()

  def testUpdateGit(self):
    options = self.Options(verbose=True)
    options.path_exists(os.path.join(self.root_dir, self.relpath, '.git')
        ).AndReturn(True)
    print >> options.stdout, (
        "________ found .git directory; skipping %s" % self.relpath)

    self.mox.ReplayAll()
    scm = gclient.SCMWrapper(url=self.url, root_dir=self.root_dir,
                             relpath=self.relpath)
    file_list = []
    scm.update(options, self.args, file_list)
    self.mox.VerifyAll()

  def testGetSVNFileInfo(self):
    xml_text = r"""<?xml version="1.0"?>
<info>
<entry kind="file" path="%s" revision="14628">
<url>http://src.chromium.org/svn/trunk/src/chrome/app/d</url>
<repository><root>http://src.chromium.org/svn</root></repository>
<wc-info>
<schedule>add</schedule>
<depth>infinity</depth>
<copy-from-url>http://src.chromium.org/svn/trunk/src/chrome/app/DEPS</copy-from-url>
<copy-from-rev>14628</copy-from-rev>
<checksum>369f59057ba0e6d9017e28f8bdfb1f43</checksum>
</wc-info>
</entry>
</info>
""" % self.url
    gclient.CaptureSVN(['info', '--xml', self.url],
                       '.', True).AndReturn(xml_text)
    expected = {
      'URL': 'http://src.chromium.org/svn/trunk/src/chrome/app/d',
      'UUID': None,
      'Repository Root': 'http://src.chromium.org/svn',
      'Schedule': 'add',
      'Copied From URL': 'http://src.chromium.org/svn/trunk/src/chrome/app/DEPS',
      'Copied From Rev': '14628',
      'Path': self.url,
      'Revision': 14628,
      'Node Kind': 'file',
    }
    self.mox.ReplayAll()
    file_info = self._CaptureSVNInfo(self.url, '.', True)
    self.assertEquals(sorted(file_info.items()), sorted(expected.items()))
    self.mox.VerifyAll()

  def testCaptureSvnInfo(self):
    xml_text = """<?xml version="1.0"?>
<info>
<entry
   kind="dir"
   path="."
   revision="35">
<url>%s</url>
<repository>
<root>%s</root>
<uuid>7b9385f5-0452-0410-af26-ad4892b7a1fb</uuid>
</repository>
<wc-info>
<schedule>normal</schedule>
<depth>infinity</depth>
</wc-info>
<commit
   revision="35">
<author>maruel</author>
<date>2008-12-04T20:12:19.685120Z</date>
</commit>
</entry>
</info>
""" % (self.url, self.root_dir)
    gclient.CaptureSVN(['info', '--xml', self.url],
                       '.', True).AndReturn(xml_text)
    self.mox.ReplayAll()
    file_info = self._CaptureSVNInfo(self.url, '.', True)
    expected = {
      'URL': self.url,
      'UUID': '7b9385f5-0452-0410-af26-ad4892b7a1fb',
      'Revision': 35,
      'Repository Root': self.root_dir,
      'Schedule': 'normal',
      'Copied From URL': None,
      'Copied From Rev': None,
      'Path': '.',
      'Node Kind': 'dir',
    }
    self.assertEqual(file_info, expected)
    self.mox.VerifyAll()


if __name__ == '__main__':
  unittest.main()

# vim: ts=2:sw=2:tw=80:et:
