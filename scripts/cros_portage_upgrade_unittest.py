# -*- coding: utf-8 -*-
# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for cros_portage_upgrade.py."""

from __future__ import print_function

import filecmp
import os
import re
import tempfile
import unittest

import mock

from chromite.lib import cros_build_lib
from chromite.lib import cros_logging as logging
from chromite.lib import cros_test_lib
from chromite.lib import depgraph
from chromite.lib import osutils
from chromite.lib import portage_util
from chromite.lib import upgrade_table as utable
from chromite.scripts import cros_portage_upgrade as cpu

# This left in, but disabled, until we can get GetCurrentVersionsTest
# working again.
# pylint: disable=import-error
#from portage.package.ebuild import config as portcfg


# This unittest module pokes a lot of internal cros_portage_upgrade state
# that we don't want exported to anyone else.
# pylint: disable=protected-access


# Enable color invariably. Since we rely on color for error/warn message
# recognition, leaving this to be decided based on stdout being a tty
# will make the tests fail/succeed based on how they are run.
cpu.oper._color._enabled = True

# Configuration for generating a temporary valid ebuild hierarchy.
# ResolverPlayground sets up a default profile with ARCH=x86, so
# other architectures are irrelevant for now.
DEFAULT_ARCH = 'x86'
EBUILDS = {
    'dev-libs/A-1': {'RDEPEND': 'dev-libs/B'},
    'dev-libs/A-2': {'RDEPEND': 'dev-libs/B'},
    'dev-libs/B-1': {'RDEPEND': 'dev-libs/C'},
    'dev-libs/B-2': {'RDEPEND': 'dev-libs/C'},
    'dev-libs/C-1': {},
    'dev-libs/C-2': {},
    'dev-libs/D-1': {'RDEPEND': '!dev-libs/E'},
    'dev-libs/D-2': {},
    'dev-libs/D-3': {},
    'dev-libs/E-2': {'RDEPEND': '!dev-libs/D'},
    'dev-libs/E-3': {},

    'dev-libs/F-1': {'SLOT': '1'},
    'dev-libs/F-2': {'SLOT': '2'},
    'dev-libs/F-2-r1': {
        'SLOT': '2',
        'KEYWORDS': '~amd64 ~x86 ~arm',
    },

    'dev-apps/X-1': {
        'EAPI': '3',
        'SLOT': '0',
        'KEYWORDS': 'amd64 arm x86',
        'RDEPEND': '=dev-libs/C-1',
    },
    'dev-apps/Y-2': {
        'EAPI': '3',
        'SLOT': '0',
        'KEYWORDS': 'amd64 arm x86',
        'RDEPEND': '=dev-libs/C-2',
    },

    'chromeos-base/flimflam-0.0.1-r228': {
        'EAPI': '2',
        'SLOT': '0',
        'KEYWORDS': 'amd64 x86 arm',
        'RDEPEND': '>=dev-libs/D-2',
    },
    'chromeos-base/flimflam-0.0.2-r123': {
        'EAPI': '2',
        'SLOT': '0',
        'KEYWORDS': '~amd64 ~x86 ~arm',
        'RDEPEND': '>=dev-libs/D-3',
    },
    'chromeos-base/libchrome-57098-r4': {
        'EAPI': '2',
        'SLOT': '0',
        'KEYWORDS': 'amd64 x86 arm',
        'RDEPEND': '>=dev-libs/E-2',
    },
    'chromeos-base/libcros-1': {
        'EAPI': '2',
        'SLOT': '0',
        'KEYWORDS': 'amd64 x86 arm',
        'RDEPEND': 'dev-libs/B dev-libs/C chromeos-base/flimflam',
        'DEPEND': ('dev-libs/B dev-libs/C chromeos-base/flimflam '
                   'chromeos-base/libchrome'),
    },

    'virtual/libusb-0': {
        'EAPI': '2',
        'SLOT': '0',
        'RDEPEND': (
            '|| ( >=dev-libs/libusb-0.1.12-r1:0 dev-libs/libusb-compat '
            '>=sys-freebsd/freebsd-lib-8.0[usb] )'
        ),
    },
    'virtual/libusb-1': {
        'EAPI':'2', 'SLOT': '1',
        'RDEPEND': '>=dev-libs/libusb-1.0.4:1',
    },
    'dev-libs/libusb-0.1.13': {},
    'dev-libs/libusb-1.0.5': {'SLOT':'1'},
    'dev-libs/libusb-compat-1': {},
    'sys-freebsd/freebsd-lib-8': {'IUSE': '+usb'},

    'sys-fs/udev-164': {'EAPI': '1', 'RDEPEND': 'virtual/libusb:0'},

    'virtual/jre-1.5.0': {
        'SLOT': '1.5',
        'RDEPEND': '|| ( =dev-java/sun-jre-bin-1.5.0* =virtual/jdk-1.5.0* )',
    },
    'virtual/jre-1.5.0-r1': {
        'SLOT': '1.5',
        'RDEPEND': '|| ( =dev-java/sun-jre-bin-1.5.0* =virtual/jdk-1.5.0* )',
    },
    'virtual/jre-1.6.0': {
        'SLOT': '1.6',
        'RDEPEND': '|| ( =dev-java/sun-jre-bin-1.6.0* =virtual/jdk-1.6.0* )',
    },
    'virtual/jre-1.6.0-r1': {
        'SLOT': '1.6',
        'RDEPEND': '|| ( =dev-java/sun-jre-bin-1.6.0* =virtual/jdk-1.6.0* )',
    },
    'virtual/jdk-1.5.0': {
        'SLOT': '1.5',
        'RDEPEND': '|| ( =dev-java/sun-jdk-1.5.0* dev-java/gcj-jdk )',
    },
    'virtual/jdk-1.5.0-r1': {
        'SLOT': '1.5',
        'RDEPEND': '|| ( =dev-java/sun-jdk-1.5.0* dev-java/gcj-jdk )',
    },
    'virtual/jdk-1.6.0': {
        'SLOT': '1.6',
        'RDEPEND': '|| ( =dev-java/icedtea-6* =dev-java/sun-jdk-1.6.0* )',
    },
    'virtual/jdk-1.6.0-r1': {
        'SLOT': '1.6',
        'RDEPEND': '|| ( =dev-java/icedtea-6* =dev-java/sun-jdk-1.6.0* )',
    },
    'dev-java/gcj-jdk-4.5': {},
    'dev-java/gcj-jdk-4.5-r1': {},
    'dev-java/icedtea-6.1': {},
    'dev-java/icedtea-6.1-r1': {},
    'dev-java/sun-jdk-1.5': {'SLOT': '1.5'},
    'dev-java/sun-jdk-1.6': {'SLOT': '1.6'},
    'dev-java/sun-jre-bin-1.5': {'SLOT': '1.5'},
    'dev-java/sun-jre-bin-1.6': {'SLOT': '1.6'},

    'dev-java/ant-core-1.8': {'DEPEND': '>=virtual/jdk-1.4'},
    'dev-db/hsqldb-1.8': {'RDEPEND': '>=virtual/jre-1.6'},
}

WORLD = [
    'dev-libs/A',
    'dev-libs/D',
    'virtual/jre',
]

INSTALLED = {
    'dev-libs/A-1': {},
    'dev-libs/B-1': {},
    'dev-libs/C-1': {},
    'dev-libs/D-1': {},

    'virtual/jre-1.5.0': {
        'SLOT': '1.5',
        'RDEPEND': '|| ( =virtual/jdk-1.5.0* =dev-java/sun-jre-bin-1.5.0* )',
    },
    'virtual/jre-1.6.0': {
        'SLOT': '1.6',
        'RDEPEND': '|| ( =virtual/jdk-1.6.0* =dev-java/sun-jre-bin-1.6.0* )',
    },
    'virtual/jdk-1.5.0': {
        'SLOT': '1.5',
        'RDEPEND': '|| ( =dev-java/sun-jdk-1.5.0* dev-java/gcj-jdk )',
    },
    'virtual/jdk-1.6.0': {
        'SLOT': '1.6',
        'RDEPEND': '|| ( =dev-java/icedtea-6* =dev-java/sun-jdk-1.6.0* )',
    },
    'dev-java/gcj-jdk-4.5': {},
    'dev-java/icedtea-6.1': {},

    'virtual/libusb-0': {
        'EAPI': '2',
        'SLOT': '0',
        'RDEPEND': (
            '|| ( >=dev-libs/libusb-0.1.12-r1:0 dev-libs/libusb-compat '
            '>=sys-freebsd/freebsd-lib-8.0[usb] )'
        )
    },
}

# For verifying dependency graph results.
GOLDEN_DEP_GRAPHS = {
    'dev-libs/A-2': {
        'needs': {'dev-libs/B-2': 'runtime'},
        'action': 'merge',
    },
    'dev-libs/B-2': {'needs': {'dev-libs/C-2': 'runtime'}},
    'dev-libs/C-2': {'needs': {}},
    'dev-libs/D-3': {'needs': {}},
    # TODO(mtennant): Bug in parallel_emerge deps graph makes blocker show up
    # for E-3, rather than in just E-2 where it belongs.
    # See https://crbug.com/205225.
    # To repeat bug, swap the commented status of next two lines.
    #'dev-libs/E-3': {'needs': {}},
    'dev-libs/E-3': {'needs': {'dev-libs/D-3': 'blocker'}},
    'chromeos-base/libcros-1': {
        'needs': {
            'dev-libs/B-2': 'runtime/buildtime',
            'dev-libs/C-2': 'runtime/buildtime',
            'chromeos-base/libchrome-57098-r4': 'buildtime',
            'chromeos-base/flimflam-0.0.1-r228': 'runtime/buildtime'
        }
    },
    'chromeos-base/flimflam-0.0.1-r228': {
        'needs': {'dev-libs/D-3': 'runtime'},
    },
    'chromeos-base/libchrome-57098-r4': {
        'needs': {'dev-libs/E-3': 'runtime'},
    },
}

# For verifying dependency set results.
GOLDEN_DEP_SETS = {
    'dev-libs/A': set(['dev-libs/A-2', 'dev-libs/B-2', 'dev-libs/C-2']),
    'dev-libs/B': set(['dev-libs/B-2', 'dev-libs/C-2']),
    'dev-libs/C': set(['dev-libs/C-2']),
    'dev-libs/D': set(['dev-libs/D-3']),
    'virtual/libusb': set(['virtual/libusb-1', 'dev-libs/libusb-1.0.5']),
    'chromeos-base/libcros': set(['chromeos-base/libcros-1',
                                  'dev-libs/B-2',
                                  'chromeos-base/libchrome-57098-r4',
                                  'dev-libs/E-3',
                                  'chromeos-base/flimflam-0.0.1-r228',
                                  'dev-libs/D-3',
                                  'dev-libs/C-2',])
}


def _GetGoldenDepsSet(pkg):
  """Retrieve the golden dependency set for |pkg| from GOLDEN_DEP_SETS."""
  return GOLDEN_DEP_SETS.get(pkg, None)


def _VerifyDepsGraph(deps_graph, pkgs):
  for pkg in pkgs:
    if not _VerifyDepsGraphOnePkg(deps_graph, pkg):
      return False

  return True


def _VerifyDepsGraphOnePkg(deps_graph, pkg):
  """Verfication function to validate deps graph for |pkg|."""

  if deps_graph is None:
    print('Error: no dependency graph passed into _GetPreOrderDepGraph')
    return False

  if not isinstance(deps_graph, dict):
    print('Error: dependency graph is expected to be a dict.  Instead:\n%r' %
          deps_graph)
    return False

  validated = True

  golden_deps_set = _GetGoldenDepsSet(pkg)
  if golden_deps_set is None:
    print('Error: golden dependency list not configured for %s package' % pkg)
    validated = False

  # Verify dependencies by comparing them to GOLDEN_DEP_GRAPHS.
  for p in deps_graph:
    golden_pkg_info = None
    try:
      golden_pkg_info = GOLDEN_DEP_GRAPHS[p]
    except KeyError:
      print('Error: golden dependency graph not configured for %s package' % p)
      validated = False
      continue

    pkg_info = deps_graph[p]
    for key in golden_pkg_info:
      golden_value = golden_pkg_info[key]
      value = pkg_info[key]
      if not value == golden_value:
        print('Error: while verifying "%s" value for %s package,'
              ' expected:\n%r\nBut instead found:\n%r'
              % (key, p, golden_value, value))
        validated = False

  if not validated:
    print('Error: dependency graph for %s is not as expected.  Instead:\n%r' %
          (pkg, deps_graph))

  return validated


def _GenDepsGraphVerifier(pkgs):
  """Generate a graph verification function for the given package."""
  return lambda deps_graph: _VerifyDepsGraph(deps_graph, pkgs)


class ManifestLine(object):
  """Class to represent a Manifest line."""

  __slots__ = (
      'type',    # DIST, EBUILD, etc.
      'file',
      'size',
      'RMD160',
      'SHA1',
      'SHA256',
  )

  __attrlist__ = __slots__

  def __init__(self, line=None, **kwargs):
    """Parse |line| from manifest file."""
    if line:
      tokens = line.split()
      self.type = tokens[0]
      self.file = tokens[1]
      self.size = tokens[2]
      self.RMD160 = tokens[4]
      self.SHA1 = tokens[6]
      self.SHA256 = tokens[8]

      assert tokens[3] == 'RMD160'
      assert tokens[5] == 'SHA1'
      assert tokens[7] == 'SHA256'

    # Entries in kwargs are overwrites.
    for attr in self.__attrlist__:
      if attr in kwargs or not hasattr(self, attr):
        setattr(self, attr, kwargs.get(attr))

  def __str__(self):
    return ('%s %s %s RMD160 %s SHA1 %s SHA256 %s' %
            (self.type, self.file, self.size,
             self.RMD160, self.SHA1, self.SHA256))

  def __eq__(self, other):
    """Equality support."""
    if not isinstance(other, type(self)):
      return False

    no_attr = object()
    for attr in self.__attrlist__:
      if getattr(self, attr, no_attr) != getattr(other, attr, no_attr):
        return False

    return True

  def __ne__(self, other):
    """Inequality for completeness."""
    return not self == other


class PInfoTest(cros_test_lib.OutputTestCase):
  """Tests for the PInfo class."""

  def testInit(self):
    pinfo = cpu.PInfo(category='SomeCat', user_arg='SomeArg')

    self.assertEqual('SomeCat', pinfo.category)
    self.assertEqual('SomeArg', pinfo.user_arg)

    self.assertEqual(None, pinfo.cpv)
    self.assertEqual(None, pinfo.overlay)

    self.assertRaises(AttributeError, getattr, pinfo, 'foobar')

  def testEqAndNe(self):
    pinfo1 = cpu.PInfo(category='SomeCat', user_arg='SomeArg')

    self.assertEqual(pinfo1, pinfo1)
    self.assertTrue(pinfo1 == pinfo1)
    self.assertFalse(pinfo1 != pinfo1)

    pinfo2 = cpu.PInfo(category='SomeCat', user_arg='SomeArg')

    self.assertEqual(pinfo1, pinfo2)
    self.assertTrue(pinfo1 == pinfo2)
    self.assertFalse(pinfo1 != pinfo2)

    pinfo3 = cpu.PInfo(category='SomeCat', user_arg='SomeOtherArg')

    self.assertNotEqual(pinfo1, pinfo3)
    self.assertFalse(pinfo1 == pinfo3)
    self.assertTrue(pinfo1 != pinfo3)

    pinfo4 = cpu.PInfo(category='SomeCat', slot='SomeSlot')

    self.assertNotEqual(pinfo1, pinfo4)
    self.assertFalse(pinfo1 == pinfo4)
    self.assertTrue(pinfo1 != pinfo4)


class CpuTestBase(cros_test_lib.MockTempDirTestCase,
                  cros_test_lib.OutputTestCase):
  """Base class for all test classes in this file."""

  def setUp(self):
    # Mock overlay we can run tests against.  Tests have to manually call
    # _SetUpPlayground first to initialize it.
    self.portage_stable = os.path.join(self.tempdir, 'portage-stable')

    # A mock system where we have packages installed.
    self.eroot = os.path.join(self.tempdir, 'root')

    # Where we clone the upstream git repo to.
    self.upstream_tmp_repo = os.path.join(self.tempdir, 'git-checkout')

  def _SetUpPlayground(self, ebuilds=None, installed=None, world=None):
    """Prepare the temporary ebuild playground.

    This used to leverage portage.tests.resolver.ResolverPlayground, but that
    module isn't installed anymore.  So we do some of our own ad-hoc setup in
    place of the portage code.  This works for some unittests, but needs to
    fill out the playground more for others.  See the unittest.skip tests for
    the ones we've disabled for now.

    Args:
      ebuilds: A list of hashes representing ebuild files in a portdir.
      installed: A list of hashes representing ebuilds files already installed.
      world: A list of lines to simulate in the world file.
    """
    # It's safe to use these globals as we treat these dicts are read-only.
    if ebuilds is None:
      ebuilds = EBUILDS
    if installed is None:
      installed = INSTALLED
    if world is None:
      world = WORLD

    osutils.SafeMakedirs(self.portage_stable)

    # Default keys that we often look at for packages.
    DEFAULT_METADATA = {
        'EAPI': '5',
        'KEYWORDS': DEFAULT_ARCH,
        'SLOT': '0',
    }

    var_lib_portage = os.path.join(self.eroot, 'var', 'lib', 'portage')
    osutils.WriteFile(os.path.join(var_lib_portage, 'world'),
                      '\n'.join(world), makedirs=True)

    vdb = os.path.join(var_lib_portage, 'pkgs')
    for ebuild, custom_metadata in installed.items():
      cpv = portage_util.SplitCPV(ebuild)
      vdb_pkg = os.path.join(vdb, cpv.cpf)
      osutils.SafeMakedirs(vdb_pkg)
      metadata = DEFAULT_METADATA.copy()
      metadata.update(custom_metadata)
      for key, value in metadata.items():
        osutils.WriteFile(os.path.join(vdb_pkg, key), value)

    for ebuild, custom_metadata in ebuilds.items():
      cpv = portage_util.SplitCPV(ebuild)
      metadata = DEFAULT_METADATA.copy()
      metadata.update(custom_metadata)
      content = 'EAPI=%s\n' % metadata.pop('EAPI')
      content += '\n'.join('%s="%s"' % x for x in metadata.items())
      osutils.WriteFile(os.path.join(self.upstream_tmp_repo, cpv.category,
                                     cpv.package,
                                     os.path.basename(cpv.cpf) + '.ebuild'),
                        content, makedirs=True)

    # Set all envvars needed by parallel_emerge, since parallel_emerge
    # normally does that when --board is given.
    os.environ.update({
        'PORTAGE_CONFIGROOT': self.eroot,
        'ROOT': self.eroot,
        'PORTDIR': self.portage_stable,
        # See _GenPortageEnvvars for more info on this setting.
        'PORTDIR_OVERLAY': self.portage_stable,
    })

  def _MockUpgrader(self, cmdargs=None, **kwargs):
    """Set up a mocked Upgrader object with the given args."""
    upgrader_slot_defaults = {
        '_curr_arch': DEFAULT_ARCH,
        '_curr_board': 'some_board',
        '_unstable_ok': False,
        '_verbose': False,
    }

    # The mock autospec will mock out class constants too.  Copy over the
    # members by hand (assuming everything uppercase is a constant).
    upgrader = mock.create_autospec(cpu.Upgrader)
    for member in dir(cpu.Upgrader):
      if member.upper() == member:
        setattr(upgrader, member, getattr(cpu.Upgrader, member))

    # Initialize each attribute with default value.
    for slot in cpu.Upgrader.__slots__:
      value = upgrader_slot_defaults.get(slot, None)
      upgrader.__setattr__(slot, value)

    # Initialize with command line if given.
    if cmdargs is not None:
      parser = cpu._CreateParser()
      options = parser.parse_args(cmdargs)
      cpu.Upgrader.__init__(upgrader, options)

    # Override Upgrader attributes if requested.
    for key, value in kwargs.items():
      self.assertIn(key, cpu.Upgrader.__slots__)
      upgrader.__setattr__(key, value)

    # Point to our tempdir to avoid clobbering /tmp/portage by accident.
    upgrader.UPSTREAM_TMP_REPO = upgrader._upstream = self.upstream_tmp_repo
    upgrader._stable_repo = self.portage_stable

    return upgrader


class CopyUpstreamTest(CpuTestBase):
  """Test Upgrader._CopyUpstreamPackage, _CopyUpstreamEclass, _CreateManifest"""

  def _AddEclassToPlayground(self, eclass, content='',
                             ebuilds=None, missing=False):
    """Hack to insert an eclass into the playground source.

    Args:
      eclass: Name of eclass to create (without .eclass suffix).  Will be
        created as an empty file unless |content| is specified.
      content: Text to put into created eclass.
      ebuilds: List of ebuilds to put inherit line into.  Should be path
        to ebuild from playground portdir.
      missing: If True, do not actually create the eclass file.  Only makes
        sense if |ebuilds| is non-empty, presumably to test inherit failure.
    """
    if ebuilds is None:
      ebuilds = []

    portdir = self.upstream_tmp_repo
    eclass_path = os.path.join(portdir, 'eclass', '%s.eclass' % eclass)

    # Create the eclass file.
    osutils.WriteFile(eclass_path, content, makedirs=True)

    # Insert the inherit line into the ebuild file, if requested.
    for ebuild in ebuilds:
      ebuild_path = os.path.join(self.upstream_tmp_repo, ebuild)

      text = osutils.ReadFile(ebuild_path)

      def repl(match):
        return match.group(1) + '\ninherit ' + eclass
      text = re.sub(r'(EAPI.*)', repl, text)

      osutils.WriteFile(ebuild_path, text)

      # Remove the Manifest file.
      osutils.SafeUnlink(os.path.join(os.path.dirname(ebuild_path), 'Manifest'))

      # Recreate the Manifests using the ebuild utility.
      cmd = ['ebuild', ebuild_path, 'manifest']
      cros_build_lib.RunCommand(cmd, print_cmd=False, redirect_stdout=True,
                                combine_stdout_stderr=True)

    # If requested, remove the eclass.
    if missing:
      os.remove(eclass_path)

  #
  # _IdentifyNeededEclass
  #

  def _TestIdentifyNeededEclass(self, cpv, ebuild, eclass, create_eclass):
    """Test Upgrader._IdentifyNeededEclass"""
    self._SetUpPlayground()
    mocked_upgrader = self._MockUpgrader(cmdargs=[])
    self._AddEclassToPlayground(eclass,
                                ebuilds=[ebuild],
                                missing=not create_eclass)

    # Replay script.
    envvars = cpu.Upgrader._GenPortageEnvvars(mocked_upgrader,
                                              mocked_upgrader._curr_arch,
                                              unstable_ok=True)
    mocked_upgrader._GenPortageEnvvars.return_value = envvars
    mocked_upgrader._GetBoardCmd.return_value = 'equery'

    # Verify.
    with self.OutputCapturer():
      return cpu.Upgrader._IdentifyNeededEclass(mocked_upgrader, cpv)

  @unittest.skip('playground setup needs more work')
  def testIdentifyNeededEclassMissing(self):
    result = self._TestIdentifyNeededEclass('dev-libs/A-2',
                                            'dev-libs/A/A-2.ebuild',
                                            'inheritme',
                                            False)
    self.assertEqual('inheritme.eclass', result)

  def testIdentifyNeededEclassOK(self):
    result = self._TestIdentifyNeededEclass('dev-libs/A-2',
                                            'dev-libs/A/A-2.ebuild',
                                            'inheritme',
                                            True)
    self.assertIsNone(result)

  #
  # _CopyUpstreamEclass
  #

  def _TestCopyUpstreamEclass(self, eclass,
                              local_content=None, upstream_content=None):
    """Test Upgrader._CopyUpstreamEclass"""
    self._SetUpPlayground()
    mocked_upgrader = self._MockUpgrader(_curr_board=None)

    eclass_name = '%s.eclass' % eclass
    eclass_subpath = os.path.join('eclass', eclass_name)
    eclass_path = os.path.join(self.portage_stable, eclass_subpath)
    upstream_eclass_path = os.path.join(self.upstream_tmp_repo, eclass_subpath)

    if local_content:
      osutils.WriteFile(eclass_path, local_content, makedirs=True)

    if upstream_content:
      osutils.WriteFile(upstream_eclass_path, upstream_content, makedirs=True)

    # Verify.
    result = None
    with self.OutputCapturer():
      if upstream_content is None:
        self.assertRaises(RuntimeError, cpu.Upgrader._CopyUpstreamEclass,
                          mocked_upgrader, eclass_name)
      else:
        result = cpu.Upgrader._CopyUpstreamEclass(mocked_upgrader, eclass_name)

    if upstream_content and local_content != upstream_content:
      mocked_upgrader._RunGit.assert_called_once_with(
          mocked_upgrader._stable_repo, ['add', eclass_subpath])

      self.assertTrue(result)
      # Verify that eclass has been copied into portage-stable.
      self.assertExists(eclass_path)
      # Verify that eclass contents are correct.
      self.assertTrue(filecmp.cmp(upstream_eclass_path, eclass_path))

    else:
      self.assertFalse(result)

  def testCopyUpstreamEclassCopyBecauseMissing(self):
    self._TestCopyUpstreamEclass('inheritme',
                                 upstream_content='# Up')

  def testCopyUpstreamEclassCopyBecauseDifferent(self):
    self._TestCopyUpstreamEclass('inheritme',
                                 local_content='# Local',
                                 upstream_content='# Up')

  def testCopyUpstreamEclassNoCopyBecauseIdentical(self):
    self._TestCopyUpstreamEclass('inheritme',
                                 local_content='# Bar',
                                 upstream_content='# Bar')

  def testCopyUpstreamEclassNoCopyBecauseUpstreamMissing(self):
    self._TestCopyUpstreamEclass('inheritme',
                                 local_content='# Local')

  #
  # _CopyUpstreamPackage
  #

  def _TestCopyUpstreamPackage(self, catpkg, verrev, success,
                               existing_files, extra_upstream_files,
                               error=None):
    """Test Upgrader._CopyUpstreamPackage"""
    upstream_cpv = '%s-%s' % (catpkg, verrev)
    ebuild = '%s-%s.ebuild' % (catpkg.split('/')[-1], verrev)

    self._SetUpPlayground()

    # Simulate extra files in upsteam package dir.
    if extra_upstream_files:
      pkg_dir = os.path.join(self.upstream_tmp_repo, catpkg)
      if os.path.exists(pkg_dir):
        for extra_file in extra_upstream_files:
          osutils.Touch(os.path.join(pkg_dir, extra_file))

    # Prepare dummy portage-stable dir, with extra previously
    # existing files simulated if requested.
    if existing_files:
      pkg_dir = os.path.join(self.portage_stable, catpkg)
      for existing_file in existing_files:
        osutils.Touch(os.path.join(pkg_dir, existing_file), makedirs=True)

    mocked_upgrader = self._MockUpgrader(_curr_board=None)

    # Replay script.
    if success:
      if existing_files:
        def git_rm(cwd, cmd, **_kwargs):
          # Identify file that psuedo-git is to remove, then remove it.
          self.assertEqual('rm', cmd[0])
          self.assertEqual('-rf', cmd[1])

          # Remove the files after the options.
          paths = cmd[1:]
          while paths[0].startswith('-'):
            paths.pop(0)
          self.assertEqual(sorted(rm_list), sorted(paths))
          for path in paths:
            os.remove(os.path.join(cwd, path))

          # As with real "git rm", if the dir is then empty remove that.
          try:
            os.rmdir(cwd)
            os.rmdir(os.path.dirname(cwd))
          except OSError:
            pass

        rm_list = [os.path.join(catpkg, f) for f in existing_files]
        mocked_upgrader._RunGit.side_effect = git_rm

      mocked_upgrader._IdentifyNeededEclass.return_value = None

    # Verify.
    result = None
    with self.OutputCapturer():
      if error:
        self.assertRaises(error, cpu.Upgrader._CopyUpstreamPackage,
                          mocked_upgrader, upstream_cpv)
      else:
        result = cpu.Upgrader._CopyUpstreamPackage(mocked_upgrader,
                                                   upstream_cpv)

    if success:
      self.assertEqual(result, upstream_cpv)

      pkgdir = os.path.join(mocked_upgrader._stable_repo, catpkg)
      mocked_upgrader._CreateManifest.assert_called_once_with(
          os.path.join(self.upstream_tmp_repo, catpkg), pkgdir, ebuild)
      mocked_upgrader._IdentifyNeededEclass.assert_called_once_with(
          upstream_cpv)

      # Verify that ebuild has been copied into portage-stable.
      ebuild_path = os.path.join(self.portage_stable, catpkg, ebuild)
      self.assertExists(ebuild_path,
                        msg='Missing expected ebuild after copy from upstream')

      # Verify that any extra files upstream are also copied.
      for extra_file in extra_upstream_files:
        file_path = os.path.join(self.portage_stable, catpkg, extra_file)
        msg = ('Missing expected extra file %s after copy from upstream' %
               extra_file)
        self.assertExists(file_path, msg=msg)
    else:
      self.assertIsNone(result)

  def testCopyUpstreamPackageEmptyStable(self):
    existing_files = []
    extra_upstream_files = []
    self._TestCopyUpstreamPackage('dev-libs/D', '2', True,
                                  existing_files,
                                  extra_upstream_files)

  def testCopyUpstreamPackageClutteredStable(self):
    existing_files = ['foo', 'bar', 'foobar.ebuild', 'D-1.ebuild']
    extra_upstream_files = []
    self._TestCopyUpstreamPackage('dev-libs/D', '2', True,
                                  existing_files,
                                  extra_upstream_files)

  def testCopyUpstreamPackageVersionNotAvailable(self):
    """Should fail, dev-libs/D version 5 does not exist 'upstream'"""
    existing_files = []
    extra_upstream_files = []
    self._TestCopyUpstreamPackage('dev-libs/D', '5', False,
                                  existing_files,
                                  extra_upstream_files,
                                  error=RuntimeError)

  def testCopyUpstreamPackagePackageNotAvailable(self):
    """Should fail, a-b-c/D does not exist 'upstream' in any version"""
    existing_files = []
    extra_upstream_files = []
    self._TestCopyUpstreamPackage('a-b-c/D', '5', False,
                                  existing_files,
                                  extra_upstream_files,
                                  error=RuntimeError)

  def testCopyUpstreamPackageExtraUpstreamFiles(self):
    existing_files = ['foo', 'bar']
    extra_upstream_files = ['keepme', 'andme']
    self._TestCopyUpstreamPackage('dev-libs/F', '2-r1', True,
                                  existing_files,
                                  extra_upstream_files)

  def _SetupManifestTest(self, ebuild,
                         upstream_mlines, current_mlines):
    upstream_dir = tempfile.mkdtemp(dir=self.tempdir)
    current_dir = tempfile.mkdtemp(dir=self.tempdir)

    upstream_manifest = os.path.join(upstream_dir, 'Manifest')
    current_manifest = os.path.join(current_dir, 'Manifest')

    if upstream_mlines:
      osutils.WriteFile(upstream_manifest,
                        '\n'.join(str(x) for x in upstream_mlines) + '\n',
                        makedirs=True)

    if current_mlines:
      osutils.WriteFile(current_manifest,
                        '\n'.join(str(x) for x in current_mlines) + '\n',
                        makedirs=True)

    ebuild_path = os.path.join(current_dir, ebuild)

    # Add test-specific mocks/stubs.
    def CheckRunCommand(cmd, **kwargs):
      self.assertEqual(cmd, ['ebuild', ebuild_path, 'manifest'])
      self.assertTrue(kwargs.get('redirect_stdout'))
      return cros_build_lib.CommandResult(returncode=0, output='')

    self.PatchObject(cros_build_lib, 'RunCommand').side_effect = CheckRunCommand

    return (upstream_dir, current_dir)

  def _AssertManifestContents(self, manifest_path, expected_manifest_lines):
    manifest_lines = []
    with open(manifest_path, 'r') as f:
      for line in f:
        manifest_lines.append(ManifestLine(line))

    msg = ('Manifest contents not as expected.  Expected:\n%s\n'
           '\nBut got:\n%s\n' %
           ('\n'.join([str(ml) for ml in expected_manifest_lines]),
            '\n'.join([str(ml) for ml in manifest_lines])))
    self.assertTrue(manifest_lines == expected_manifest_lines, msg=msg)
    self.assertFalse(manifest_lines != expected_manifest_lines, msg=msg)

  def testCreateManifestNew(self):
    """Test case with upstream but no current Manifest."""

    mocked_upgrader = self._MockUpgrader()

    ebuild = 'some-pkg.ebuild'
    upst_mlines = [ManifestLine(type='DIST',
                                file='fileA',
                                size='100',
                                RMD160='abc',
                                SHA1='123',
                                SHA256='abc123'),
                   ManifestLine(type='EBUILD',
                                file=ebuild,
                                size='254',
                                RMD160='def',
                                SHA1='456',
                                SHA256='def456'),]
    upstream_dir, current_dir = self._SetupManifestTest(ebuild,
                                                        upst_mlines, None)

    upstream_manifest = os.path.join(upstream_dir, 'Manifest')
    current_manifest = os.path.join(current_dir, 'Manifest')

    # Run test verification.
    self.assertNotExists(current_manifest)
    cpu.Upgrader._CreateManifest(mocked_upgrader,
                                 upstream_dir, current_dir, ebuild)
    self.assertTrue(filecmp.cmp(upstream_manifest, current_manifest))

  def testCreateManifestMerge(self):
    """Test case with upstream but no current Manifest."""

    mocked_upgrader = self._MockUpgrader()

    ebuild = 'some-pkg.ebuild'
    curr_mlines = [ManifestLine(type='DIST',
                                file='fileA',
                                size='101',
                                RMD160='abc',
                                SHA1='123',
                                SHA256='abc123'),
                   ManifestLine(type='DIST',
                                file='fileC',
                                size='321',
                                RMD160='cde',
                                SHA1='345',
                                SHA256='cde345'),
                   ManifestLine(type='EBUILD',
                                file=ebuild,
                                size='254',
                                RMD160='def',
                                SHA1='789',
                                SHA256='def789'),]
    upst_mlines = [ManifestLine(type='DIST',
                                file='fileA',
                                size='100',
                                RMD160='abc',
                                SHA1='123',
                                SHA256='abc123'),
                   # This file is different from current manifest.
                   # It should be picked up by _CreateManifest.
                   ManifestLine(type='DIST',
                                file='fileB',
                                size='345',
                                RMD160='bcd',
                                SHA1='234',
                                SHA256='bcd234'),
                   ManifestLine(type='EBUILD',
                                file=ebuild,
                                size='254',
                                RMD160='def',
                                SHA1='789',
                                SHA256='def789'),]

    upstream_dir, current_dir = self._SetupManifestTest(ebuild,
                                                        upst_mlines,
                                                        curr_mlines)

    current_manifest = os.path.join(current_dir, 'Manifest')

    # Run test verification.
    self.assertExists(current_manifest)
    cpu.Upgrader._CreateManifest(mocked_upgrader,
                                 upstream_dir, current_dir, ebuild)

    expected_mlines = curr_mlines + upst_mlines[1:2]
    self._AssertManifestContents(current_manifest, expected_mlines)


class GetPackageUpgradeStateTest(CpuTestBase):
  """Test Upgrader._GetPackageUpgradeState"""

  def _TestGetPackageUpgradeState(self, pinfo, exists_upstream):
    cmdargs = []
    mocked_upgrader = self._MockUpgrader(cmdargs=cmdargs)

    def FindUpstream(pkg, unstable_ok=False):
      self.assertEqual(pinfo.cpv, pkg)
      self.assertTrue(unstable_ok)
      return exists_upstream
    mocked_upgrader._FindUpstreamCPV.side_effect = FindUpstream

    # Verify.
    return cpu.Upgrader._GetPackageUpgradeState(mocked_upgrader, pinfo)

  def testGetPackageUpgradeStateLocalOnly(self):
    pinfo = cpu.PInfo(cpv='foo/bar-2',
                      overlay='chromiumos-overlay',
                      cpv_cmp_upstream=None,
                      latest_upstream_cpv=None)
    result = self._TestGetPackageUpgradeState(pinfo, exists_upstream=False)
    self.assertEqual(result, utable.UpgradeTable.STATE_LOCAL_ONLY)

  def testGetPackageUpgradeStateUnknown(self):
    pinfo = cpu.PInfo(cpv='foo/bar-2',
                      overlay='portage',
                      cpv_cmp_upstream=None,
                      latest_upstream_cpv=None)
    result = self._TestGetPackageUpgradeState(pinfo, exists_upstream=False)
    self.assertEqual(result, utable.UpgradeTable.STATE_UNKNOWN)

  def testGetPackageUpgradeStateUpgradeAndDuplicated(self):
    pinfo = cpu.PInfo(cpv='foo/bar-2',
                      overlay='chromiumos-overlay',
                      cpv_cmp_upstream=1, # outdated
                      latest_upstream_cpv='not important')
    result = self._TestGetPackageUpgradeState(pinfo, exists_upstream=True)
    self.assertEqual(result,
                     utable.UpgradeTable.STATE_NEEDS_UPGRADE_AND_DUPLICATED)

  def testGetPackageUpgradeStateUpgradeAndPatched(self):
    pinfo = cpu.PInfo(cpv='foo/bar-2',
                      overlay='chromiumos-overlay',
                      cpv_cmp_upstream=1, # outdated
                      latest_upstream_cpv='not important')
    result = self._TestGetPackageUpgradeState(pinfo, exists_upstream=False)
    self.assertEqual(result,
                     utable.UpgradeTable.STATE_NEEDS_UPGRADE_AND_PATCHED)

  def testGetPackageUpgradeStateUpgrade(self):
    pinfo = cpu.PInfo(cpv='foo/bar-2',
                      overlay='portage-stable',
                      cpv_cmp_upstream=1, # outdated
                      latest_upstream_cpv='not important')
    result = self._TestGetPackageUpgradeState(pinfo, exists_upstream=False)
    self.assertEqual(result, utable.UpgradeTable.STATE_NEEDS_UPGRADE)

  def testGetPackageUpgradeStateDuplicated(self):
    pinfo = cpu.PInfo(cpv='foo/bar-2',
                      overlay='chromiumos-overlay',
                      cpv_cmp_upstream=0, # current
                      latest_upstream_cpv='not important')
    result = self._TestGetPackageUpgradeState(pinfo, exists_upstream=True)
    self.assertEqual(result, utable.UpgradeTable.STATE_DUPLICATED)

  def testGetPackageUpgradeStatePatched(self):
    pinfo = cpu.PInfo(cpv='foo/bar-2',
                      overlay='chromiumos-overlay',
                      cpv_cmp_upstream=0, # current
                      latest_upstream_cpv='not important')
    result = self._TestGetPackageUpgradeState(pinfo, exists_upstream=False)
    self.assertEqual(result, utable.UpgradeTable.STATE_PATCHED)

  def testGetPackageUpgradeStateCurrent(self):
    pinfo = cpu.PInfo(cpv='foo/bar-2',
                      overlay='portage-stable',
                      cpv_cmp_upstream=0, # current
                      latest_upstream_cpv='not important')
    result = self._TestGetPackageUpgradeState(pinfo, exists_upstream=False)
    self.assertEqual(result, utable.UpgradeTable.STATE_CURRENT)


@unittest.skip('playground setup needs more work')
class EmergeableTest(CpuTestBase):
  """Test Upgrader._AreEmergeable."""

  def _TestAreEmergeable(self, cpvlist, expect, world=None):
    """Test the Upgrader._AreEmergeable method.

    Args:
      cpvlist: Passed to _AreEmergeable.
      expect: Expected boolean return value of _AreEmergeable.
      world: List of lines to override default world contents.
    """
    # It's safe to use these globals as we treat these dicts are read-only.
    if world is None:
      world = WORLD

    cmdargs = ['--upgrade'] + cpvlist
    mocked_upgrader = self._MockUpgrader(cmdargs=cmdargs)
    self._SetUpPlayground(world=world)

    # Replay script.
    envvars = cpu.Upgrader._GenPortageEnvvars(mocked_upgrader,
                                              mocked_upgrader._curr_arch,
                                              unstable_ok=False)
    mocked_upgrader._GenPortageEnvvars.return_value = envvars
    mocked_upgrader._GetBoardCmd.return_value = 'emerge'

    # Verify.
    result = cpu.Upgrader._AreEmergeable(mocked_upgrader, cpvlist)

    mocked_upgrader._GenPortageEnvvars.assert_called_once_with(
        mocked_upgrader._curr_arch, unstable_ok=False)

    (code, _cmd, output) = result
    logging.debug('Test ended with success==%r (expected==%r)', code, expect)
    logging.debug('Emerge output:\n%s', output)

    self.assertEqual(code, expect)

  def testAreEmergeableOnePkg(self):
    """Should pass, one cpv target."""
    cpvlist = ['dev-libs/A-1']
    return self._TestAreEmergeable(cpvlist, True)

  def testAreEmergeableTwoPkgs(self):
    """Should pass, two cpv targets."""
    cpvlist = ['dev-libs/A-1', 'dev-libs/B-1']
    return self._TestAreEmergeable(cpvlist, True)

  def testAreEmergeableOnePkgTwoVersions(self):
    """Should fail, targets two versions of same package."""
    cpvlist = ['dev-libs/A-1', 'dev-libs/A-2']
    return self._TestAreEmergeable(cpvlist, False)

  def testAreEmergeableStableFlimFlam(self):
    """Should pass, target stable version of pkg."""
    cpvlist = ['chromeos-base/flimflam-0.0.1-r228']
    return self._TestAreEmergeable(cpvlist, True)

  def testAreEmergeableUnstableFlimFlam(self):
    """Should fail, target unstable version of pkg."""
    cpvlist = ['chromeos-base/flimflam-0.0.2-r123']
    return self._TestAreEmergeable(cpvlist, False)

  def testAreEmergeableBlockedPackages(self):
    """Should fail, targets have blocking deps on each other."""
    cpvlist = ['dev-libs/D-1', 'dev-libs/E-2']
    return self._TestAreEmergeable(cpvlist, False)

  def testAreEmergeableBlockedByInstalledPkg(self):
    """Should fail because of installed D-1 pkg."""
    cpvlist = ['dev-libs/E-2']
    return self._TestAreEmergeable(cpvlist, False)

  def testAreEmergeableNotBlockedByInstalledPkgNotInWorld(self):
    """Should pass because installed D-1 pkg not in world."""
    cpvlist = ['dev-libs/E-2']
    return self._TestAreEmergeable(cpvlist, True, world=[])

  def testAreEmergeableSamePkgDiffSlots(self):
    """Should pass, same package but different slots."""
    cpvlist = ['dev-libs/F-1', 'dev-libs/F-2']
    return self._TestAreEmergeable(cpvlist, True)

  def testAreEmergeableTwoPackagesIncompatibleDeps(self):
    """Should fail, targets depend on two versions of same pkg."""
    cpvlist = ['dev-apps/X-1', 'dev-apps/Y-2']
    return self._TestAreEmergeable(cpvlist, False)


class CPVUtilTest(cros_test_lib.TestCase):
  """Test various CPV utilities in Upgrader"""

  def _TestCmpCpv(self, cpv1, cpv2):
    """Test Upgrader._CmpCpv"""
    return cpu.Upgrader._CmpCpv(cpv1, cpv2)

  def testCmpCpv(self):
    # cpvs to compare.
    equal = [('foo/bar-1', 'foo/bar-1'),
             ('a-b-c/x-y-z-1.2.3-r1', 'a-b-c/x-y-z-1.2.3-r1'),
             ('foo/bar-1', 'foo/bar-1-r0'),
             (None, None)]
    for (cpv1, cpv2) in equal:
      self.assertEqual(0, self._TestCmpCpv(cpv1, cpv2))

    lessthan = [(None, 'foo/bar-1'),
                ('foo/bar-1', 'foo/bar-2'),
                ('foo/bar-1', 'foo/bar-1-r1'),
                ('foo/bar-1-r1', 'foo/bar-1-r2'),
                ('foo/bar-1.2.3', 'foo/bar-1.2.4'),
                ('foo/bar-5a', 'foo/bar-5b')]
    for (cpv1, cpv2) in lessthan:
      self.assertTrue(self._TestCmpCpv(cpv1, cpv2) < 0)
      self.assertTrue(self._TestCmpCpv(cpv2, cpv1) > 0)

    not_comparable = [('foo/bar-1', 'bar/foo-1')]
    for (cpv1, cpv2) in not_comparable:
      self.assertEqual(None, self._TestCmpCpv(cpv1, cpv2))

  def _TestGetCatPkgFromCpv(self, cpv):
    """Test Upgrader._GetCatPkgFromCpv"""
    return cpu.Upgrader._GetCatPkgFromCpv(cpv)

  def testGetCatPkgFromCpv(self):
    # (input, output) tuples.
    data = [('foo/bar-1', 'foo/bar'),
            ('a-b-c/x-y-z-1', 'a-b-c/x-y-z'),
            ('a-b-c/x-y-z-1.2.3-r3', 'a-b-c/x-y-z'),
            ('bar-1', 'bar'),
            ('bar', None)]

    for (cpv, catpn) in data:
      result = self._TestGetCatPkgFromCpv(cpv)
      self.assertEqual(catpn, result)

  def _TestGetVerRevFromCpv(self, cpv):
    """Test Upgrader._GetVerRevFromCpv"""
    return cpu.Upgrader._GetVerRevFromCpv(cpv)

  def testGetVerRevFromCpv(self):
    # (input, output) tuples.
    data = [('foo/bar-1', '1'),
            ('a-b-c/x-y-z-1', '1'),
            ('a-b-c/x-y-z-1.2.3-r3', '1.2.3-r3'),
            ('foo/bar-3.222-r0', '3.222'),
            ('bar-1', '1'),
            ('bar', None)]

    for (cpv, verrev) in data:
      result = self._TestGetVerRevFromCpv(cpv)
      self.assertEqual(verrev, result)

  def _TestGetEbuildPathFromCpv(self, cpv):
    """Test Upgrader._GetEbuildPathFromCpv"""
    return cpu.Upgrader._GetEbuildPathFromCpv(cpv)

  def testGetEbuildPathFromCpv(self):
    # (input, output) tuples.
    data = [('foo/bar-1', 'foo/bar/bar-1.ebuild'),
            ('a-b-c/x-y-z-1', 'a-b-c/x-y-z/x-y-z-1.ebuild'),
            ('a-b-c/x-y-z-1.2.3-r3', 'a-b-c/x-y-z/x-y-z-1.2.3-r3.ebuild'),
            ('foo/bar-3.222-r0', 'foo/bar/bar-3.222-r0.ebuild'),]

    for (cpv, verrev) in data:
      result = self._TestGetEbuildPathFromCpv(cpv)
      self.assertEqual(verrev, result)


class PortageStableTest(CpuTestBase):
  """Test Upgrader methods _SaveStatusOnStableRepo, _CheckStableRepoOnBranch"""

  STATUS_MIX = {'path1/file1': 'M',
                'path1/path2/file2': 'A',
                'a/b/.x/y~': 'D',
                'foo/bar': 'C',
                '/bar/foo': 'U',
                'unknown/file': '??',}
  STATUS_UNKNOWN = {'foo/bar': '??',
                    'a/b/c': '??',}
  STATUS_EMPTY = {}

  #
  # _CheckStableRepoOnBranch
  #

  def _TestCheckStableRepoOnBranch(self, run_result, expect_err):
    """Test Upgrader._CheckStableRepoOnBranch"""
    cmdargs = []
    mocked_upgrader = self._MockUpgrader(cmdargs=cmdargs)
    mocked_upgrader._RunGit.return_value = run_result

    # Verify.
    try:
      cpu.Upgrader._CheckStableRepoOnBranch(mocked_upgrader)
      self.assertFalse(expect_err, 'Expected RuntimeError, but none raised.')
    except RuntimeError as ex:
      self.assertTrue(expect_err, 'Unexpected RuntimeError: %s' % str(ex))

    mocked_upgrader._RunGit.assert_called_once_with(
        mocked_upgrader._stable_repo, ['branch'], redirect_stdout=True)

  def testCheckStableRepoOnBranchNoBranch(self):
    """Should fail due to 'git branch' saying 'no branch'"""
    output = '* (no branch)\n  somebranch\n  otherbranch\n'
    run_result = cros_build_lib.CommandResult(returncode=0, output=output)
    self._TestCheckStableRepoOnBranch(run_result, True)

  def testCheckStableRepoOnBranchOK1(self):
    """Should pass as 'git branch' indicates a branch"""
    output = '* somebranch\n  otherbranch\n'
    run_result = cros_build_lib.CommandResult(returncode=0, output=output)
    self._TestCheckStableRepoOnBranch(run_result, False)

  def testCheckStableRepoOnBranchOK2(self):
    """Should pass as 'git branch' indicates a branch"""
    output = '  somebranch\n* otherbranch\n'
    run_result = cros_build_lib.CommandResult(returncode=0, output=output)
    self._TestCheckStableRepoOnBranch(run_result, False)

  def testCheckStableRepoOnBranchFail(self):
    """Should fail as 'git branch' failed"""
    output = 'does not matter'
    run_result = cros_build_lib.CommandResult(returncode=1, output=output)
    self._TestCheckStableRepoOnBranch(run_result, True)

  #
  # _SaveStatusOnStableRepo
  #

  def _TestSaveStatusOnStableRepo(self, run_result):
    """Test Upgrader._SaveStatusOnStableRepo"""
    cmdargs = []
    mocked_upgrader = self._MockUpgrader(cmdargs=cmdargs)
    mocked_upgrader._RunGit.return_value = run_result

    # Verify.
    cpu.Upgrader._SaveStatusOnStableRepo(mocked_upgrader)
    mocked_upgrader._RunGit.assert_called_once_with(
        mocked_upgrader._stable_repo, ['status', '-s'], redirect_stdout=True)

    self.assertFalse(mocked_upgrader._stable_repo_stashed)
    return mocked_upgrader._stable_repo_status

  def testSaveStatusOnStableRepoFailed(self):
    """Test case where 'git status -s' fails, should raise RuntimeError"""
    run_result = cros_build_lib.CommandResult(returncode=1)

    self.assertRaises(RuntimeError,
                      self._TestSaveStatusOnStableRepo,
                      run_result)

  def testSaveStatusOnStableRepoAllKinds(self):
    """Test where 'git status -s' returns all status kinds"""
    status_lines = ['%2s %s' % (v, k) for (k, v) in self.STATUS_MIX.items()]
    status_output = '\n'.join(status_lines)
    run_result = cros_build_lib.CommandResult(returncode=0,
                                              output=status_output)
    status = self._TestSaveStatusOnStableRepo(run_result)
    self.assertEqual(status, self.STATUS_MIX)

  def testSaveStatusOnStableRepoRename(self):
    """Test where 'git status -s' shows a file rename"""
    old = 'path/foo-1'
    new = 'path/foo-2'
    status_lines = [' R %s --> %s' % (old, new)]
    status_output = '\n'.join(status_lines)
    run_result = cros_build_lib.CommandResult(returncode=0,
                                              output=status_output)
    status = self._TestSaveStatusOnStableRepo(run_result)
    self.assertEqual(status, {old: 'D', new: 'A'})

  def testSaveStatusOnStableRepoEmpty(self):
    """Test empty response from 'git status -s'"""
    run_result = cros_build_lib.CommandResult(returncode=0, output='')
    status = self._TestSaveStatusOnStableRepo(run_result)
    self.assertEqual(status, {})

  #
  # _AnyChangesStaged
  #

  def _TestAnyChangesStaged(self, status_dict):
    """Test Upgrader._AnyChangesStaged"""
    mocked_upgrader = self._MockUpgrader(_stable_repo_status=status_dict)
    return cpu.Upgrader._AnyChangesStaged(mocked_upgrader)

  def testAnyChangesStagedMix(self):
    """Should return True"""
    self.assertTrue(self._TestAnyChangesStaged(self.STATUS_MIX),
                    'Failed to notice files with changed status.')

  def testAnyChangesStagedUnknown(self):
    """Should return False, only files with '??' status"""
    self.assertFalse(self._TestAnyChangesStaged(self.STATUS_UNKNOWN),
                     'Should not consider files with "??" status.')

  def testAnyChangesStagedEmpty(self):
    """Should return False, no file statuses"""
    self.assertFalse(self._TestAnyChangesStaged(self.STATUS_EMPTY),
                     'No files should mean no changes staged.')

  #
  # _StashChanges
  #

  def testStashChanges(self):
    """Test Upgrader._StashChanges"""
    mocked_upgrader = self._MockUpgrader(cmdargs=[],
                                         _stable_repo_stashed=False)
    self.assertFalse(mocked_upgrader._stable_repo_stashed)

    # Verify.
    cpu.Upgrader._StashChanges(mocked_upgrader)
    mocked_upgrader._RunGit.assert_called_once_with(
        mocked_upgrader._stable_repo, ['stash', 'save'], redirect_stdout=True,
        combine_stdout_stderr=True)

    self.assertTrue(mocked_upgrader._stable_repo_stashed)

  #
  # _UnstashAnyChanges
  #

  def _TestUnstashAnyChanges(self, stashed):
    """Test Upgrader._UnstashAnyChanges"""
    mocked_upgrader = self._MockUpgrader(cmdargs=[],
                                         _stable_repo_stashed=stashed)
    self.assertEqual(stashed, mocked_upgrader._stable_repo_stashed)

    # Verify.
    cpu.Upgrader._UnstashAnyChanges(mocked_upgrader)

    if stashed:
      mocked_upgrader._RunGit.assert_called_once_with(
          mocked_upgrader._stable_repo, ['stash', 'pop', '--index'],
          redirect_stdout=True, combine_stdout_stderr=True)

    self.assertFalse(mocked_upgrader._stable_repo_stashed)

  def testUnstashAnyChanges(self):
    self._TestUnstashAnyChanges(True)
    self._TestUnstashAnyChanges(False)

  #
  # _DropAnyStashedChanges
  #

  def _TestDropAnyStashedChanges(self, stashed):
    """Test Upgrader._DropAnyStashedChanges"""
    mocked_upgrader = self._MockUpgrader(cmdargs=[],
                                         _stable_repo_stashed=stashed)
    self.assertEqual(stashed, mocked_upgrader._stable_repo_stashed)

    # Verify.
    cpu.Upgrader._DropAnyStashedChanges(mocked_upgrader)

    if stashed:
      mocked_upgrader._RunGit.assert_called_once_with(
          mocked_upgrader._stable_repo, ['stash', 'drop'],
          redirect_stdout=True, combine_stdout_stderr=True)

    self.assertFalse(mocked_upgrader._stable_repo_stashed)

  def testDropAnyStashedChanges(self):
    self._TestDropAnyStashedChanges(True)
    self._TestDropAnyStashedChanges(False)


class UtilityTest(CpuTestBase):
  """Test several Upgrader methods.

  Test these Upgrader methods: _SplitEBuildPath, _GenPortageEnvvars
  """

  #
  # _IsInUpgradeMode
  #

  def _TestIsInUpgradeMode(self, cmdargs):
    """Test Upgrader._IsInUpgradeMode.  Pretty simple."""
    mocked_upgrader = self._MockUpgrader(cmdargs=cmdargs)
    return cpu.Upgrader._IsInUpgradeMode(mocked_upgrader)

  def testIsInUpgradeModeNoOpts(self):
    """Should not be in upgrade mode with no options."""
    result = self._TestIsInUpgradeMode([])
    self.assertFalse(result)

  def testIsInUpgradeModeUpgrade(self):
    """Should be in upgrade mode with --upgrade."""
    result = self._TestIsInUpgradeMode(['--upgrade'])
    self.assertTrue(result)

  def testIsInUpgradeModeUpgradeDeep(self):
    """Should be in upgrade mode with --upgrade-deep."""
    result = self._TestIsInUpgradeMode(['--upgrade-deep'])
    self.assertTrue(result)

  #
  # _GetBoardCmd
  #

  def _TestGetBoardCmd(self, cmd, board):
    """Test Upgrader._GetBoardCmd."""
    mocked_upgrader = self._MockUpgrader(_curr_board=board)
    return cpu.Upgrader._GetBoardCmd(mocked_upgrader, cmd)

  def testGetBoardCmdKnownCmds(self):
    board = 'x86-alex'
    for cmd in ['emerge', 'equery', 'portageq']:
      result = self._TestGetBoardCmd(cmd, cpu.Upgrader.HOST_BOARD)
      self.assertEqual(result, cmd)
      result = self._TestGetBoardCmd(cmd, board)
      self.assertEqual(result, '%s-%s' % (cmd, board))

  def testGetBoardCmdUnknownCmd(self):
    board = 'x86-alex'
    cmd = 'foo'
    result = self._TestGetBoardCmd(cmd, cpu.Upgrader.HOST_BOARD)
    self.assertEqual(result, cmd)
    result = self._TestGetBoardCmd(cmd, board)
    self.assertEqual(result, cmd)

  #
  # _GenPortageEnvvars
  #

  def _TestGenPortageEnvvars(self, arch, unstable_ok,
                             portdir=None, portage_configroot=None):
    """Testing the behavior of the Upgrader._GenPortageEnvvars method."""
    mocked_upgrader = self._MockUpgrader()

    # Verify.
    result = cpu.Upgrader._GenPortageEnvvars(mocked_upgrader,
                                             arch, unstable_ok,
                                             portdir, portage_configroot)

    keyw = arch
    if unstable_ok:
      keyw = arch + ' ~' + arch

    self.assertEqual(result['ACCEPT_KEYWORDS'], keyw)
    if portdir is None:
      self.assertFalse('PORTDIR' in result)
    else:
      self.assertEqual(result['PORTDIR'], portdir)
    if portage_configroot is None:
      self.assertFalse('PORTAGE_CONFIGROOT' in result)
    else:
      self.assertEqual(result['PORTAGE_CONFIGROOT'], portage_configroot)

  def testGenPortageEnvvars1(self):
    self._TestGenPortageEnvvars('arm', False)

  def testGenPortageEnvvars2(self):
    self._TestGenPortageEnvvars('x86', True)

  def testGenPortageEnvvars3(self):
    self._TestGenPortageEnvvars('x86', True,
                                portdir='/foo/bar',
                                portage_configroot='/bar/foo')

  #
  # _SplitEBuildPath
  #

  def _TestSplitEBuildPath(self, ebuild_path, golden_result):
    """Test the behavior of the Upgrader._SplitEBuildPath method."""
    mocked_upgrader = self._MockUpgrader()

    # Verify.
    result = cpu.Upgrader._SplitEBuildPath(mocked_upgrader,
                                           ebuild_path)
    self.assertEqual(result, golden_result)

  def testSplitEBuildPath1(self):
    self._TestSplitEBuildPath('/foo/bar/portage/dev-libs/A/A-2.ebuild',
                              ('portage', 'dev-libs', 'A', 'A-2'))

  def testSplitEBuildPath2(self):
    self._TestSplitEBuildPath('/foo/ooo/ccc/ppp/ppp-1.2.3-r123.ebuild',
                              ('ooo', 'ccc', 'ppp', 'ppp-1.2.3-r123'))


@unittest.skip('playground setup needs more work')
class TreeInspectTest(CpuTestBase):
  """Test Upgrader methods: _FindCurrentCPV, _FindUpstreamCPV"""

  def _GenerateTestInput(self, category, pkg_name, ver_rev):
    """Return tuple (ebuild_path, cpv, cp)."""
    ebuild_path = None
    cpv = None
    if ver_rev:
      ebuild_path = '%s/%s/%s/%s-%s.ebuild' % (self.portage_stable,
                                               category, pkg_name,
                                               pkg_name, ver_rev)
      cpv = '%s/%s-%s' % (category, pkg_name, ver_rev)
    cp = '%s/%s' % (category, pkg_name)
    return (ebuild_path, cpv, cp)

  #
  # _FindUpstreamCPV
  #

  def _TestFindUpstreamCPV(self, pkg_arg, ebuild_expect, unstable_ok=False):
    """Test Upgrader._FindUpstreamCPV

    This points _FindUpstreamCPV at the ResolverPlayground as if it is
    the upstream tree.
    """
    self._SetUpPlayground()
    mocked_upgrader = self._MockUpgrader(_curr_board=None)

    # Replay script.
    envvars = cpu.Upgrader._GenPortageEnvvars(mocked_upgrader,
                                              mocked_upgrader._curr_arch,
                                              unstable_ok,
                                              portdir=self.portage_stable,
                                              portage_configroot=self.eroot)
    portage_configroot = mocked_upgrader._emptydir
    mocked_upgrader._GenPortageEnvvars.return_value = envvars

    if ebuild_expect:
      ebuild_path = self.eroot + ebuild_expect
      split_path = cpu.Upgrader._SplitEBuildPath(mocked_upgrader, ebuild_path)
      mocked_upgrader._SplitEBuildPath.return_value = split_path

    # Verify.
    result = cpu.Upgrader._FindUpstreamCPV(mocked_upgrader, pkg_arg,
                                           unstable_ok)

    mocked_upgrader._GenPortageEnvvars.assert_called_once_with(
        mocked_upgrader._curr_arch, unstable_ok,
        portdir=mocked_upgrader._upstream,
        portage_configroot=portage_configroot)

    if ebuild_expect:
      mocked_upgrader._SplitEBuildPath.assert_called_once_with(ebuild_path)

    self.assertEqual(bool(ebuild_expect), bool(result))

    return result

  def testFindUpstreamA2(self):
    (ebuild, cpv, cp) = self._GenerateTestInput(category='dev-libs',
                                                pkg_name='A',
                                                ver_rev='2')
    result = self._TestFindUpstreamCPV(cp, ebuild)
    self.assertEqual(result, cpv)

  def testFindUpstreamAAA(self):
    (ebuild, cpv, cp) = self._GenerateTestInput(category='dev-apps',
                                                pkg_name='AAA',
                                                ver_rev=None)
    result = self._TestFindUpstreamCPV(cp, ebuild)
    self.assertEqual(result, cpv)

  def testFindUpstreamF(self):
    (ebuild, cpv, cp) = self._GenerateTestInput(category='dev-libs',
                                                pkg_name='F',
                                                ver_rev='2')
    result = self._TestFindUpstreamCPV(cp, ebuild)
    self.assertEqual(result, cpv)

  def testFindUpstreamFlimflam(self):
    """Should find 0.0.1-r228 because more recent flimflam unstable."""
    (ebuild, cpv, cp) = self._GenerateTestInput(category='chromeos-base',
                                                pkg_name='flimflam',
                                                ver_rev='0.0.1-r228')
    result = self._TestFindUpstreamCPV(cp, ebuild)
    self.assertEqual(result, cpv)

  def testFindUpstreamFlimflamUnstable(self):
    """Should find 0.0.2-r123 because of unstable_ok."""
    (ebuild, cpv, cp) = self._GenerateTestInput(category='chromeos-base',
                                                pkg_name='flimflam',
                                                ver_rev='0.0.2-r123')
    result = self._TestFindUpstreamCPV(cp, ebuild, unstable_ok=True)
    self.assertEqual(result, cpv)

  #
  # _FindCurrentCPV
  #

  def _TestFindCurrentCPV(self, pkg_arg, ebuild_expect):
    """Test Upgrader._FindCurrentCPV

    This test points Upgrader._FindCurrentCPV to the ResolverPlayground
    tree as if it is the local source.
    """
    mocked_upgrader = self._MockUpgrader(_curr_board=None)
    self._SetUpPlayground()

    # Replay script.
    envvars = cpu.Upgrader._GenPortageEnvvars(mocked_upgrader,
                                              mocked_upgrader._curr_arch,
                                              unstable_ok=False)
    mocked_upgrader._GenPortageEnvvars.return_value = envvars
    mocked_upgrader._GetBoardCmd.return_value = 'equery'

    if ebuild_expect:
      ebuild_path = self.eroot + ebuild_expect
      split_path = cpu.Upgrader._SplitEBuildPath(mocked_upgrader, ebuild_path)
      mocked_upgrader._SplitEBuildPath.return_value = split_path

    # Verify.
    result = cpu.Upgrader._FindCurrentCPV(mocked_upgrader, pkg_arg)

    mocked_upgrader._GenPortageEnvvars.assert_called_once_with(
        mocked_upgrader._curr_arch, unstable_ok=False)

    if ebuild_expect:
      mocked_upgrader._SplitEBuildPath.assert_called_once_with(ebuild_path)

    return result

  def testFindCurrentA(self):
    """Should find dev-libs/A-2."""
    (ebuild, cpv, cp) = self._GenerateTestInput(category='dev-libs',
                                                pkg_name='A',
                                                ver_rev='2')
    result = self._TestFindCurrentCPV(cp, ebuild)
    self.assertEqual(result, cpv)

  def testFindCurrentAAA(self):
    """Should find None, because dev-libs/AAA does not exist in tree."""
    (ebuild, cpv, cp) = self._GenerateTestInput(category='dev-libs',
                                                pkg_name='AAA',
                                                ver_rev=None)
    result = self._TestFindCurrentCPV(cp, ebuild)
    self.assertEqual(result, cpv)

  def testFindCurrentF(self):
    """Should find dev-libs/F-2."""
    (ebuild, cpv, cp) = self._GenerateTestInput(category='dev-libs',
                                                pkg_name='F',
                                                ver_rev='2')
    result = self._TestFindCurrentCPV(cp, ebuild)
    self.assertEqual(result, cpv)

  def testFindCurrentFlimflam(self):
    """Should find 0.0.1-r228 because more recent flimflam unstable."""
    (ebuild, cpv, cp) = self._GenerateTestInput(category='chromeos-base',
                                                pkg_name='flimflam',
                                                ver_rev='0.0.1-r228')
    result = self._TestFindCurrentCPV(cp, ebuild)
    self.assertEqual(result, cpv)


class RunBoardTest(CpuTestBase):
  """Test Upgrader.RunBoard,PrepareToRun,RunCompleted."""

  def testRunCompletedSpecified(self):
    cmdargs = ['--upstream=/some/dir']
    mocked_upgrader = self._MockUpgrader(cmdargs=cmdargs, _curr_board=None)
    with self.OutputCapturer():
      cpu.Upgrader.RunCompleted(mocked_upgrader)

  def testRunCompletedRemoveCache(self):
    # TODO: Create cache and check it's cleaned up.
    cmdargs = ['--no-upstream-cache']
    mocked_upgrader = self._MockUpgrader(cmdargs=cmdargs, _curr_board=None)
    with self.OutputCapturer():
      cpu.Upgrader.RunCompleted(mocked_upgrader)

  def testRunCompletedKeepCache(self):
    # TODO: Create cache and check it's left behind.
    cmdargs = []
    mocked_upgrader = self._MockUpgrader(cmdargs=cmdargs, _curr_board=None)
    with self.OutputCapturer():
      cpu.Upgrader.RunCompleted(mocked_upgrader)

  def testPrepareToRunUpstreamRepoExists(self):
    osutils.Touch(os.path.join(self.upstream_tmp_repo, '.git', 'shallow'),
                  makedirs=True)

    cmdargs = []
    mocked_upgrader = self._MockUpgrader(cmdargs=cmdargs,
                                         _curr_board=None)

    # Verify.
    with self.OutputCapturer():
      cpu.Upgrader.PrepareToRun(mocked_upgrader)

    mocked_upgrader._RunGit.assert_has_calls([
        mock.call(self.upstream_tmp_repo,
                  ['remote', 'set-url', 'origin',
                   cpu.Upgrader.PORTAGE_GIT_URL]),
        mock.call(self.upstream_tmp_repo,
                  ['config', 'remote.origin.fetch',
                   '+refs/heads/master:refs/remotes/origin/master']),
        mock.call(self.upstream_tmp_repo, ['remote', 'update']),
        mock.call(self.upstream_tmp_repo, ['checkout', '-f', 'origin/master'],
                  combine_stdout_stderr=True, redirect_stdout=True),
    ])

  def testPrepareToRunUpstreamRepoNew(self):
    cmdargs = []
    mocked_upgrader = self._MockUpgrader(cmdargs=cmdargs,
                                         _curr_board=None)

    # Verify.
    with self.OutputCapturer():
      cpu.Upgrader.PrepareToRun(mocked_upgrader)

    self.assertExists(self.upstream_tmp_repo + '-README')
    mocked_upgrader._RunGit.assert_called_once_with(
        self.tempdir, ['clone', '--branch', 'master', '--depth', '1',
                       cpu.Upgrader.PORTAGE_GIT_URL,
                       os.path.basename(self.upstream_tmp_repo)])

  def _TestRunBoard(self, pinfolist, upgrade=False, staged_changes=False):
    """Test Upgrader.RunBoard."""
    targetlist = [pinfo.user_arg for pinfo in pinfolist]
    upstream_only_pinfolist = [pinfo for pinfo in pinfolist if not pinfo.cpv]

    cmdargs = targetlist
    if upgrade:
      cmdargs = ['--upgrade'] + cmdargs
    mocked_upgrader = self._MockUpgrader(cmdargs=cmdargs,
                                         _curr_board=None)
    board = 'runboard_testboard'

    self.PatchObject(cpu.Upgrader, '_FindBoardArch', return_value=DEFAULT_ARCH)
    upgrade_mode = cpu.Upgrader._IsInUpgradeMode(mocked_upgrader)
    mocked_upgrader._IsInUpgradeMode.return_value = upgrade_mode
    mocked_upgrader._AnyChangesStaged.return_value = staged_changes
    if staged_changes:
      mocked_upgrader._StashChanges()
    mocked_upgrader._ResolveAndVerifyArgs.return_value = pinfolist

    if upgrade:
      mocked_upgrader._FinalizeUpstreamPInfolist.return_value = []
    else:
      mocked_upgrader._GetCurrentVersions.return_value = pinfolist
      mocked_upgrader._FinalizeLocalPInfolist.return_value = []

      if upgrade_mode:
        mocked_upgrader._FinalizeUpstreamPInfolist.return_value = []

    # Verify.
    with self.OutputCapturer():
      cpu.Upgrader.RunBoard(mocked_upgrader, board)

    mocked_upgrader._ResolveAndVerifyArgs.assert_called_once_with(
        targetlist, upgrade_mode)
    if not upgrade and upgrade_mode:
      mocked_upgrader._FinalizeUpstreamPInfolist.assert_called_once_with(
          upstream_only_pinfolist)

  def testRunBoard1(self):
    target_pinfolist = [cpu.PInfo(user_arg='dev-libs/A',
                                  cpv='dev-libs/A-1',
                                  upstream_cpv='dev-libs/A-2')]
    return self._TestRunBoard(target_pinfolist)

  def testRunBoard2(self):
    target_pinfolist = [cpu.PInfo(user_arg='dev-libs/A',
                                  cpv='dev-libs/A-1',
                                  upstream_cpv='dev-libs/A-2')]
    return self._TestRunBoard(target_pinfolist, upgrade=True)

  def testRunBoard3(self):
    target_pinfolist = [cpu.PInfo(user_arg='dev-libs/A',
                                  cpv='dev-libs/A-1',
                                  upstream_cpv='dev-libs/A-2')]
    return self._TestRunBoard(target_pinfolist, upgrade=True,
                              staged_changes=True)

  def testRunBoardUpstreamOnlyStatusMode(self):
    """Status mode with package that is only upstream should error."""
    pinfolist = [cpu.PInfo(user_arg='dev-libs/M',
                           cpv=None,
                           upstream_cpv='dev-libs/M-2'),]

    targetlist = [pinfo.user_arg for pinfo in pinfolist]

    mocked_upgrader = self._MockUpgrader(cmdargs=['dev-libs/M'],
                                         _curr_board=None)
    board = 'runboard_testboard'

    self.PatchObject(cpu.Upgrader, '_FindBoardArch', return_value=DEFAULT_ARCH)
    upgrade_mode = cpu.Upgrader._IsInUpgradeMode(mocked_upgrader)
    mocked_upgrader._IsInUpgradeMode.return_value = upgrade_mode
    mocked_upgrader._AnyChangesStaged.return_value = False
    mocked_upgrader._ResolveAndVerifyArgs.return_value = pinfolist

    # Verify.
    with self.OutputCapturer():
      self.assertRaises(RuntimeError,
                        cpu.Upgrader.RunBoard,
                        mocked_upgrader, board)

    mocked_upgrader._ResolveAndVerifyArgs.assert_called_once_with(
        targetlist, upgrade_mode)


class GiveEmergeResultsTest(CpuTestBase):
  """Test Upgrader._GiveEmergeResults"""

  def _TestGiveEmergeResultsOK(self, pinfolist, ok, error=None):
    cmdargs = []
    mocked_upgrader = self._MockUpgrader(cmdargs=cmdargs)

    # Replay script.
    mocked_upgrader._AreEmergeable.return_value = (ok, None, None)

    # Verify.
    result = None
    with self.OutputCapturer():
      if error:
        self.assertRaises(error, cpu.Upgrader._GiveEmergeResults,
                          mocked_upgrader, pinfolist)
      else:
        result = cpu.Upgrader._GiveEmergeResults(mocked_upgrader, pinfolist)

    return result

  def testGiveEmergeResultsUnmaskedOK(self):
    pinfolist = [cpu.PInfo(upgraded_cpv='abc/def-4', upgraded_unmasked=True),
                 cpu.PInfo(upgraded_cpv='bcd/efg-8', upgraded_unmasked=True)]
    self._TestGiveEmergeResultsOK(pinfolist, True)

  def testGiveEmergeResultsUnmaskedNotOK(self):
    pinfolist = [cpu.PInfo(upgraded_cpv='abc/def-4', upgraded_unmasked=True),
                 cpu.PInfo(upgraded_cpv='bcd/efg-8', upgraded_unmasked=True)]
    self._TestGiveEmergeResultsOK(pinfolist, False, error=RuntimeError)

  def _TestGiveEmergeResultsMasked(self, pinfolist, ok, masked_cpvs,
                                   error=None):
    cmdargs = []
    mocked_upgrader = self._MockUpgrader(cmdargs=cmdargs)

    # Replay script.
    emergeable_tuple = (ok, 'some-cmd', 'some-output')
    mocked_upgrader._AreEmergeable.return_value = emergeable_tuple

    # Verify.
    result = None
    with self.OutputCapturer():
      if error:
        self.assertRaises(error, cpu.Upgrader._GiveEmergeResults,
                          mocked_upgrader, pinfolist)
      else:
        result = cpu.Upgrader._GiveEmergeResults(mocked_upgrader, pinfolist)

    if not ok:
      self.assertEqual(sorted(mocked_upgrader._GiveMaskedError.call_args_list),
                       sorted(mock.call(x, 'some-output') for x in masked_cpvs))

    return result

  def testGiveEmergeResultsMaskedOK(self):
    pinfolist = [cpu.PInfo(upgraded_cpv='abc/def-4', upgraded_unmasked=False),
                 cpu.PInfo(upgraded_cpv='bcd/efg-8', upgraded_unmasked=False)]
    masked_cpvs = ['abc/def-4', 'bcd/efg-8']
    self._TestGiveEmergeResultsMasked(pinfolist, True, masked_cpvs,
                                      error=RuntimeError)

  def testGiveEmergeResultsMaskedNotOK(self):
    pinfolist = [cpu.PInfo(upgraded_cpv='abc/def-4', upgraded_unmasked=False),
                 cpu.PInfo(upgraded_cpv='bcd/efg-8', upgraded_unmasked=False)]
    masked_cpvs = ['abc/def-4', 'bcd/efg-8']
    self._TestGiveEmergeResultsMasked(pinfolist, False, masked_cpvs,
                                      error=RuntimeError)


class CheckStagedUpgradesTest(CpuTestBase):
  """Test Upgrader._CheckStagedUpgrades"""

  def testCheckStagedUpgradesTwoStaged(self):
    cmdargs = []

    ebuild1 = 'a/b/foo/bar/bar-1.ebuild'
    ebuild2 = 'x/y/bar/foo/foo-3.ebuild'
    repo_status = {ebuild1: 'A',
                   'a/b/foo/garbage': 'A',
                   ebuild2: 'A',}

    pinfolist = [cpu.PInfo(package='foo/bar'),
                 cpu.PInfo(package='bar/foo')]

    mocked_upgrader = self._MockUpgrader(cmdargs=cmdargs,
                                         _stable_repo_status=repo_status)

    def SplitPath(ebuild):
      return cpu.Upgrader._SplitEBuildPath(mocked_upgrader, ebuild)
    mocked_upgrader._SplitEBuildPath.side_effect = SplitPath

    # Verify.
    cpu.Upgrader._CheckStagedUpgrades(mocked_upgrader, pinfolist)

  def testCheckStagedUpgradesTwoStagedOneUnexpected(self):
    cmdargs = []

    ebuild1 = 'a/b/foo/bar/bar-1.ebuild'
    ebuild2 = 'x/y/bar/foo/foo-3.ebuild'
    repo_status = {ebuild1: 'A',
                   'a/b/foo/garbage': 'A',
                   ebuild2: 'A',}

    # Without foo/bar in the pinfolist it should complain about that
    # package having staged changes.
    pinfolist = [cpu.PInfo(package='bar/foo')]

    mocked_upgrader = self._MockUpgrader(cmdargs=cmdargs,
                                         _stable_repo_status=repo_status)

    def SplitPath(ebuild):
      return cpu.Upgrader._SplitEBuildPath(mocked_upgrader, ebuild)
    mocked_upgrader._SplitEBuildPath.side_effect = SplitPath

    # Verify.
    self.assertRaises(RuntimeError, cpu.Upgrader._CheckStagedUpgrades,
                      mocked_upgrader, pinfolist)

  def testCheckStagedUpgradesNoneStaged(self):
    cmdargs = []

    pinfolist = [cpu.PInfo(package='foo/bar-1'),
                 cpu.PInfo(package='bar/foo-3')]

    mocked_upgrader = self._MockUpgrader(cmdargs=cmdargs,
                                         _stable_repo_status=None)

    # Verify.
    cpu.Upgrader._CheckStagedUpgrades(mocked_upgrader, pinfolist)


class UpgradePackagesTest(CpuTestBase):
  """Test Upgrader._UpgradePackages"""

  def _TestUpgradePackages(self, pinfolist, upgrade):
    cmdargs = []
    if upgrade:
      cmdargs.append('--upgrade')
    table = utable.UpgradeTable('some-arch')
    mocked_upgrader = self._MockUpgrader(cmdargs=cmdargs,
                                         _curr_table=table)

    # Replay script.
    upgrades_this_run = False
    calls_pinfo = []
    calls_VerifyPackageUpgrade = []
    side_effect_UpgradePackage = []
    for pinfo in pinfolist:
      pkg_result = bool(pinfo.upgraded_cpv)
      calls_pinfo.append(mock.call(pinfo))
      side_effect_UpgradePackage.append(pkg_result)
      if pinfo.upgraded_cpv:
        calls_VerifyPackageUpgrade.append(mock.call(pinfo))
      if pkg_result:
        upgrades_this_run = True
    mocked_upgrader._UpgradePackage.side_effect = side_effect_UpgradePackage

    upgrade_mode = cpu.Upgrader._IsInUpgradeMode(mocked_upgrader)
    mocked_upgrader._IsInUpgradeMode.return_value = upgrade_mode

    # Verify.
    cpu.Upgrader._UpgradePackages(mocked_upgrader, pinfolist)

    if upgrades_this_run:
      self.assertEqual(1, mocked_upgrader._GiveEmergeResults.call_count)

    mocked_upgrader._UpgradePackage.assert_has_calls(calls_pinfo)
    mocked_upgrader._VerifyPackageUpgrade.assert_has_calls(
        calls_VerifyPackageUpgrade)
    mocked_upgrader._PackageReport.assert_has_calls(calls_pinfo)

  def testUpgradePackagesUpgradeModeWithUpgrades(self):
    pinfolist = [cpu.PInfo(upgraded_cpv='abc/def-4'),
                 cpu.PInfo(upgraded_cpv='bcd/efg-8'),
                 cpu.PInfo(upgraded_cpv=None),
                 cpu.PInfo(upgraded_cpv=None)]
    self._TestUpgradePackages(pinfolist, True)

  def testUpgradePackagesUpgradeModeNoUpgrades(self):
    pinfolist = [cpu.PInfo(upgraded_cpv=None),
                 cpu.PInfo(upgraded_cpv=None)]
    self._TestUpgradePackages(pinfolist, True)

  def testUpgradePackagesStatusModeNoUpgrades(self):
    pinfolist = [cpu.PInfo(upgraded_cpv=None),
                 cpu.PInfo(upgraded_cpv=None)]
    self._TestUpgradePackages(pinfolist, False)


class CategoriesRoundtripTest(cros_test_lib.MockTempDirTestCase,
                              cros_test_lib.OutputTestCase):
  """Tests for full "round trip" runs."""

  def _TestCategoriesRoundtrip(self, categories):
    stable_repo = self.tempdir
    cat_file = cpu.Upgrader.CATEGORIES_FILE
    profiles_dir = os.path.join(stable_repo, os.path.dirname(cat_file))

    git_mock = self.PatchObject(cpu.Upgrader, '_RunGit')

    options = cros_test_lib.EasyAttr(srcroot='foobar', upstream=None,
                                     packages='')
    upgrader = cpu.Upgrader(options=options)
    upgrader._stable_repo = stable_repo
    os.makedirs(profiles_dir)

    # Verification phase.  Write then load categories.
    upgrader._stable_repo_categories = set(categories)
    upgrader._WriteStableRepoCategories()
    upgrader._stable_repo_categories = None
    upgrader._LoadStableRepoCategories()
    git_mock.assert_called_once_with(stable_repo, ['add', cat_file])
    self.assertEqual(sorted(categories),
                     sorted(upgrader._stable_repo_categories))

  def test1(self):
    categories = ['alpha-omega', 'omega-beta', 'beta-chi']
    self._TestCategoriesRoundtrip(categories)

  def test2(self):
    categories = []
    self._TestCategoriesRoundtrip(categories)

  def test3(self):
    categories = ['virtual', 'happy-days', 'virtually-there']
    self._TestCategoriesRoundtrip(categories)


class UpgradePackageTest(CpuTestBase):
  """Test Upgrader._UpgradePackage"""

  def _TestUpgradePackage(self, pinfo, upstream_cpv, upstream_cmp,
                          stable_up, latest_up,
                          upgrade_requested, upgrade_staged,
                          unstable_ok, force):
    cmdargs = []
    if unstable_ok:
      cmdargs.append('--unstable-ok')
    if force:
      cmdargs.append('--force')
    mocked_upgrader = self._MockUpgrader(cmdargs=cmdargs)

    # Add test-specific mocks/stubs.
    run_result = cros_build_lib.CommandResult(returncode=0)
    run_mock = self.PatchObject(cros_build_lib, 'RunCommand',
                                return_value=run_result)
    run_calls = []

    # Replay script.
    def FindUpstreamCPV(pkg, unstable_ok=False):
      self.assertEqual(pinfo.package, pkg)
      return latest_up if unstable_ok else stable_up
    mocked_upgrader._FindUpstreamCPV.side_effect = FindUpstreamCPV

    git_calls = []
    if upstream_cpv:
      mocked_upgrader._PkgUpgradeRequested.return_value = upgrade_requested

      if upgrade_requested:
        mocked_upgrader._PkgUpgradeStaged.return_value = upgrade_staged
        if (not upgrade_staged and
            (upstream_cmp > 0 or (upstream_cmp == 0 and force))):
          mocked_upgrader._CopyUpstreamPackage.return_value = upstream_cpv
          upgrade_staged = True

        if upgrade_staged:
          mocked_upgrader._SetUpgradedMaskBits(pinfo)
          ebuild_path = cpu.Upgrader._GetEbuildPathFromCpv(upstream_cpv)
          ebuild_path = os.path.join(mocked_upgrader._stable_repo,
                                     ebuild_path)
          cache_files = 'metadata/md5-cache/%s-[0-9]*' % pinfo.package
          git_calls += [
              mock.call(mocked_upgrader._stable_repo, ['add', pinfo.package]),
              mock.call(mocked_upgrader._stable_repo,
                        ['rm', '--ignore-unmatch', '-q', '-f', cache_files]),
              mock.call(mocked_upgrader._stable_repo, ['add', cache_files]),
          ]
          cmd = ['egencache', '--update', '--repo=portage-stable',
                 pinfo.package]
          run_calls.append(mock.call(cmd, print_cmd=False, redirect_stdout=True,
                                     combine_stdout_stderr=True))

    # Verify.
    with self.OutputCapturer():
      result = cpu.Upgrader._UpgradePackage(mocked_upgrader, pinfo)

    run_mock.assert_has_calls(run_calls)
    mocked_upgrader._RunGit.assert_has_calls(git_calls)

    if upstream_cpv:
      mocked_upgrader._PkgUpgradeRequested.assert_called_once_with(pinfo)
      self.assertEqual(upstream_cpv, pinfo.upstream_cpv)

      if upgrade_requested:
        mocked_upgrader._PkgUpgradeStaged.assert_called_once_with(upstream_cpv)

      if upgrade_requested and (upstream_cpv != pinfo.cpv or force):
        self.assertEqual(upstream_cpv, pinfo.upgraded_cpv)
      else:
        self.assertIsNone(pinfo.upgraded_cpv)
    else:
      self.assertIsNone(pinfo.upstream_cpv)
      self.assertIsNone(pinfo.upgraded_cpv)
    self.assertEqual(stable_up, pinfo.stable_upstream_cpv)
    self.assertEqual(latest_up, pinfo.latest_upstream_cpv)

    return result

  # Dimensions to vary:
  # 1) Upgrade for this package requested or not.
  # 2) Upgrade can be stable or not.
  # 3) Specific version to upgrade to specified.
  # 4) Upgrade already staged or not.
  # 5) Upgrade needed or not (current).

  def testUpgradePackageOutdatedRequestedStable(self):
    pinfo = cpu.PInfo(cpv='foo/bar-2',
                      package='foo/bar',
                      upstream_cpv=None)
    result = self._TestUpgradePackage(pinfo,
                                      upstream_cpv='foo/bar-3',
                                      upstream_cmp=1, # outdated
                                      stable_up='foo/bar-3',
                                      latest_up='foo/bar-5',
                                      upgrade_requested=True,
                                      upgrade_staged=False,
                                      unstable_ok=False,
                                      force=False)
    self.assertTrue(result)

  def testUpgradePackageOutdatedRequestedUnstable(self):
    pinfo = cpu.PInfo(cpv='foo/bar-2',
                      package='foo/bar',
                      upstream_cpv=None)
    result = self._TestUpgradePackage(pinfo,
                                      upstream_cpv='foo/bar-5',
                                      upstream_cmp=1, # outdated
                                      stable_up='foo/bar-3',
                                      latest_up='foo/bar-5',
                                      upgrade_requested=True,
                                      upgrade_staged=False,
                                      unstable_ok=True,
                                      force=False)
    self.assertTrue(result)

  def testUpgradePackageOutdatedRequestedStableSpecified(self):
    pinfo = cpu.PInfo(cpv='foo/bar-2',
                      package='foo/bar',
                      upstream_cpv='foo/bar-4')
    result = self._TestUpgradePackage(pinfo,
                                      upstream_cpv='foo/bar-4',
                                      upstream_cmp=1, # outdated
                                      stable_up='foo/bar-3',
                                      latest_up='foo/bar-5',
                                      upgrade_requested=True,
                                      upgrade_staged=False,
                                      unstable_ok=False, # not important
                                      force=False)
    self.assertTrue(result)

  def testUpgradePackageCurrentRequestedStable(self):
    pinfo = cpu.PInfo(cpv='foo/bar-3',
                      package='foo/bar',
                      upstream_cpv=None)
    result = self._TestUpgradePackage(pinfo,
                                      upstream_cpv='foo/bar-3',
                                      upstream_cmp=0, # current
                                      stable_up='foo/bar-3',
                                      latest_up='foo/bar-5',
                                      upgrade_requested=True,
                                      upgrade_staged=False,
                                      unstable_ok=False,
                                      force=False)
    self.assertFalse(result)

  def testUpgradePackageCurrentRequestedStableForce(self):
    pinfo = cpu.PInfo(cpv='foo/bar-3',
                      package='foo/bar',
                      upstream_cpv='foo/bar-3')
    result = self._TestUpgradePackage(pinfo,
                                      upstream_cpv='foo/bar-3',
                                      upstream_cmp=0, # current
                                      stable_up='foo/bar-3',
                                      latest_up='foo/bar-5',
                                      upgrade_requested=True,
                                      upgrade_staged=False,
                                      unstable_ok=False,
                                      force=True)
    self.assertTrue(result)

  def testUpgradePackageOutdatedStable(self):
    pinfo = cpu.PInfo(cpv='foo/bar-2',
                      package='foo/bar',
                      upstream_cpv=None)
    result = self._TestUpgradePackage(pinfo,
                                      upstream_cpv='foo/bar-3',
                                      upstream_cmp=1, # outdated
                                      stable_up='foo/bar-3',
                                      latest_up='foo/bar-5',
                                      upgrade_requested=False,
                                      upgrade_staged=False,
                                      unstable_ok=False,
                                      force=False)
    self.assertFalse(result)

  def testUpgradePackageOutdatedRequestedStableStaged(self):
    pinfo = cpu.PInfo(cpv='foo/bar-2',
                      package='foo/bar',
                      upstream_cpv=None)
    result = self._TestUpgradePackage(pinfo,
                                      upstream_cpv='foo/bar-3',
                                      upstream_cmp=1, # outdated
                                      stable_up='foo/bar-3',
                                      latest_up='foo/bar-5',
                                      upgrade_requested=True,
                                      upgrade_staged=True,
                                      unstable_ok=False,
                                      force=False)
    self.assertTrue(result)

  def testUpgradePackageOutdatedRequestedUnstableStaged(self):
    pinfo = cpu.PInfo(cpv='foo/bar-2',
                      package='foo/bar',
                      upstream_cpv='foo/bar-5')
    result = self._TestUpgradePackage(pinfo,
                                      upstream_cpv='foo/bar-5',
                                      upstream_cmp=1, # outdated
                                      stable_up='foo/bar-3',
                                      latest_up='foo/bar-5',
                                      upgrade_requested=True,
                                      upgrade_staged=True,
                                      unstable_ok=True,
                                      force=False)
    self.assertTrue(result)


class VerifyPackageTest(CpuTestBase):
  """Tests for _VerifyPackageUpgrade()."""

  def _TestVerifyPackageUpgrade(self, pinfo):
    cmdargs = []
    mocked_upgrader = self._MockUpgrader(cmdargs=cmdargs)
    was_overwrite = pinfo.cpv_cmp_upstream == 0

    # Verify.
    cpu.Upgrader._VerifyPackageUpgrade(mocked_upgrader, pinfo)

    mocked_upgrader._VerifyEbuildOverlay.assert_called_once_with(
        pinfo.upgraded_cpv, 'portage-stable', was_overwrite)

  def testVerifyPackageUpgrade(self):
    pinfo = cpu.PInfo(upgraded_cpv='foo/bar-3')

    for cpv_cmp_upstream in (0, 1):
      pinfo.cpv_cmp_upstream = cpv_cmp_upstream
      self._TestVerifyPackageUpgrade(pinfo)

  def _TestVerifyEbuildOverlay(self, cpv, overlay, ebuild_path, was_overwrite):
    """Test Upgrader._VerifyEbuildOverlay"""
    cmdargs = []
    mocked_upgrader = self._MockUpgrader(cmdargs=cmdargs)

    run_mock = self.PatchObject(cros_build_lib, 'RunCommand')

    # Replay script.
    envvars = cpu.Upgrader._GenPortageEnvvars(mocked_upgrader,
                                              mocked_upgrader._curr_arch,
                                              unstable_ok=False)
    mocked_upgrader._GenPortageEnvvars.return_value = envvars
    mocked_upgrader._GetBoardCmd.return_value = 'equery'
    run_result = cros_build_lib.CommandResult(returncode=0, output=ebuild_path)
    run_mock.return_value = run_result
    split_ebuild = cpu.Upgrader._SplitEBuildPath(mocked_upgrader, ebuild_path)
    mocked_upgrader._SplitEBuildPath.return_value = split_ebuild

    # Verify.
    cpu.Upgrader._VerifyEbuildOverlay(mocked_upgrader, cpv,
                                      overlay, was_overwrite)

    mocked_upgrader._GenPortageEnvvars.assert_called_once_with(
        mocked_upgrader._curr_arch, unstable_ok=False)
    run_mock.assert_called_once_with(
        ['equery', '-C', 'which', '--include-masked', cpv], error_code_ok=True,
        extra_env=envvars, print_cmd=False, redirect_stdout=True,
        combine_stdout_stderr=True)

  def testVerifyEbuildOverlayGood(self):
    cpv = 'foo/bar-2'
    overlay = 'some-overlay'
    good_path = '/some/path/%s/foo/bar/bar-2.ebuild' % overlay

    self._TestVerifyEbuildOverlay(cpv, overlay, good_path, False)

  def testVerifyEbuildOverlayEvilNonOverwrite(self):
    cpv = 'foo/bar-2'
    overlay = 'some-overlay'
    evil_path = '/some/path/spam/foo/bar/bar-2.ebuild'

    self.assertRaises(RuntimeError,
                      self._TestVerifyEbuildOverlay,
                      cpv, overlay, evil_path, False)

  def testVerifyEbuildOverlayEvilOverwrite(self):
    cpv = 'foo/bar-2'
    overlay = 'some-overlay'
    evil_path = '/some/path/spam/foo/bar/bar-2.ebuild'

    self.assertRaises(RuntimeError,
                      self._TestVerifyEbuildOverlay,
                      cpv, overlay, evil_path, True)

  def _TestSetUpgradedMaskBits(self, pinfo, output):
    cpv = pinfo.upgraded_cpv
    cmdargs = []
    mocked_upgrader = self._MockUpgrader(cmdargs=cmdargs)

    run_mock = self.PatchObject(cros_build_lib, 'RunCommand')

    # Replay script.
    mocked_upgrader._GenPortageEnvvars.return_value = 'envvars'
    mocked_upgrader._GetBoardCmd.return_value = 'equery'
    run_result = cros_build_lib.CommandResult(returncode=0, output=output)
    run_mock.return_value = run_result

    # Verify.
    cpu.Upgrader._SetUpgradedMaskBits(mocked_upgrader, pinfo)

    mocked_upgrader._GenPortageEnvvars.assert_called_once_with(
        mocked_upgrader._curr_arch, unstable_ok=False)
    run_mock.assert_called_once_with(
        ['equery', '-qCN', 'list', '-F', '$mask|$cpv:$slot', '-op', cpv],
        error_code_ok=True, extra_env='envvars', print_cmd=False,
        redirect_stdout=True, combine_stdout_stderr=True)

  def testGetMaskBitsUnmaskedStable(self):
    output = '  |foo/bar-2.7.0:0'
    pinfo = cpu.PInfo(upgraded_cpv='foo/bar-2.7.0')
    self._TestSetUpgradedMaskBits(pinfo, output)
    self.assertTrue(pinfo.upgraded_unmasked)

  def testGetMaskBitsUnmaskedUnstable(self):
    output = ' ~|foo/bar-2.7.3:0'
    pinfo = cpu.PInfo(upgraded_cpv='foo/bar-2.7.3')
    self._TestSetUpgradedMaskBits(pinfo, output)
    self.assertTrue(pinfo.upgraded_unmasked)

  def testGetMaskBitsMaskedStable(self):
    output = 'M |foo/bar-2.7.4:0'
    pinfo = cpu.PInfo(upgraded_cpv='foo/bar-2.7.4')
    self._TestSetUpgradedMaskBits(pinfo, output)
    self.assertFalse(pinfo.upgraded_unmasked)

  def testGetMaskBitsMaskedUnstable(self):
    output = 'M~|foo/bar-2.7.4-r1:0'
    pinfo = cpu.PInfo(upgraded_cpv='foo/bar-2.7.4-r1')
    self._TestSetUpgradedMaskBits(pinfo, output)
    self.assertFalse(pinfo.upgraded_unmasked)


class CommitTest(CpuTestBase):
  """Test various commit-related Upgrader methods"""

  #
  # _ExtractUpgradedPkgs
  #

  def _TestExtractUpgradedPkgs(self, upgrade_lines):
    """Test Upgrader._ExtractUpgradedPkgs"""
    mocked_upgrader = self._MockUpgrader()

    # Verify.
    with self.OutputCapturer():
      return cpu.Upgrader._ExtractUpgradedPkgs(mocked_upgrader, upgrade_lines)

  def testExtractUpgradedPkgs(self):
    upgrade_lines = [
        'Upgraded abc/efg to version 1.2.3 on amd64, arm, x86',
        'Upgraded xyz/uvw to version 1.2.3 on amd64',
        'Upgraded xyz/uvw to version 3.2.1 on arm, x86',
        'Upgraded mno/pqr to version 12345 on x86',
    ]
    result = self._TestExtractUpgradedPkgs(upgrade_lines)
    self.assertEqual(result, ['efg', 'pqr', 'uvw'])

  #
  # _AmendCommitMessage
  #

  def _TestAmendCommitMessage(self, new_upgrade_lines,
                              old_upgrade_lines, remaining_lines,
                              git_show):
    """Test Upgrader._AmendCommitMessage"""
    mocked_upgrader = self._MockUpgrader()

    gold_lines = new_upgrade_lines + old_upgrade_lines

    # Replay script.
    def RunGit(cwd, _cmd, **_kwargs):
      self.assertEqual(mocked_upgrader._stable_repo, cwd)
      return cros_build_lib.CommandResult(returncode=0, output=git_show)
    mocked_upgrader._RunGit.side_effect = RunGit

    def CreateCommit(mock_upgrade_lines, mock_remaining_lines):
      self.assertEqual(gold_lines, mock_upgrade_lines)
      self.assertEqual(remaining_lines, mock_remaining_lines)
    mocked_upgrader._CreateCommitMessage.side_effect = CreateCommit

    # Verify.
    with self.OutputCapturer():
      cpu.Upgrader._AmendCommitMessage(mocked_upgrader,
                                       new_upgrade_lines)

  def testOldAndNew(self):
    new_upgrade_lines = [
        'Upgraded abc/efg to version 1.2.3 on amd64, arm, x86',
        'Upgraded mno/pqr to version 4.5-r1 on x86',
    ]
    old_upgrade_lines = [
        'Upgraded xyz/uvw to version 3.2.1 on arm, x86',
        'Upgraded mno/pqr to version 12345 on x86',
    ]
    remaining_lines = [
        'Extraneous extra comments in commit body.',
        '',
        'BUG=chromium-os:12345',
        'TEST=test everything',
        'again and again',
    ]
    git_show_output = ('\n'.join(old_upgrade_lines) + '\n'
                       + '\n'
                       + '\n'.join(remaining_lines))
    self._TestAmendCommitMessage(new_upgrade_lines, old_upgrade_lines,
                                 remaining_lines, git_show_output)

  def testOldOnly(self):
    old_upgrade_lines = [
        'Upgraded xyz/uvw to version 3.2.1 on arm, x86',
        'Upgraded mno/pqr to version 12345 on x86',
    ]
    git_show_output = ('\n'.join(old_upgrade_lines))
    self._TestAmendCommitMessage([], old_upgrade_lines, [], git_show_output)

  def testNewOnly(self):
    new_upgrade_lines = [
        'Upgraded abc/efg to version 1.2.3 on amd64, arm, x86',
        'Upgraded mno/pqr to version 4.5-r1 on x86',
    ]
    git_show_output = ''
    self._TestAmendCommitMessage(new_upgrade_lines, [], [], git_show_output)

  def testOldEditedAndNew(self):
    new_upgrade_lines = [
        'Upgraded abc/efg to version 1.2.3 on amd64, arm, x86',
        'Upgraded mno/pqr to version 4.5-r1 on x86',
    ]
    old_upgrade_lines = [
        'So I upgraded xyz/uvw to version 3.2.1 on arm, x86',
        'Then I Upgraded mno/pqr to version 12345 on x86',
    ]
    remaining_lines = [
        'Extraneous extra comments in commit body.',
        '',
        'BUG=chromium-os:12345',
        'TEST=test everything',
        'again and again',
    ]
    git_show_output = ('\n'.join(old_upgrade_lines) + '\n'
                       + '\n'
                       + '\n'.join(remaining_lines))

    # In this test, it should not recognize the existing old_upgrade_lines
    # as a previous commit message from this script.  So it should give a
    # warning and push those lines to the end (grouped with remaining_lines).
    remaining_lines = old_upgrade_lines + [''] + remaining_lines
    self._TestAmendCommitMessage(new_upgrade_lines, [],
                                 remaining_lines, git_show_output)

  #
  # _CreateCommitMessage
  #

  def _TestCreateCommitMessage(self, upgrade_lines):
    """Test Upgrader._CreateCommitMessage"""
    mocked_upgrader = self._MockUpgrader()

    upgrade_pkgs = cpu.Upgrader._ExtractUpgradedPkgs(mocked_upgrader,
                                                     upgrade_lines)

    def Extract(lines):
      self.assertEqual(upgrade_lines, lines)
      return upgrade_pkgs
    mocked_upgrader._ExtractUpgradedPkgs.side_effect = Extract

    # Verify.
    with self.OutputCapturer():
      result = cpu.Upgrader._CreateCommitMessage(mocked_upgrader, upgrade_lines)

    self.assertTrue(': upgraded package' in result or
                    'Upgraded the following' in result)
    return result

  def testCreateCommitMessageOnePkg(self):
    upgrade_lines = ['Upgraded abc/efg to version 1.2.3 on amd64, arm, x86']
    result = self._TestCreateCommitMessage(upgrade_lines)

    # Commit message should have:
    # - Summary that mentions 'efg' and ends in "package" (singular).
    # - Body corresponding to upgrade_lines.
    # - BUG= line (with space after '=' to invalidate it).
    # - TEST= line (with space after '=' to invalidate it).
    body = r'\n'.join([re.sub(r'\s+', r'\s', line) for line in upgrade_lines])
    regexp = re.compile(r"""^efg:\supgraded\spackage\sto\supstream\n # Summary
                            ^\s*\n                            # Blank line
                            %s\n                              # Body
                            ^\s*\n                            # Blank line
                            ^BUG=\s.+\n                       # BUG line
                            ^TEST=\s                          # TEST line
                            """ % body,
                        re.VERBOSE | re.MULTILINE)
    self.assertTrue(regexp.search(result))

  def testCreateCommitMessageThreePkgs(self):
    upgrade_lines = [
        'Upgraded abc/efg to version 1.2.3 on amd64, arm, x86',
        'Upgraded xyz/uvw to version 1.2.3 on amd64',
        'Upgraded xyz/uvw to version 3.2.1 on arm, x86',
        'Upgraded mno/pqr to version 12345 on x86',
    ]
    result = self._TestCreateCommitMessage(upgrade_lines)

    # Commit message should have:
    # - Summary that mentions 'efg, pqr, uvw' and ends in "packages" (plural).
    # - Body corresponding to upgrade_lines.
    # - BUG= line (with space after '=' to invalidate it).
    # - TEST= line (with space after '=' to invalidate it).
    body = r'\n'.join([re.sub(r'\s+', r'\s', line) for line in upgrade_lines])
    regexp = re.compile(r"""^efg,\spqr,\suvw:\supgraded\spackages.*\n # Summary
                            ^\s*\n                            # Blank line
                            %s\n                              # Body
                            ^\s*\n                            # Blank line
                            ^BUG=\s.+\n                       # BUG line
                            ^TEST=\s                          # TEST line
                            """ % body,
                        re.VERBOSE | re.MULTILINE)
    self.assertTrue(regexp.search(result))

  def testCreateCommitMessageTenPkgs(self):
    upgrade_lines = [
        'Upgraded abc/efg to version 1.2.3 on amd64, arm, x86',
        'Upgraded bcd/fgh to version 1.2.3 on amd64',
        'Upgraded cde/ghi to version 3.2.1 on arm, x86',
        'Upgraded def/hij to version 12345 on x86',
        'Upgraded efg/ijk to version 1.2.3 on amd64',
        'Upgraded fgh/jkl to version 3.2.1 on arm, x86',
        'Upgraded ghi/klm to version 12345 on x86',
        'Upgraded hij/lmn to version 1.2.3 on amd64',
        'Upgraded ijk/mno to version 3.2.1 on arm, x86',
        'Upgraded jkl/nop to version 12345 on x86',
    ]
    result = self._TestCreateCommitMessage(upgrade_lines)

    # Commit message should have:
    # - Summary that mentions '10' and ends in "packages" (plural).
    # - Body corresponding to upgrade_lines.
    # - BUG= line (with space after '=' to invalidate it).
    # - TEST= line (with space after '=' to invalidate it).
    body = r'\n'.join([re.sub(r'\s+', r'\s', line) for line in upgrade_lines])
    regexp = re.compile(r"""^Upgraded\s.*10.*\spackages\n     # Summary
                            ^\s*\n                            # Blank line
                            %s\n                              # Body
                            ^\s*\n                            # Blank line
                            ^BUG=\s.+\n                       # BUG line
                            ^TEST=\s                          # TEST line
                            """ % body,
                        re.VERBOSE | re.MULTILINE)
    self.assertTrue(regexp.search(result))


@unittest.skip('playground setup needs more work')
class GetCurrentVersionsTest(CpuTestBase):
  """Test Upgrader._GetCurrentVersions"""

  def _TestGetCurrentVersionsLocalCpv(self, target_pinfolist):
    cmdargs = []
    mocked_upgrader = self._MockUpgrader(cmdargs=cmdargs,
                                         _curr_board=None)
    self._SetUpPlayground()

    # Add test-specific mocks/stubs.
    _depgraph_mock = self.PatchObject(cpu.Upgrader, '_GetPreOrderDepGraph',
                                      return_value=[])

    # Replay script.
    targets = ['=' + pinfo.cpv for pinfo in target_pinfolist]
    pm_argv = cpu.Upgrader._GenParallelEmergeArgv(mocked_upgrader, targets)
    pm_argv.append('--root-deps')
    mocked_upgrader._GenParallelEmergeArgv.return_value = pm_argv

    # Verify.
    cpu.Upgrader._GetCurrentVersions(mocked_upgrader, target_pinfolist)

    mocked_upgrader._GenParallelEmergeArgv.assert_called_once_with(targets)
    # If we can get this unittest passing again, this is the old mox way of
    # checking the test worked.  We need a fuller sandbox first.
    #mocked_upgrader._SetPortTree(mox.IsA(portcfg.config), mox.IsA(dict))
    #packages = [pinfo.package for pinfo in target_pinfolist]
    #verifier = _GenDepsGraphVerifier(packages)
    #depgraph_mock.assert_called_once_with(mox.Func(verifier))

  def testGetCurrentVersionsTwoPkgs(self):
    target_pinfolist = [cpu.PInfo(package='dev-libs/A', cpv='dev-libs/A-2'),
                        cpu.PInfo(package='dev-libs/D', cpv='dev-libs/D-3')]
    self._TestGetCurrentVersionsLocalCpv(target_pinfolist)

  def testGetCurrentVersionsOnePkgB(self):
    target_pinfolist = [cpu.PInfo(package='dev-libs/B', cpv='dev-libs/B-2')]
    self._TestGetCurrentVersionsLocalCpv(target_pinfolist)

  def testGetCurrentVersionsOnePkgLibcros(self):
    target_pinfolist = [cpu.PInfo(package='chromeos-base/libcros',
                                  cpv='chromeos-base/libcros-1')]
    self._TestGetCurrentVersionsLocalCpv(target_pinfolist)

  def _TestGetCurrentVersionsPackageOnly(self, target_pinfolist):
    cmdargs = []
    mocked_upgrader = self._MockUpgrader(cmdargs=cmdargs,
                                         _curr_board=None)
    self._SetUpPlayground()

    # Add test-specific mocks/stubs.
    self.PatchObject(cpu.Upgrader, '_GetPreOrderDepGraph', return_value=[])

    # Replay script.
    packages = [pinfo.package for pinfo in target_pinfolist]
    pm_argv = cpu.Upgrader._GenParallelEmergeArgv(mocked_upgrader, packages)
    pm_argv.append('--root-deps')
    mocked_upgrader._GenParallelEmergeArgv.return_value = pm_argv

    # Verify.
    cpu.Upgrader._GetCurrentVersions(mocked_upgrader, target_pinfolist)

    mocked_upgrader._GenParallelEmergeArgv.assert_called_once_with(packages)
    # If we can get this unittest passing again, this is the old mox way of
    # checking the test worked.  We need a fuller sandbox first.
    #mocked_upgrader._SetPortTree(mox.IsA(portcfg.config), mox.IsA(dict))

  def testGetCurrentVersionsWorld(self):
    target_pinfolist = [cpu.PInfo(package='world', cpv='world')]
    self._TestGetCurrentVersionsPackageOnly(target_pinfolist)

  def testGetCurrentVersionsLocalOnlyB(self):
    target_pinfolist = [cpu.PInfo(package='dev-libs/B', cpv=None)]
    self._TestGetCurrentVersionsPackageOnly(target_pinfolist)


class ResolveAndVerifyArgsTest(CpuTestBase):
  """Test Upgrader._ResolveAndVerifyArgs"""

  def _TestResolveAndVerifyArgsWorld(self, upgrade_mode):
    args = ['world']
    cmdargs = []
    mocked_upgrader = self._MockUpgrader(cmdargs=cmdargs,
                                         _curr_board=None)

    # Verify.
    with self.OutputCapturer():
      result = cpu.Upgrader._ResolveAndVerifyArgs(mocked_upgrader, args,
                                                  upgrade_mode=upgrade_mode)

    self.assertEqual(result, [cpu.PInfo(user_arg='world',
                                        package='world',
                                        package_name='world',
                                        category=None,
                                        cpv='world')])

  def testResolveAndVerifyArgsWorldUpgradeMode(self):
    self._TestResolveAndVerifyArgsWorld(True)

  def testResolveAndVerifyArgsWorldStatusMode(self):
    self._TestResolveAndVerifyArgsWorld(False)

  def _TestResolveAndVerifyArgsNonWorld(self, pinfolist, cmdargs=None,
                                        error=None, error_checker=None):
    if cmdargs is None:
      cmdargs = []
    mocked_upgrader = self._MockUpgrader(cmdargs=cmdargs,
                                         _curr_board=None)
    upgrade_mode = cpu.Upgrader._IsInUpgradeMode(mocked_upgrader)

    # Replay script.
    calls_FindCurrentCPV = []
    calls_FindUpstreamCPV = []
    side_effect_FindCurrentCPV = []
    side_effect_FindUpstreamCPV = []
    args = []
    for pinfo in pinfolist:
      arg = pinfo.user_arg
      local_cpv = pinfo.cpv
      upstream_cpv = pinfo.upstream_cpv
      args.append(arg)

      catpkg = cpu.Upgrader._GetCatPkgFromCpv(arg)
      local_arg = catpkg if catpkg else arg

      calls_FindCurrentCPV.append(mock.call(local_arg))
      side_effect_FindCurrentCPV.append(local_cpv)

      calls_FindUpstreamCPV.append(mock.call(arg, mocked_upgrader._unstable_ok))
      side_effect_FindUpstreamCPV.append(upstream_cpv)

      if not upstream_cpv and upgrade_mode:
        # Real method raises an exception here.
        if not mocked_upgrader._unstable_ok:
          calls_FindUpstreamCPV.append(mock.call(arg, True))
          side_effect_FindUpstreamCPV.append(arg)
        break

    mocked_upgrader._FindCurrentCPV.side_effect = side_effect_FindCurrentCPV
    mocked_upgrader._FindUpstreamCPV.side_effect = side_effect_FindUpstreamCPV

    # Verify.
    result = None
    with self.OutputCapturer():
      if error:
        exc = self.AssertRaisesAndReturn(error,
                                         cpu.Upgrader._ResolveAndVerifyArgs,
                                         mocked_upgrader, args, upgrade_mode)
        if error_checker:
          check = error_checker(exc)
          self.assertTrue(check[0], msg=check[1])
      else:
        result = cpu.Upgrader._ResolveAndVerifyArgs(mocked_upgrader, args,
                                                    upgrade_mode)

    mocked_upgrader._FindCurrentCPV.assert_has_calls(calls_FindCurrentCPV)
    mocked_upgrader._FindUpstreamCPV.assert_has_calls(calls_FindUpstreamCPV)

    return result

  def testResolveAndVerifyArgsNonWorldUpgrade(self):
    pinfolist = [cpu.PInfo(user_arg='dev-libs/B',
                           cpv='dev-libs/B-1',
                           upstream_cpv='dev-libs/B-2')]
    cmdargs = ['--upgrade', '--unstable-ok']
    result = self._TestResolveAndVerifyArgsNonWorld(pinfolist, cmdargs)
    self.assertEqual(result, pinfolist)

  def testResolveAndVerifyArgsNonWorldUpgradeSpecificVer(self):
    pinfolist = [cpu.PInfo(user_arg='dev-libs/B-2',
                           cpv='dev-libs/B-1',
                           upstream_cpv='dev-libs/B-2')]
    cmdargs = ['--upgrade', '--unstable-ok']
    result = self._TestResolveAndVerifyArgsNonWorld(pinfolist, cmdargs)
    self.assertEqual(result, pinfolist)

  def testResolveAndVerifyArgsNonWorldUpgradeSpecificVerNotFoundStable(self):
    pinfolist = [cpu.PInfo(user_arg='dev-libs/B-2',
                           cpv='dev-libs/B-1')]
    cmdargs = ['--upgrade']

    def _error_checker(exception):
      # RuntimeError text should mention 'is unstable'.
      text = str(exception)
      phrase = 'is unstable'
      msg = 'No mention of "%s" in error message: %s' % (phrase, text)
      return (text.find(phrase) >= 0, msg)

    self._TestResolveAndVerifyArgsNonWorld(pinfolist, cmdargs,
                                           error=RuntimeError,
                                           error_checker=_error_checker)

  def testResolveAndVerifyArgsNonWorldUpgradeSpecificVerNotFoundUnstable(self):
    pinfolist = [cpu.PInfo(user_arg='dev-libs/B-2', cpv='dev-libs/B-1')]
    cmdargs = ['--upgrade', '--unstable-ok']

    def _error_checker(exception):
      # RuntimeError text should start with 'Unable to find'.
      text = str(exception)
      phrase = 'Unable to find'
      msg = 'Error message expected to start with "%s": %s' % (phrase, text)
      return (text.startswith(phrase), msg)

    self._TestResolveAndVerifyArgsNonWorld(pinfolist, cmdargs,
                                           error=RuntimeError,
                                           error_checker=_error_checker)

  def testResolveAndVerifyArgsNonWorldLocalOnly(self):
    pinfolist = [cpu.PInfo(user_arg='dev-libs/B', cpv='dev-libs/B-1')]
    cmdargs = ['--upgrade', '--unstable-ok']

    def _error_checker(exception):
      # RuntimeError text should start with 'Unable to find'.
      text = str(exception)
      phrase = 'Unable to find'
      msg = 'Error message expected to start with "%s": %s' % (phrase, text)
      return (text.startswith(phrase), msg)

    self._TestResolveAndVerifyArgsNonWorld(pinfolist, cmdargs,
                                           error=RuntimeError,
                                           error_checker=_error_checker)

  def testResolveAndVerifyArgsNonWorldUpstreamOnly(self):
    pinfolist = [cpu.PInfo(user_arg='dev-libs/B',
                           upstream_cpv='dev-libs/B-2')]
    cmdargs = ['--upgrade', '--unstable-ok']
    result = self._TestResolveAndVerifyArgsNonWorld(pinfolist, cmdargs)
    self.assertEqual(result, pinfolist)

  def testResolveAndVerifyArgsNonWorldNeither(self):
    pinfolist = [cpu.PInfo(user_arg='dev-libs/B')]
    cmdargs = ['--upgrade', '--unstable-ok']
    self._TestResolveAndVerifyArgsNonWorld(pinfolist, cmdargs,
                                           error=RuntimeError)

  def testResolveAndVerifyArgsNonWorldStatusSpecificVer(self):
    """Exception because specific cpv arg not allowed without --ugprade."""
    cmdargs = ['--unstable-ok']
    mocked_upgrader = self._MockUpgrader(cmdargs=cmdargs,
                                         _curr_board=None)
    upgrade_mode = cpu.Upgrader._IsInUpgradeMode(mocked_upgrader)

    # Verify.
    self.assertRaises(RuntimeError,
                      cpu.Upgrader._ResolveAndVerifyArgs,
                      mocked_upgrader,
                      ['dev-libs/B-2'], upgrade_mode)


class StabilizeEbuildTest(CpuTestBase):
  """Tests for _StabilizeEbuild()."""

  PREFIX_LINES = [
      'Garbletygook nonsense unimportant',
      'Some other nonsense with KEYWORDS mention',
  ]
  POSTFIX_LINES = [
      'Some mention of KEYWORDS in a line',
      'And other nonsense',
  ]

  def _TestStabilizeEbuild(self, ebuild_path, arch):
    mocked_upgrader = self._MockUpgrader(cmdargs=[], _curr_arch=arch)

    # This is the verification phase.
    with self.OutputCapturer():
      cpu.Upgrader._StabilizeEbuild(mocked_upgrader, ebuild_path)

  def _AssertEqualsExcludingComments(self, lines1, lines2):
    lines1 = [ln for ln in lines1 if not ln.startswith('#')]
    lines2 = [ln for ln in lines2 if not ln.startswith('#')]

    self.assertEqual(lines1, lines2)

  def _TestStabilizeEbuildWrapper(self, ebuild_path, arch,
                                  keyword_line, gold_keyword_line):
    if not isinstance(keyword_line, list):
      keyword_line = [keyword_line]
    if not isinstance(gold_keyword_line, list):
      gold_keyword_line = [gold_keyword_line]

    input_content = self.PREFIX_LINES + keyword_line + self.POSTFIX_LINES
    gold_content = self.PREFIX_LINES + gold_keyword_line + self.POSTFIX_LINES

    # Write contents to ebuild_path before test.
    osutils.WriteFile(ebuild_path, '\n'.join(input_content))

    self._TestStabilizeEbuild(ebuild_path, arch)

    # Read content back after test.
    content_lines = osutils.ReadFile(ebuild_path).splitlines()

    self._AssertEqualsExcludingComments(gold_content, content_lines)

  @osutils.TempFileDecorator
  def testNothingToDo(self):
    arch = 'arm'
    keyword_line = 'KEYWORDS="amd64 arm mips x86"'
    gold_keyword_line = 'KEYWORDS="*"'
    self._TestStabilizeEbuildWrapper(self.tempfile, arch,
                                     keyword_line, gold_keyword_line)

  @osutils.TempFileDecorator
  def testNothingToDoFbsd(self):
    arch = 'x86'
    keyword_line = 'KEYWORDS="amd64 arm ~mips x86 ~x86-fbsd"'
    gold_keyword_line = 'KEYWORDS="*"'
    self._TestStabilizeEbuildWrapper(self.tempfile, arch,
                                     keyword_line, gold_keyword_line)

  @osutils.TempFileDecorator
  def testSimpleMiddleOfLine(self):
    arch = 'arm'
    keyword_line = 'KEYWORDS="amd64 ~arm ~mips x86"'
    gold_keyword_line = 'KEYWORDS="*"'
    self._TestStabilizeEbuildWrapper(self.tempfile, arch,
                                     keyword_line, gold_keyword_line)

  @osutils.TempFileDecorator
  def testSimpleMiddleOfLineSpacePrefix(self):
    arch = 'arm'
    keyword_line = '    KEYWORDS="amd64 ~arm ~mips x86"'
    gold_keyword_line = '    KEYWORDS="*"'
    self._TestStabilizeEbuildWrapper(self.tempfile, arch,
                                     keyword_line, gold_keyword_line)

  @osutils.TempFileDecorator
  def testSimpleStartOfLine(self):
    arch = 'arm'
    keyword_line = 'KEYWORDS="~arm amd64 ~mips x86"'
    gold_keyword_line = 'KEYWORDS="*"'
    self._TestStabilizeEbuildWrapper(self.tempfile, arch,
                                     keyword_line, gold_keyword_line)

  @osutils.TempFileDecorator
  def testSimpleEndOfLine(self):
    arch = 'arm'
    keyword_line = 'KEYWORDS="amd64 ~mips x86 ~arm"'
    gold_keyword_line = 'KEYWORDS="*"'
    self._TestStabilizeEbuildWrapper(self.tempfile, arch,
                                     keyword_line, gold_keyword_line)

  @osutils.TempFileDecorator
  def testPreFbsd(self):
    arch = 'x86'
    keyword_line = 'KEYWORDS="amd64 ~arm ~mips ~x86 ~x86-fbsd"'
    gold_keyword_line = 'KEYWORDS="*"'
    self._TestStabilizeEbuildWrapper(self.tempfile, arch,
                                     keyword_line, gold_keyword_line)

  @osutils.TempFileDecorator
  def testPostFbsd(self):
    arch = 'x86'
    keyword_line = 'KEYWORDS="amd64 ~arm ~mips ~x86-fbsd ~x86"'
    gold_keyword_line = 'KEYWORDS="*"'
    self._TestStabilizeEbuildWrapper(self.tempfile, arch,
                                     keyword_line, gold_keyword_line)

  @osutils.TempFileDecorator
  def testMultilineKeywordsMiddle(self):
    arch = 'arm'
    keyword_lines = [
        'KEYWORDS="amd64',
        '  ~arm',
        '  ~mips',
        '  x86"',
    ]
    gold_keyword_lines = ['KEYWORDS="*"',]
    self._TestStabilizeEbuildWrapper(self.tempfile, arch,
                                     keyword_lines, gold_keyword_lines)

  @osutils.TempFileDecorator
  def testMultilineKeywordsStart(self):
    arch = 'amd64'
    keyword_lines = [
        'KEYWORDS="~amd64',
        '  arm',
        '  ~mips',
        '  x86"',
    ]
    gold_keyword_lines = ['KEYWORDS="*"',]
    self._TestStabilizeEbuildWrapper(self.tempfile, arch,
                                     keyword_lines, gold_keyword_lines)

  @osutils.TempFileDecorator
  def testMultilineKeywordsEnd(self):
    arch = 'x86'
    keyword_lines = [
        'KEYWORDS="amd64',
        '  arm',
        '  ~mips',
        '  ~x86"',
    ]
    gold_keyword_lines = ['KEYWORDS="*"',]
    self._TestStabilizeEbuildWrapper(self.tempfile, arch,
                                     keyword_lines, gold_keyword_lines)

  @osutils.TempFileDecorator
  def testMultipleKeywordLinesOneChange(self):
    arch = 'arm'
    keyword_lines = [
        'KEYWORDS="amd64 arm mips x86"',
        'KEYWORDS="~amd64 ~arm ~mips ~x86"',
    ]
    gold_keyword_lines = ['KEYWORDS="*"',] * 2
    self._TestStabilizeEbuildWrapper(self.tempfile, arch,
                                     keyword_lines, gold_keyword_lines)

  @osutils.TempFileDecorator
  def testMultipleKeywordLinesMultipleChanges(self):
    arch = 'arm'
    keyword_lines = [
        'KEYWORDS="amd64 ~arm mips x86"',
        'KEYWORDS="~amd64 ~arm ~mips ~x86"',
    ]
    gold_keyword_lines = ['KEYWORDS="*"',] * 2
    self._TestStabilizeEbuildWrapper(self.tempfile, arch,
                                     keyword_lines, gold_keyword_lines)

  @osutils.TempFileDecorator
  def testMultipleKeywordLinesMultipleChangesSpacePrefix(self):
    arch = 'arm'
    keyword_lines = [
        '     KEYWORDS="amd64 ~arm mips x86"',
        '     KEYWORDS="~amd64 ~arm ~mips ~x86"',
    ]
    gold_keyword_lines = ['     KEYWORDS="*"',] * 2
    self._TestStabilizeEbuildWrapper(self.tempfile, arch,
                                     keyword_lines, gold_keyword_lines)


@unittest.skip('playground setup needs more work')
class GetPreOrderDepGraphTest(CpuTestBase):
  """Test the Upgrader class from cros_portage_upgrade."""

  #
  # _GetPreOrderDepGraph (defunct - to be replaced)
  #

  def _TestGetPreOrderDepGraph(self, pkg):
    """Test the behavior of the Upgrader._GetPreOrderDepGraph method."""

    cmdargs = []
    mocked_upgrader = self._MockUpgrader(cmdargs=cmdargs,
                                         _curr_board=None)
    self._SetUpPlayground()

    # Verify.
    pm_argv = cpu.Upgrader._GenParallelEmergeArgv(mocked_upgrader, [pkg])
    pm_argv.append('--root-deps')
    deps = depgraph.DepGraphGenerator()
    deps.Initialize(pm_argv)
    deps_tree, deps_info = deps.GenDependencyTree()
    deps_graph = deps.GenDependencyGraph(deps_tree, deps_info)

    deps_list = cpu.Upgrader._GetPreOrderDepGraph(deps_graph)
    golden_deps_set = _GetGoldenDepsSet(pkg)
    self.assertEqual(set(deps_list), golden_deps_set)

  def testGetPreOrderDepGraphDevLibsA(self):
    return self._TestGetPreOrderDepGraph('dev-libs/A')

  def testGetPreOrderDepGraphDevLibsC(self):
    return self._TestGetPreOrderDepGraph('dev-libs/C')

  def testGetPreOrderDepGraphVirtualLibusb(self):
    return self._TestGetPreOrderDepGraph('virtual/libusb')

  def testGetPreOrderDepGraphCrosbaseLibcros(self):
    return self._TestGetPreOrderDepGraph('chromeos-base/libcros')


class MainTest(CpuTestBase):
  """Test argument handling at the main method level."""

  def _AssertCPUMain(self, args, expect_zero):
    """Run cpu.main() and assert exit value is expected.

    If |expect_zero| is True, assert exit value = 0.  If False,
    assert exit value != 0.
    """
    try:
      cpu.main(args)
    except SystemExit as e:
      if expect_zero:
        self.assertEqual(e.args[0], 0,
                         msg='expected call to main() to exit cleanly, '
                         'but it exited with code %d' % e.args[0])
      else:
        self.assertNotEqual(e.args[0], 0,
                            msg='expected call to main() to exit with '
                            'failure code, but exited with code 0 instead.')

  def testHelp(self):
    """Test that --help is functioning"""

    with self.OutputCapturer() as output:
      # Running with --help should exit with code==0.
      try:
        cpu.main(['--help'])
      except SystemExit as e:
        self.assertEqual(e.args[0], 0)

    # Verify that a message beginning with "Usage: " was printed.
    stdout = output.GetStdout()
    self.assertTrue(stdout.startswith('usage: '))

  def testMissingBoard(self):
    """Test that running without --board exits with an error."""
    with self.OutputCapturer():
      # Running without --board should exit with code!=0.
      try:
        cpu.main([])
      except SystemExit as e:
        self.assertNotEqual(e.args[0], 0)

    # Verify that an error message was printed.
    self.AssertOutputEndsInError()

  def testBoardWithoutPackage(self):
    """Test that running without a package argument exits with an error."""
    with self.OutputCapturer():
      # Running without a package should exit with code!=0.
      self._AssertCPUMain(['--board=any-board'], expect_zero=False)

    # Verify that an error message was printed.
    self.AssertOutputEndsInError()

  def testHostWithoutPackage(self):
    """Test that running without a package argument exits with an error."""
    with self.OutputCapturer():
      # Running without a package should exit with code!=0.
      self._AssertCPUMain(['--host'], expect_zero=False)

    # Verify that an error message was printed.
    self.AssertOutputEndsInError()

  def testUpgradeAndUpgradeDeep(self):
    """Running with --upgrade and --upgrade-deep exits with an error."""
    with self.OutputCapturer():
      # Expect exit with code!=0.
      self._AssertCPUMain(['--host', '--upgrade', '--upgrade-deep',
                           'any-package'], expect_zero=False)

    # Verify that an error message was printed.
    self.AssertOutputEndsInError()

  def testForceWithoutUpgrade(self):
    """Running with --force requires --upgrade or --upgrade-deep."""
    with self.OutputCapturer():
      # Expect exit with code!=0.
      self._AssertCPUMain(['--host', '--force', 'any-package'],
                          expect_zero=False)

    # Verify that an error message was printed.
    self.AssertOutputEndsInError()

  def testFlowStatusReportOneBoard(self):
    """Test main flow for basic one-board status report."""
    self.PatchObject(cpu.Upgrader, 'PreRunChecks')
    self.PatchObject(cpu, '_BoardIsSetUp', return_value=True)
    self.PatchObject(cpu.Upgrader, 'PrepareToRun')
    self.PatchObject(cpu.Upgrader, 'RunBoard')
    self.PatchObject(cpu.Upgrader, 'RunCompleted')
    self.PatchObject(cpu.Upgrader, 'WriteTableFiles')

    with self.OutputCapturer():
      self._AssertCPUMain(['--board=any-board', '--to-csv=/dev/null',
                           'any-package'], expect_zero=True)

  def testFlowStatusReportOneBoardNotSetUp(self):
    """Test main flow for basic one-board status report."""
    self.PatchObject(cpu.Upgrader, 'PreRunChecks')
    self.PatchObject(cpu, '_BoardIsSetUp', return_value=False)

    # Running with a package not set up should exit with code!=0.
    with self.OutputCapturer():
      self._AssertCPUMain(['--board=any-board', '--to-csv=/dev/null',
                           'any-package'], expect_zero=False)

    # Verify that an error message was printed.
    self.AssertOutputEndsInError()

  def testFlowStatusReportTwoBoards(self):
    """Test main flow for two-board status report."""
    self.PatchObject(cpu.Upgrader, 'PreRunChecks')
    self.PatchObject(cpu, '_BoardIsSetUp', return_value=True)
    self.PatchObject(cpu.Upgrader, 'PrepareToRun')
    self.PatchObject(cpu.Upgrader, 'RunBoard')
    self.PatchObject(cpu.Upgrader, 'RunCompleted')
    self.PatchObject(cpu.Upgrader, 'WriteTableFiles')

    with self.OutputCapturer():
      self._AssertCPUMain(['--board=board1:board2', 'any-package'],
                          expect_zero=True)

  def testFlowUpgradeOneBoard(self):
    """Test main flow for basic one-board upgrade."""
    self.PatchObject(cpu.Upgrader, 'PreRunChecks')
    self.PatchObject(cpu.Upgrader, 'CheckBoardList')
    self.PatchObject(cpu, '_BoardIsSetUp', return_value=True)
    self.PatchObject(cpu.Upgrader, 'PrepareToRun')
    self.PatchObject(cpu.Upgrader, 'RunBoard')
    self.PatchObject(cpu.Upgrader, 'RunCompleted')
    self.PatchObject(cpu.Upgrader, 'WriteTableFiles')

    with self.OutputCapturer():
      self._AssertCPUMain(['--upgrade', '--board=any-board', 'any-package'],
                          expect_zero=True)

  def testFlowUpgradeTwoBoards(self):
    """Test main flow for two-board upgrade."""
    self.PatchObject(cpu.Upgrader, 'PreRunChecks')
    self.PatchObject(cpu.Upgrader, 'CheckBoardList')
    self.PatchObject(cpu, '_BoardIsSetUp', return_value=True)
    self.PatchObject(cpu.Upgrader, 'PrepareToRun')
    self.PatchObject(cpu.Upgrader, 'RunBoard')
    self.PatchObject(cpu.Upgrader, 'RunCompleted')
    self.PatchObject(cpu.Upgrader, 'WriteTableFiles')

    with self.OutputCapturer():
      self._AssertCPUMain(['--upgrade', '--board=board1:board2',
                           '--to-csv=/dev/null', 'any-package'],
                          expect_zero=True)

  def testFlowUpgradeTwoBoardsAndHost(self):
    """Test main flow for two-board and host upgrade."""
    self.PatchObject(cpu.Upgrader, 'PreRunChecks')
    self.PatchObject(cpu.Upgrader, 'CheckBoardList')
    self.PatchObject(cpu, '_BoardIsSetUp', return_value=True)
    self.PatchObject(cpu.Upgrader, 'PrepareToRun')
    self.PatchObject(cpu.Upgrader, 'RunBoard')
    self.PatchObject(cpu.Upgrader, 'RunCompleted')
    self.PatchObject(cpu.Upgrader, 'WriteTableFiles')

    with self.OutputCapturer():
      self._AssertCPUMain(['--upgrade', '--host', '--board=board1:host:board2',
                           '--to-csv=/dev/null', 'any-package'],
                          expect_zero=True)
