# -*- coding: utf-8 -*-
# Copyright 2020 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Utilities for setting up Portage objects for testing."""

from __future__ import print_function
from __future__ import division

import itertools
import os
import pathlib  # pylint: disable=import-error

from chromite.lib import constants
from chromite.lib import cros_build_lib
from chromite.lib import osutils
from chromite.lib import portage_util

__all__ = ['Overlay', 'Package', 'Profile', 'Sysroot']


def _dict_to_conf(dictionary):
  """Helper to format a dictionary into a layout.conf file."""
  output = []
  for key in sorted(dictionary.keys()):
    output.append('%s = %s' % (key, dictionary[key]))

  output.append('\n')
  return '\n'.join(output)


def _dict_to_ebuild(dictionary):
  """Helper to format a dictionary into an ebuild file."""
  output = []
  for key in sorted(dictionary.keys()):
    output.append('%s="%s"' % (key, dictionary[key]))

  output.append('\n')
  return '\n'.join(output)


class Overlay(object):
  """Portage overlay object, responsible for all writes to its directory."""

  HIERARCHY_NAMES = ('stable', 'project', 'chipset', 'baseboard', 'board',
                     'board-private')

  def __init__(self, root_path, name, masters=None):
    self.path = pathlib.Path(root_path)
    self.name = str(name)
    self.masters = tuple(masters) if masters else None
    self.packages = []
    self.profiles = dict()
    self.categories = set()

    self._write_layout_conf()

  def __contains__(self, item: portage_util.CPV):
    if not isinstance(item, portage_util.CPV):
      raise TypeError(f'Expected a CPV but received a {type(item)}')

    ebuild_path = self.path / item.category / item.package / f'{item.pv}.ebuild'

    return ebuild_path.is_file()

  def _write_layout_conf(self):
    """Write out the layout.conf as part of this Overlay's initialization."""
    layout_conf_path = self.path / 'metadata' / 'layout.conf'
    master_names = ' '.join(
        m.name for m in self.masters) if self.masters else ''
    conf = {
        'masters': master_names,
        'profile-formats': 'portage-2 profile-default-eapi',
        'profile_eapi_when_unspecified': '5-progress',
        'repo-name': str(self.name),
        'thin-manifests': 'true',
        'use-manifests': 'strict',
    }

    osutils.WriteFile(layout_conf_path, _dict_to_conf(conf), makedirs=True)

  def add_package(self, pkg):
    """Add a package to this overlay.

    Adds the package to the Overlay object's internal storage and writes the
    package metadata to the Overlay's directory.
    """
    self.packages.append(pkg)
    self._write_ebuild(pkg)
    if pkg.category not in self.categories:
      self.categories.add(pkg.category)
      osutils.WriteFile(
          self.path / 'profiles' / 'categories',
          pkg.category + '\n',
          mode='a',
          makedirs=True)

  def _write_ebuild(self, pkg):
    """Write a Package object out to an ebuild file in this Overlay."""
    ebuild_path = self.path / pkg.category / pkg.package / (
        pkg.package + '-' + pkg.version + '.ebuild')

    # EAPI must be the first thing defined in a ebuild, so write this config
    # before anything else.
    base_conf = {
        'EAPI': pkg.eapi,
        'KEYWORDS': pkg.keywords,
        'SLOT': pkg.slot,
    }

    osutils.WriteFile(ebuild_path, _dict_to_ebuild(base_conf), makedirs=True)

    extra_conf = {
        'DEPEND': pkg.depend,
        'RDEPEND': pkg.rdepend,
    }

    osutils.WriteFile(ebuild_path, _dict_to_ebuild(extra_conf), mode='a')

  def create_profile(self, path=None, profile_parents=None, make_defaults=None):
    """Create a profile in this overlay.

    Creates a profile with the given settings and writes the profile with those
    settings to the Overlay's directory.
    """
    path = pathlib.Path(path) if path else pathlib.Path('base')
    if path in self.profiles:
      raise KeyError('A profile with that path already exists!')

    prof = Profile(
        self, path, parents=profile_parents, make_defaults=make_defaults)

    self._write_profile(prof)
    self.profiles[path] = prof

    return prof

  def _write_profile(self, profile):
    """Write a Profile object out to this Overlay's directory."""
    osutils.WriteFile(
        self.path / 'profiles' / profile.path / 'make.defaults',
        _dict_to_ebuild(profile.make_defaults),
        makedirs=True)
    if profile.parents:
      formatted_parents = []
      for parent in profile.parents:
        formatted_parents.append(str(parent.overlay) + ':' + str(parent.path))
      osutils.WriteFile(self.path / 'profiles' / profile.path / 'parents',
                        '\n'.join(formatted_parents) + '\n')


class Sysroot(object):
  """Sysroot object representing a functional Portage directory for testing."""

  # These PORTDIR_OVERLAY entries are necessary for any Portage operations to
  # function as the chroot's profile is parsed first, even if that profile is
  # not used by the sysroot at all.
  # This tuple should effectively be:
  # /mnt/host/source/src/third_party/portage-stable
  # /mnt/host/source/src/third_party/chromiumos-overlay
  # /mnt/host/source/src/third_party/eclass-overlay
  _BOOTSTRAP_PORTDIR_OVERLAYS = (
      os.path.join(constants.CHROOT_SOURCE_ROOT,
                   constants.PORTAGE_STABLE_OVERLAY_DIR),
      os.path.join(constants.CHROOT_SOURCE_ROOT,
                   constants.CHROMIUMOS_OVERLAY_DIR),
      os.path.join(constants.CHROOT_SOURCE_ROOT, constants.ECLASS_OVERLAY_DIR),
  )

  def __init__(self, path, profile, overlays):
    self.path = path

    osutils.SafeMakedirs(path / 'etc' / 'portage' / 'profile')
    osutils.SafeMakedirs(path / 'etc' / 'portage' / 'hooks')

    osutils.SafeSymlink(profile.full_path,
                        path / 'etc' / 'portage' / 'make.profile')

    sysroot_conf = {
        'ACCEPT_KEYWORDS':
            'amd64',
        'ROOT':
            str(self.path),
        'SYSROOT':
            str(self.path),
        'ARCH':
            'amd64',
        'PORTAGE_CONFIGROOT':
            str(self.path),
        'PORTDIR':
            str(overlays[0].path),
        'PORTDIR_OVERLAY':
            '\n'.join(
                itertools.chain((str(o.path) for o in overlays),
                                Sysroot._BOOTSTRAP_PORTDIR_OVERLAYS)),
        'PORTAGE_BINHOST':
            '',
        'USE':
            '',
        'PKGDIR':
            str(self.path / 'packages'),
        'PORTAGE_TMPDIR':
            str(self.path / 'tmp'),
        'DISTDIR':
            str(self.path / 'var' / 'lib' / 'portage' / 'distfiles'),
    }

    osutils.WriteFile(self.path / 'etc' / 'portage' / 'make.conf',
                      _dict_to_ebuild(sysroot_conf))

  @property
  def _env(self):
    """Return a dict of the environment variables for this sysroot."""
    return {
        'PORTAGE_CONFIGROOT': str(self.path),
        'ROOT': str(self.path),
        'SYSROOT': str(self.path),
        'BROOT': str(self.path),
    }

  def run(self, cmd, **kwargs):
    """Run a command against this sysroot.

    This method sets up the equivalent calling environment to the `emerge`
    wrappers we generate but targeted at this specific sysroot, which has
    an arbitrary path in the test environment. This means that Portage commands
    such as `equery list '*'` will correctly run against this sysroot.
    """
    extra_env = self._env
    extra_env.update(kwargs.pop('extra_env', {}))

    return cros_build_lib.run(
        cmd, extra_env=extra_env, encoding='utf-8', **kwargs)


class Profile(object):
  """Portage profile, lives in an overlay."""

  def __init__(self, overlay, path, parents=None, make_defaults=None):
    self.overlay = overlay.name
    self.path = path
    self.full_path = overlay.path / 'profiles' / path
    self.parents = tuple(parents) if parents else None
    self.make_defaults = make_defaults if make_defaults else {'USE': ''}


class Package(object):
  """Portage package, lives in an overlay."""

  def __init__(self,
               category,
               package,
               version='1',
               eapi='7',
               keywords='*',
               slot='0',
               depend='',
               rdepend=''):
    self.category = category
    self.package = package
    self.version = version
    self.eapi = eapi
    self.keywords = keywords
    self.slot = slot
    self.depend = depend
    self.rdepend = rdepend

  @classmethod
  def from_cpv(cls, pkg_str: str):
    """Creates a Package from a CPV string."""
    cpv = portage_util.SplitCPV(pkg_str)
    return cls(category=cpv.category, package=cpv.package, version=cpv.version)

  @property
  def cpv(self) -> portage_util.CPV:
    """Returns a CPV object constructed from this package's metadata."""
    return portage_util.SplitCPV(self.category + '/' + self.package + '-' +
                                 self.version)
