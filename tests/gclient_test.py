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
import os
import re
import StringIO
import unittest

import gclient
import gclient_scm
import gclient_utils
import super_mox
from super_mox import mox


class IsOneOf(mox.Comparator):
  def __init__(self, keys):
    self._keys = keys

  def equals(self, rhs):
    return rhs in self._keys

  def __repr__(self):
    return '<sequence or map containing \'%s\'>' % str(self._keys)


class BaseTestCase(super_mox.SuperMoxTestBase):
  def setUp(self):
    super_mox.SuperMoxTestBase.setUp(self)

  # Like unittest's assertRaises, but checks for Gclient.Error.
  def assertRaisesError(self, msg, fn, *args, **kwargs):
    try:
      fn(*args, **kwargs)
    except gclient.Error, e:
      self.assertEquals(e.args[0], msg)
    else:
      self.fail('%s not raised' % msg)


class GClientBaseTestCase(BaseTestCase):
  def Options(self, *args, **kwargs):
    return self.OptionsObject(self, *args, **kwargs)

  def setUp(self):
    BaseTestCase.setUp(self)
    self.mox.StubOutWithMock(gclient.os.path, 'exists')
    self.mox.StubOutWithMock(gclient.os.path, 'isfile')
    self.mox.StubOutWithMock(gclient.os.path, 'isdir')
    self.mox.StubOutWithMock(gclient.os, 'remove')
    self.mox.StubOutWithMock(gclient.sys, 'stdout')
    self.mox.StubOutWithMock(gclient_utils, 'subprocess')
    # These are not tested.
    self.mox.StubOutWithMock(gclient, 'FileRead')
    self.mox.StubOutWithMock(gclient, 'FileWrite')
    self.mox.StubOutWithMock(gclient_utils, 'SubprocessCall')
    self.mox.StubOutWithMock(gclient_utils, 'RemoveDirectory')
    # Mock them to be sure nothing bad happens.
    self.mox.StubOutWithMock(gclient_scm, 'CaptureSVN')
    self._CaptureSVNInfo = gclient_scm.CaptureSVNInfo
    self.mox.StubOutWithMock(gclient_scm, 'CaptureSVNInfo')
    self.mox.StubOutWithMock(gclient_scm, 'CaptureSVNStatus')
    self.mox.StubOutWithMock(gclient_scm, 'RunSVN')
    self.mox.StubOutWithMock(gclient_scm, 'RunSVNAndGetFileList')
    self._gclient_gclient = gclient.GClient
    gclient.GClient = self.mox.CreateMockAnything()
    self._scm_wrapper = gclient_scm.CreateSCM
    gclient_scm.CreateSCM = self.mox.CreateMockAnything()

  def tearDown(self):
    gclient.GClient = self._gclient_gclient
    gclient_scm.CreateSCM = self._scm_wrapper
    BaseTestCase.tearDown(self)


class GclientTestCase(GClientBaseTestCase):
  class OptionsObject(object):
    def __init__(self, test_case, verbose=False, spec=None,
                 config_filename='a_file_name',
                 entries_filename='a_entry_file_name',
                 deps_file='a_deps_file_name', force=False, nohooks=False):
      self.verbose = verbose
      self.spec = spec
      self.config_filename = config_filename
      self.entries_filename = entries_filename
      self.deps_file = deps_file
      self.force = force
      self.nohooks = nohooks
      self.revisions = []
      self.manually_grab_svn_rev = True
      self.deps_os = None
      self.head = False

      # Mox
      self.platform = test_case.platform

  def setUp(self):
    GClientBaseTestCase.setUp(self)
    self.platform = 'darwin'

    self.args = self.Args()
    self.root_dir = self.Dir()
    self.url = self.Url()


class GClientCommandsTestCase(GClientBaseTestCase):
  def testCommands(self):
    known_commands = [gclient.DoCleanup, gclient.DoConfig, gclient.DoDiff,
                      gclient.DoExport, gclient.DoHelp, gclient.DoStatus,
		      gclient.DoUpdate, gclient.DoRevert, gclient.DoRunHooks,
		      gclient.DoRevInfo, gclient.DoPack]
    for (k,v) in gclient.gclient_command_map.iteritems():
      # If it fails, you need to add a test case for the new command.
      self.assert_(v in known_commands)
    self.mox.ReplayAll()

class TestDoConfig(GclientTestCase):
  def testMissingArgument(self):
    exception_msg = "required argument missing; see 'gclient help config'"

    self.mox.ReplayAll()
    self.assertRaisesError(exception_msg, gclient.DoConfig, self.Options(), ())

  def testExistingClientFile(self):
    options = self.Options()
    exception_msg = ('%s file already exists in the current directory' %
                        options.config_filename)
    gclient.os.path.exists(options.config_filename).AndReturn(True)

    self.mox.ReplayAll()
    self.assertRaisesError(exception_msg, gclient.DoConfig, options, (1,))

  def testFromText(self):
    options = self.Options(spec='config_source_content')
    gclient.os.path.exists(options.config_filename).AndReturn(False)
    gclient.GClient('.', options).AndReturn(gclient.GClient)
    gclient.GClient.SetConfig(options.spec)
    gclient.GClient.SaveConfig()

    self.mox.ReplayAll()
    gclient.DoConfig(options, (1,),)

  def testCreateClientFile(self):
    options = self.Options()
    gclient.os.path.exists(options.config_filename).AndReturn(False)
    gclient.GClient('.', options).AndReturn(gclient.GClient)
    gclient.GClient.SetDefaultConfig('the_name', 'http://svn/url/the_name',
                                     'other')
    gclient.GClient.SaveConfig()

    self.mox.ReplayAll()
    gclient.DoConfig(options,
                     ('http://svn/url/the_name', 'other', 'args', 'ignored'))


class TestDoHelp(GclientTestCase):
  def testGetUsage(self):
    print(gclient.COMMAND_USAGE_TEXT['config'])
    self.mox.ReplayAll()
    options = self.Options()
    gclient.DoHelp(options, ('config',))

  def testTooManyArgs(self):
    self.mox.ReplayAll()
    options = self.Options()
    self.assertRaisesError("unknown subcommand 'config'; see 'gclient help'",
                           gclient.DoHelp, options, ('config',
                                                     'another argument'))

  def testUnknownSubcommand(self):
    self.mox.ReplayAll()
    options = self.Options()
    self.assertRaisesError("unknown subcommand 'xyzzy'; see 'gclient help'",
                           gclient.DoHelp, options, ('xyzzy',))


class GenericCommandTestCase(GclientTestCase):
  def ReturnValue(self, command, function, return_value):
    options = self.Options()
    gclient.GClient.LoadCurrentConfig(options).AndReturn(gclient.GClient)
    gclient.GClient.RunOnDeps(command, self.args).AndReturn(return_value)

    self.mox.ReplayAll()
    result = function(options, self.args)
    self.assertEquals(result, return_value)

  def BadClient(self, function):
    options = self.Options()
    gclient.GClient.LoadCurrentConfig(options).AndReturn(None)

    self.mox.ReplayAll()
    self.assertRaisesError(
        "client not configured; see 'gclient config'",
        function, options, self.args)

  def Verbose(self, command, function):
    options = self.Options(verbose=True)
    gclient.GClient.LoadCurrentConfig(options).AndReturn(gclient.GClient)
    text = "# Dummy content\nclient = 'my client'"
    gclient.GClient.ConfigContent().AndReturn(text)
    print(text)
    gclient.GClient.RunOnDeps(command, self.args).AndReturn(0)

    self.mox.ReplayAll()
    result = function(options, self.args)
    self.assertEquals(result, 0)

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
    gclient.GClient.LoadCurrentConfig(options).AndReturn(gclient.GClient)
    gclient.GClient.GetVar("solutions")
    gclient.GClient.RunOnDeps(command, self.args).AndReturn(return_value)

    self.mox.ReplayAll()
    result = function(options, self.args)
    self.assertEquals(result, return_value)

  def Verbose(self, command, function):
    options = self.Options(verbose=True)
    gclient.GClient.LoadCurrentConfig(options).AndReturn(gclient.GClient)
    gclient.GClient.GetVar("solutions")
    text = "# Dummy content\nclient = 'my client'"
    gclient.GClient.ConfigContent().AndReturn(text)
    print(text)
    gclient.GClient.RunOnDeps(command, self.args).AndReturn(0)

    self.mox.ReplayAll()
    result = function(options, self.args)
    self.assertEquals(result, 0)

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


class TestDoExport(GenericCommandTestCase):
  def testBasic(self):
    self.args = ['dir']
    self.ReturnValue('export', gclient.DoExport, 0)
  def testError(self):
    self.args = ['dir']
    self.ReturnValue('export', gclient.DoExport, 42)
  def testBadClient(self):
    self.args = ['dir']
    self.BadClient(gclient.DoExport)


class TestDoPack(GenericCommandTestCase):
  def Options(self, *args, **kwargs):
      return self.OptionsObject(self, *args, **kwargs)

  def testBasic(self):
    self.ReturnValue('pack', gclient.DoPack, 0)
  def testError(self):
    self.ReturnValue('pack', gclient.DoPack, 42)
  def testBadClient(self):
    self.BadClient(gclient.DoPack)


class TestDoRevert(GenericCommandTestCase):
  def testBasic(self):
    self.ReturnValue('revert', gclient.DoRevert, 0)
  def testError(self):
    self.ReturnValue('revert', gclient.DoRevert, 42)
  def testBadClient(self):
    self.BadClient(gclient.DoRevert)


class GClientClassTestCase(GclientTestCase):
  def testDir(self):
    members = [
      'ConfigContent', 'FromImpl', 'GetVar', 'LoadCurrentConfig',
      'RunOnDeps', 'SaveConfig', 'SetConfig', 'SetDefaultConfig',
      'supported_commands', 'PrintRevInfo',
    ]

    # If you add a member, be sure to add the relevant test!
    self.compareMembers(self._gclient_gclient('root_dir', 'options'), members)

  def testSetConfig_ConfigContent_GetVar_SaveConfig_SetDefaultConfig(self):
    options = self.Options()
    text = "# Dummy content\nclient = 'my client'"
    gclient.FileWrite(os.path.join(self.root_dir, options.config_filename),
                      text)

    self.mox.ReplayAll()
    client = self._gclient_gclient(self.root_dir, options)
    client.SetConfig(text)
    self.assertEqual(client.ConfigContent(), text)
    self.assertEqual(client.GetVar('client'), 'my client')
    self.assertEqual(client.GetVar('foo'), None)
    client.SaveConfig()

    solution_name = 'solution name'
    solution_url = 'solution url'
    safesync_url = 'safesync url'
    default_text = gclient.DEFAULT_CLIENT_FILE_TEXT % {
      'solution_name' : solution_name,
      'solution_url'  : solution_url,
      'safesync_url' : safesync_url
    }
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

  def testLoadCurrentConfig(self):
    options = self.Options()
    path = os.path.realpath(self.root_dir)
    gclient.os.path.exists(os.path.join(path, options.config_filename)
        ).AndReturn(True)
    gclient.GClient(path, options).AndReturn(gclient.GClient)
    gclient.GClient._LoadConfig()

    self.mox.ReplayAll()
    client = self._gclient_gclient.LoadCurrentConfig(options, self.root_dir)

  def testRunOnDepsNoDeps(self):
    solution_name = 'testRunOnDepsNoDeps_solution_name'
    gclient_config = (
      "solutions = [ {\n"
      "  'name': '%s',\n"
      "  'url': '%s',\n"
      "  'custom_deps': {},\n"
      "} ]\n"
    ) % (solution_name, self.url)

    # pprint.pformat() is non-deterministic in this case!!
    entries_content1 = (
      "entries = \\\n"
      "{ '%s': '%s'}\n"
    ) % (solution_name, self.url)
    entries_content2 = (
      "entries = \\\n"
      "{'%s': '%s'}\n"
    ) % (solution_name, self.url)

    options = self.Options()

    checkout_path = os.path.join(self.root_dir, solution_name)
    gclient.os.path.exists(os.path.join(checkout_path, '.git')).AndReturn(False)
    # Expect a check for the entries file and we say there is not one.
    gclient.os.path.exists(os.path.join(self.root_dir, options.entries_filename)
        ).AndReturn(False)

    # An scm will be requested for the solution.
    scm_wrapper_sol = self.mox.CreateMockAnything()
    gclient_scm.CreateSCM(self.url, self.root_dir, solution_name
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
        IsOneOf((entries_content1, entries_content2)))

    self.mox.ReplayAll()
    client = self._gclient_gclient(self.root_dir, options)
    client.SetConfig(gclient_config)
    client.RunOnDeps('update', self.args)

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
    entry_path = os.path.join(solution_name, 'src', 't').replace('\\', '\\\\')
    entries_content = (
      "entries = \\\n"
      "{ '%s': '%s',\n"
      "  '%s': 'svn://scm.t/trunk'}\n"
    ) % (solution_name, self.url, entry_path)

    scm_wrapper_sol = self.mox.CreateMockAnything()
    scm_wrapper_t = self.mox.CreateMockAnything()

    options = self.Options()

    gclient.os.path.exists(os.path.join(self.root_dir, solution_name, 'src',
                                        't', '.git')
        ).AndReturn(False)
    gclient.os.path.exists(os.path.join(self.root_dir, solution_name, '.git')
        ).AndReturn(False)
    # Expect a check for the entries file and we say there is not one.
    gclient.os.path.exists(os.path.join(self.root_dir, options.entries_filename)
        ).AndReturn(False)

    # An scm will be requested for the solution.
    gclient_scm.CreateSCM(self.url, self.root_dir, solution_name
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
    gclient_scm.CreateSCM(
        'svn://scm.t/trunk',
        self.root_dir,
        os.path.join(solution_name, "src", "t")).AndReturn(scm_wrapper_t)
    scm_wrapper_t.RunCommand('update', options, self.args, [])

    # After everything is done, an attempt is made to write an entries
    # file.
    gclient.FileWrite(os.path.join(self.root_dir, options.entries_filename),
        entries_content)

    self.mox.ReplayAll()
    client = self._gclient_gclient(self.root_dir, options)
    client.SetConfig(gclient_config)
    client.RunOnDeps('update', self.args)

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
      "entries = \\\n"
      "{ 'src/n': 'svn://custom.n/trunk',\n"
      "  'src/t': 'svn://custom.t/trunk',\n"
      "  '%s': '%s'}\n"
    ) % (solution_name, self.url)

    scm_wrapper_sol = self.mox.CreateMockAnything()
    scm_wrapper_t = self.mox.CreateMockAnything()
    scm_wrapper_n = self.mox.CreateMockAnything()

    options = self.Options()

    checkout_path = os.path.join(self.root_dir, solution_name)
    gclient.os.path.exists(os.path.join(checkout_path, '.git')).AndReturn(False)
    gclient.os.path.exists(os.path.join(self.root_dir, 'src/n', '.git')
        ).AndReturn(False)
    gclient.os.path.exists(os.path.join(self.root_dir, 'src/t', '.git')
        ).AndReturn(False)

    # Expect a check for the entries file and we say there is not one.
    gclient.os.path.exists(os.path.join(self.root_dir, options.entries_filename)
        ).AndReturn(False)

    # An scm will be requested for the solution.
    gclient_scm.CreateSCM(self.url, self.root_dir, solution_name
        ).AndReturn(scm_wrapper_sol)
    # Then an update will be performed.
    scm_wrapper_sol.RunCommand('update', options, self.args, [])
    # Then an attempt will be made to read its DEPS file.
    gclient.FileRead(os.path.join(checkout_path, options.deps_file)
        ).AndReturn(deps)

    # Next we expect an scm to be request for dep src/n even though it does not
    # exist in the DEPS file.
    gclient_scm.CreateSCM('svn://custom.n/trunk',
                           self.root_dir,
                           "src/n").AndReturn(scm_wrapper_n)

    # Next we expect an scm to be request for dep src/t but it should
    # use the url specified in custom_deps.
    gclient_scm.CreateSCM('svn://custom.t/trunk',
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
    client = self._gclient_gclient(self.root_dir, options)
    client.SetConfig(gclient_config)
    client.RunOnDeps('update', self.args)

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
      "entries = \\\n"
      "{ 'src/t': 'http://svn.t/trunk',\n"
      "  '%s': '%s',\n"
      "  '%s': '%s'}\n"
    ) % (name_a, url_a, name_b, url_b)

    scm_wrapper_a = self.mox.CreateMockAnything()
    scm_wrapper_b = self.mox.CreateMockAnything()
    scm_wrapper_dep = self.mox.CreateMockAnything()

    options = self.Options()

    gclient.os.path.exists(os.path.join(self.root_dir, name_a, '.git')
        ).AndReturn(False)
    gclient.os.path.exists(os.path.join(self.root_dir, name_b, '.git')
        ).AndReturn(False)
    gclient.os.path.exists(os.path.join(self.root_dir, 'src/t', '.git')
        ).AndReturn(False)

    # Expect a check for the entries file and we say there is not one.
    gclient.os.path.exists(os.path.join(self.root_dir, options.entries_filename)
        ).AndReturn(False)

    # An scm will be requested for the first solution.
    gclient_scm.CreateSCM(url_a, self.root_dir, name_a).AndReturn(
        scm_wrapper_a)
    # Then an attempt will be made to read it's DEPS file.
    gclient.FileRead(os.path.join(self.root_dir, name_a, options.deps_file)
        ).AndReturn(deps_a)
    # Then an update will be performed.
    scm_wrapper_a.RunCommand('update', options, self.args, [])

    # An scm will be requested for the second solution.
    gclient_scm.CreateSCM(url_b, self.root_dir, name_b).AndReturn(
        scm_wrapper_b)
    # Then an attempt will be made to read its DEPS file.
    gclient.FileRead(os.path.join(self.root_dir, name_b, options.deps_file)
        ).AndReturn(deps_b)
    # Then an update will be performed.
    scm_wrapper_b.RunCommand('update', options, self.args, [])

    # Finally, an scm is requested for the shared dep.
    gclient_scm.CreateSCM('http://svn.t/trunk', self.root_dir, 'src/t'
        ).AndReturn(scm_wrapper_dep)
    # And an update is run on it.
    scm_wrapper_dep.RunCommand('update', options, self.args, [])

    # After everything is done, an attempt is made to write an entries file.
    gclient.FileWrite(os.path.join(self.root_dir, options.entries_filename),
        entries_content)

    self.mox.ReplayAll()
    client = self._gclient_gclient(self.root_dir, options)
    client.SetConfig(gclient_config)
    client.RunOnDeps('update', self.args)

  def testRunOnDepsSuccess(self):
    # Fake .gclient file.
    name = 'testRunOnDepsSuccess_solution_name'
    gclient_config = """solutions = [ {
  'name': '%s',
  'url': '%s',
  'custom_deps': {},
}, ]""" % (name, self.url)

    entries_content = (
      "entries = \\\n"
      "{ '%s': '%s'}\n"
    ) % (name, self.url)

    options = self.Options()
    gclient.os.path.exists(os.path.join(self.root_dir, name, '.git')
        ).AndReturn(False)
    gclient.os.path.exists(os.path.join(self.root_dir, options.entries_filename)
        ).AndReturn(False)
    gclient_scm.CreateSCM(self.url, self.root_dir, name).AndReturn(
        gclient_scm.CreateSCM)
    gclient_scm.CreateSCM.RunCommand('update', options, self.args, [])
    gclient.FileRead(os.path.join(self.root_dir, name, options.deps_file)
        ).AndReturn("Boo = 'a'")
    gclient.FileWrite(os.path.join(self.root_dir, options.entries_filename),
                      entries_content)

    self.mox.ReplayAll()
    client = self._gclient_gclient(self.root_dir, options)
    client.SetConfig(gclient_config)
    client.RunOnDeps('update', self.args)

  def testRunOnDepsRevisions(self):
    def OptIsRev(options, rev):
      if not options.revision == str(rev):
        print("options.revision = %s" % options.revision)
      return options.revision == str(rev)
    def OptIsRevNone(options):
      if options.revision:
        print("options.revision = %s" % options.revision)
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

    cygwin_path = 'dummy path cygwin'
    webkit_path = 'dummy path webkit'

    entries_content = (
      "entries = \\\n"
      "{ 'foo/third_party/WebKit': '%s',\n"
      "  'src': '%s',\n"
      "  'src/breakpad/bar':"
      " 'http://google-breakpad.googlecode.com/svn/trunk/src@285',\n"
      "  'src/third_party/cygwin': '%s',\n"
      "  'src/third_party/python_24':"
      " 'svn://random_server:123/trunk/python_24@5580'}\n"
    ) % (webkit_path, self.url, cygwin_path)

    scm_wrapper_bleh = self.mox.CreateMockAnything()
    scm_wrapper_src = self.mox.CreateMockAnything()
    scm_wrapper_src2 = self.mox.CreateMockAnything()
    scm_wrapper_webkit = self.mox.CreateMockAnything()
    scm_wrapper_breakpad = self.mox.CreateMockAnything()
    scm_wrapper_cygwin = self.mox.CreateMockAnything()
    scm_wrapper_python = self.mox.CreateMockAnything()
    options = self.Options()
    options.revisions = [ 'src@123', 'foo/third_party/WebKit@42',
                          'src/third_party/cygwin@333' ]

    # Also, pymox doesn't verify the order of function calling w.r.t. different
    # mock objects. Pretty lame. So reorder as we wish to make it clearer.
    gclient.FileRead(os.path.join(self.root_dir, 'src', options.deps_file)
        ).AndReturn(deps_content)
    gclient.FileWrite(os.path.join(self.root_dir, options.entries_filename),
                      entries_content)

    gclient.os.path.exists(os.path.join(self.root_dir, 'src', '.git')
        ).AndReturn(False)
    gclient.os.path.exists(os.path.join(self.root_dir, 'foo/third_party/WebKit',
                                        '.git')
        ).AndReturn(False)
    gclient.os.path.exists(os.path.join(self.root_dir, 'src/third_party/cygwin',
                                        '.git')
        ).AndReturn(False)
    gclient.os.path.exists(os.path.join(self.root_dir,
                                         'src/third_party/python_24', '.git')
        ).AndReturn(False)
    gclient.os.path.exists(os.path.join(self.root_dir, 'src/breakpad/bar',
                                        '.git')
        ).AndReturn(False)
    gclient.os.path.exists(os.path.join(self.root_dir, options.entries_filename)
        ).AndReturn(False)

    gclient_scm.CreateSCM(self.url, self.root_dir, 'src').AndReturn(
        scm_wrapper_src)
    scm_wrapper_src.RunCommand('update', mox.Func(OptIsRev123), self.args, [])

    gclient_scm.CreateSCM(self.url, self.root_dir,
                       None).AndReturn(scm_wrapper_src2)
    scm_wrapper_src2.FullUrlForRelativeUrl('/trunk/deps/third_party/cygwin@3248'
        ).AndReturn(cygwin_path)

    gclient_scm.CreateSCM(self.url, self.root_dir,
                       None).AndReturn(scm_wrapper_src2)
    scm_wrapper_src2.FullUrlForRelativeUrl('/trunk/deps/third_party/WebKit'
        ).AndReturn(webkit_path)

    gclient_scm.CreateSCM(webkit_path, self.root_dir,
                       'foo/third_party/WebKit').AndReturn(scm_wrapper_webkit)
    scm_wrapper_webkit.RunCommand('update', mox.Func(OptIsRev42), self.args, [])

    gclient_scm.CreateSCM(
        'http://google-breakpad.googlecode.com/svn/trunk/src@285',
        self.root_dir, 'src/breakpad/bar').AndReturn(scm_wrapper_breakpad)
    scm_wrapper_breakpad.RunCommand('update', mox.Func(OptIsRevNone),
                                    self.args, [])

    gclient_scm.CreateSCM(cygwin_path, self.root_dir,
                       'src/third_party/cygwin').AndReturn(scm_wrapper_cygwin)
    scm_wrapper_cygwin.RunCommand('update', mox.Func(OptIsRev333), self.args,
                                  [])

    gclient_scm.CreateSCM('svn://random_server:123/trunk/python_24@5580',
                       self.root_dir,
                       'src/third_party/python_24').AndReturn(
                            scm_wrapper_python)
    scm_wrapper_python.RunCommand('update', mox.Func(OptIsRevNone), self.args,
                                  [])

    self.mox.ReplayAll()
    client = self._gclient_gclient(self.root_dir, options)
    client.SetConfig(gclient_config)
    client.RunOnDeps('update', self.args)

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
    client = self._gclient_gclient(self.root_dir, options)
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

    webkit_path = 'dummy path webkit'

    entries_content = (
      "entries = \\\n"
      "{ 'foo/third_party/WebKit': '%s',\n"
      "  '%s': '%s'}\n"
    ) % (webkit_path, name, self.url)

    scm_wrapper_webkit = self.mox.CreateMockAnything()
    scm_wrapper_src = self.mox.CreateMockAnything()

    options = self.Options()
    gclient.FileRead(os.path.join(self.root_dir, name, options.deps_file)
        ).AndReturn(deps_content)
    gclient.FileWrite(os.path.join(self.root_dir, options.entries_filename),
                      entries_content)

    gclient.os.path.exists(os.path.join(self.root_dir, 'foo/third_party/WebKit',
                                        '.git')).AndReturn(False)
    gclient.os.path.exists(os.path.join(self.root_dir, name, '.git')
        ).AndReturn(False)
    gclient.os.path.exists(os.path.join(self.root_dir, options.entries_filename)
        ).AndReturn(False)
    gclient_scm.CreateSCM(self.url, self.root_dir, name).AndReturn(
        gclient_scm.CreateSCM)
    gclient_scm.CreateSCM.RunCommand('update', options, self.args, [])

    gclient_scm.CreateSCM(self.url, self.root_dir,
                       None).AndReturn(scm_wrapper_src)
    scm_wrapper_src.FullUrlForRelativeUrl('/trunk/bar/WebKit'
        ).AndReturn(webkit_path)

    gclient_scm.CreateSCM(webkit_path, self.root_dir,
        'foo/third_party/WebKit').AndReturn(gclient_scm.CreateSCM)
    gclient_scm.CreateSCM.RunCommand('update', options, self.args, [])

    self.mox.ReplayAll()
    client = self._gclient_gclient(self.root_dir, options)
    client.SetConfig(gclient_config)
    client.RunOnDeps('update', self.args)

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

    webkit_path = 'dummy path webkit'

    entries_content = (
      "entries = \\\n"
      "{ 'foo/third_party/WebKit': '%s',\n"
      "  '%s': '%s'}\n"
    ) % (webkit_path, name, self.url)

    scm_wrapper_webkit = self.mox.CreateMockAnything()
    scm_wrapper_src = self.mox.CreateMockAnything()

    options = self.Options()
    gclient.FileRead(os.path.join(self.root_dir, name, options.deps_file)
        ).AndReturn(deps_content)
    gclient.FileWrite(os.path.join(self.root_dir, options.entries_filename),
                      entries_content)

    gclient.os.path.exists(os.path.join(self.root_dir, 'foo/third_party/WebKit',
                                        '.git')
        ).AndReturn(False)
    gclient.os.path.exists(os.path.join(self.root_dir, name, '.git')
        ).AndReturn(False)
    gclient.os.path.exists(os.path.join(self.root_dir, options.entries_filename)
        ).AndReturn(False)
    gclient_scm.CreateSCM(self.url, self.root_dir, name).AndReturn(
        gclient_scm.CreateSCM)
    gclient_scm.CreateSCM.RunCommand('update', options, self.args, [])

    gclient_scm.CreateSCM(self.url, self.root_dir,
                       None).AndReturn(scm_wrapper_src)
    scm_wrapper_src.FullUrlForRelativeUrl('/trunk/bar_custom/WebKit'
        ).AndReturn(webkit_path)

    gclient_scm.CreateSCM(webkit_path, self.root_dir,
        'foo/third_party/WebKit').AndReturn(gclient_scm.CreateSCM)
    gclient_scm.CreateSCM.RunCommand('update', options, self.args, [])

    self.mox.ReplayAll()
    client = self._gclient_gclient(self.root_dir, options)
    client.SetConfig(gclient_config)
    client.RunOnDeps('update', self.args)

  def testRunOnDepsFailureVars(self):
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

    options = self.Options()
    gclient.FileRead(os.path.join(self.root_dir, name, options.deps_file)
        ).AndReturn(deps_content)
    gclient_scm.CreateSCM(self.url, self.root_dir, name).AndReturn(
        gclient_scm.CreateSCM)
    gclient_scm.CreateSCM.RunCommand('update', options, self.args, [])

    self.mox.ReplayAll()
    client = self._gclient_gclient(self.root_dir, options)
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
    client = self._gclient_gclient(self.root_dir, options)
    exception = "'foo' is an unsupported command"
    self.assertRaisesError(exception, self._gclient_gclient.RunOnDeps, client,
                           'foo', self.args)

  def testRunOnDepsFailureEmpty(self):
    options = self.Options()

    self.mox.ReplayAll()
    client = self._gclient_gclient(self.root_dir, options)
    exception = "No solution specified"
    self.assertRaisesError(exception, self._gclient_gclient.RunOnDeps, client,
                           'update', self.args)

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


class SubprocessCallAndFilterTestCase(BaseTestCase):
  def setUp(self):
    BaseTestCase.setUp(self)
    self.mox.StubOutWithMock(gclient_utils, 'subprocess')
    self.mox.StubOutWithMock(gclient_scm, 'CaptureSVN')

  def testSubprocessCallAndFilter(self):
    command = ['boo', 'foo', 'bar']
    in_directory = 'bleh'
    fail_status = None
    pattern = 'a(.*)b'
    test_string = 'ahah\naccb\nallo\naddb\n'
    class Mock(object):
      stdout = StringIO.StringIO(test_string)
      def wait(self):
        pass
    kid = Mock()
    print("\n________ running 'boo foo bar' in 'bleh'")
    for i in test_string:
      gclient.sys.stdout.write(i)
    gclient_utils.subprocess.Popen(command, bufsize=0, cwd=in_directory,
      shell=(gclient.sys.platform == 'win32'),
      stdout=gclient_utils.subprocess.PIPE).AndReturn(kid)
    self.mox.ReplayAll()
    compiled_pattern = re.compile(pattern)
    line_list = []
    capture_list = []
    def FilterLines(line):
      line_list.append(line)
      match = compiled_pattern.search(line)
      if match:
        capture_list.append(match.group(1))
    gclient_utils.SubprocessCallAndFilter(command, in_directory,
                                          True, True,
                                          fail_status, FilterLines)
    self.assertEquals(line_list, ['ahah', 'accb', 'allo', 'addb'])
    self.assertEquals(capture_list, ['cc', 'dd'])

  def testCaptureSVNStatus(self):
    x = self
    def CaptureSVNMock(command, in_directory=None, print_error=True):
      x.assertEquals(in_directory, None)
      x.assertEquals(print_error, True)
      x.assertEquals(['status', '--xml', '.'], command)
      return r"""<?xml version="1.0"?>
<status>
<target path=".">
<entry path="unversionned_file.txt">
<wc-status props="none" item="unversioned"></wc-status>
</entry>
<entry path="build\internal\essential.vsprops">
<wc-status props="normal" item="modified" revision="14628">
<commit revision="13818">
<author>ajwong@chromium.org</author>
<date>2009-04-16T00:42:06.872358Z</date>
</commit>
</wc-status>
</entry>
<entry path="chrome\app\d">
<wc-status props="none" copied="true" tree-conflicted="true" item="added">
</wc-status>
</entry>
<entry path="chrome\app\DEPS">
<wc-status props="modified" item="modified" revision="14628">
<commit revision="1279">
<author>brettw@google.com</author>
<date>2008-08-23T17:16:42.090152Z</date>
</commit>
</wc-status>
</entry>
<entry path="scripts\master\factory\gclient_factory.py">
<wc-status props="normal" item="conflicted" revision="14725">
<commit revision="14633">
<author>nsylvain@chromium.org</author>
<date>2009-04-27T19:37:17.977400Z</date>
</commit>
</wc-status>
</entry>
</target>
</status>
"""
    gclient_scm.CaptureSVN = CaptureSVNMock
    info = gclient_scm.CaptureSVNStatus('.')
    expected = [
      ('?      ', 'unversionned_file.txt'),
      ('M      ', 'build\\internal\\essential.vsprops'),
      ('A  +   ', 'chrome\\app\\d'),
      ('MM     ', 'chrome\\app\\DEPS'),
      ('C      ', 'scripts\\master\\factory\\gclient_factory.py'),
    ]
    self.assertEquals(sorted(info), sorted(expected))

  def testCaptureSVNStatusEmpty(self):
    x = self
    def CaptureSVNMock(command, in_directory=None, print_error=True):
      x.assertEquals(in_directory, None)
      x.assertEquals(['status', '--xml'], command)
      return r"""<?xml version="1.0"?>
<status>
<target
   path="perf">
</target>
</status>
"""
    gclient_scm.CaptureSVN = CaptureSVNMock
    info = gclient_scm.CaptureSVNStatus(None)
    self.assertEquals(info, [])


if __name__ == '__main__':
  unittest.main()

# vim: ts=2:sw=2:tw=80:et:
