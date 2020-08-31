# -*- coding: utf-8 -*-
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Common functions for interacting with repo manifest XML files."""

from __future__ import print_function

import sys

# TODO(vapier): Use ElementTree directly once we're Python 3-only.
from xml.etree import cElementTree as ElementTree


assert sys.version_info >= (3, 6), 'This module requires Python 3.6+'


# The ElementTree.tostring method is a bit of a mess.  Under Python 2, it
# returns a string, but Python 3 returns bytes (even though its name says
# "to string").  In order to get a string, encoding='unicode' is required
# (encoding='utf-8' still returns bytes).  But Python 2 doesn't recognize
# "unicode", only "utf-8".
# https://bugs.python.org/issue10942
# TODO(vapier): Inline the setting once we're Python 3-only.
TOSTRING_ENCODING = 'utf-8' if sys.version_info.major < 3 else 'unicode'


class Error(Exception):
  """An error related to repo manifests."""


class InvalidManifest(Error):
  """The manifest data has invalid values or structure."""


class UnsupportedFeature(Error):
  """The manifest data uses features that are not supported by this code."""


class Manifest(object):
  """Manifest represents the contents of a repo manifest XML file."""

  # https://chromium.googlesource.com/external/repo/+/master/docs/manifest-format.md

  def __init__(self, etree, allow_unsupported_features=False):
    """Initialize Manifest.

    Args:
      etree: An ElementTree object representing a manifest XML file.
      allow_unsupported_features: If true, will not explode if manifest
          contains unsupported features such as includes.
    """
    self._etree = etree
    self._allow_unsupported_features = allow_unsupported_features
    self._ValidateTree()

  # These __*state__ pickle protocol methods are intended for multiprocessing.

  def __getstate__(self):
    """Return picklable state for this Manifest."""
    return (ElementTree.tostring(self._etree.getroot(),
                                 encoding=TOSTRING_ENCODING),
            self._allow_unsupported_features)

  def __setstate__(self, state):
    """Set the state from pickle for this Manifest."""
    tree_string, allow_unsupported_features = state
    root = ElementTree.fromstring(tree_string)
    self._etree = ElementTree.ElementTree(root)
    self._allow_unsupported_features = allow_unsupported_features

  def _ValidateTree(self):
    """Raise Error if self._etree is not a valid manifest tree.

    Args:
      allow_unsupported_features: If true, ignore unsupported features.
    """
    root = self._etree.getroot()
    if root is None:
      raise InvalidManifest('no data')
    if root.tag != 'manifest':
      raise InvalidManifest('expected root <manifest>, got <%s>' % root.tag)
    if not self._allow_unsupported_features:
      if self._etree.find('./project/project') is not None:
        raise UnsupportedFeature('nested <project>')
      for unsupported_tag in ('extend-project', 'remove-project', 'include'):
        if self._etree.find(unsupported_tag) is not None:
          raise UnsupportedFeature('<%s>' % unsupported_tag)

  @classmethod
  def FromFile(cls, source, allow_unsupported_features=False):
    """Parse XML into a Manifest.

    Args:
      source: A string path or file object containing XML data.
      allow_unsupported_features: If true, will not explode if manifest
          contains unsupported features such as includes.

    Raises:
      Error: If source couldn't be parsed into a Manifest.
    """
    return cls(ElementTree.parse(source),
               allow_unsupported_features=allow_unsupported_features)

  @classmethod
  def FromString(cls, data, allow_unsupported_features=False):
    """Parse XML into a Manifest.

    Args:
      data: A string containing XML data.
      allow_unsupported_features: If true, will not explode if manifest
          contains unsupported features such as includes.

    Raises:
      Error: If data couldn't be parsed into a Manifest.
    """
    root = ElementTree.fromstring(data)
    return cls(ElementTree.ElementTree(root),
               allow_unsupported_features=allow_unsupported_features)

  def Write(self, dest):
    """Write the Manifest as XML.

    Args:
      dest: A string path or file object to write to.

    Raises:
      Error: If the Manifest contains invalid data.
    """
    self._ValidateTree()
    self._etree.write(dest, encoding='UTF-8', xml_declaration=True)

  def Default(self):
    """Return this Manifest's Default or an empty Default."""
    default_element = self._etree.find('default')
    if default_element is None:
      default_element = ElementTree.fromstring('<default/>')
    return Default(self, default_element)

  def Includes(self):
    """Yield an Include for each <include> element in the manifest.

    This class does NOT process includes, so they are considered an unsupported
    feature. You must set allow_unsupported_features to use this function.
    """
    if not self._allow_unsupported_features:
      raise UnsupportedFeature('allow unsupported features to use Includes()')
    for include_element in self._etree.iterfind('include'):
      yield Include(self, include_element)

  def Remotes(self):
    """Yield a Remote for each <remote> element in the manifest."""
    for remote_element in self._etree.iterfind('remote'):
      yield Remote(self, remote_element)

  def GetRemote(self, name):
    """Return the Remote with the given name.

    Raises:
      ValueError: If no Remote has the given name.
    """
    for remote in self.Remotes():
      if remote.name == name:
        return remote
    raise ValueError('no remote named %s' % name)

  def Projects(self):
    """Yield a Project for each <project> element in the manifest."""
    for project_element in self._etree.iterfind('project'):
      yield Project(self, project_element)

  def GetUniqueProject(self, name, branch=None):
    """Return the unique Project with the given name and optional branch.

    Args:
      name: The name of the project to search for.
      branch: The optional branch to search (without the 'refs/heads/').

    Raises:
      ValueError: If there is not exactly one Project with the given name/branch
    """
    projects = []
    for project in self.Projects():
      # Strip the 'refs/heads/' from the branch name
      stripped_branch = project.Revision() or 'master'
      if stripped_branch.startswith('refs/heads/'):
        stripped_branch = stripped_branch[len('refs/heads/'):]
      if (branch is None or stripped_branch == branch) and project.name == name:
        projects.append(project)
    if len(projects) != 1:
      raise ValueError(
          '%s projects named %s on branch %s' % (len(projects), name, branch))
    return projects[0]


class _ManifestElement(object):
  """Subclasses of _ManifestElement wrap Manifest child XML elements."""

  ATTRS = ()
  TAG = None

  def __init__(self, manifest, element):
    tag = self.TAG or self.__class__.__name__.lower()
    if element.tag != tag:
      raise ValueError('expected a <%s> but got a <%s>' % (tag, element.tag))
    self._manifest = manifest
    self._el = element

  # These __*state__ pickle protocol methods are intended for multiprocessing.

  def __getstate__(self):
    """Return picklable state for this element."""
    return (self._manifest,
            ElementTree.tostring(self._el, encoding=TOSTRING_ENCODING))

  def __setstate__(self, state):
    """Set the state from pickle for this element."""
    self._manifest, xml_data = state
    self._el = ElementTree.fromstring(xml_data)

  def _XMLAttrName(self, name):
    """Return the XML attr name for the given Python attr name."""
    if name not in self.ATTRS:
      raise AttributeError(
          '%r has no attribute %r' % (self.__class__.__name__, name))
    return name.replace('_', '-')

  def __getattr__(self, name):
    return self._el.get(self._XMLAttrName(name))

  def __setattr__(self, name, value):
    if name.startswith('_'):
      super(_ManifestElement, self).__setattr__(name, value)
    else:
      self._el.set(self._XMLAttrName(name), value)

  def __delattr__(self, name):
    if name.startswith('_'):
      super(_ManifestElement, self).__delattr__(name)
    else:
      self._el.attrib.pop(self._XMLAttrName(name))

  def __str__(self):
    s = self.__class__.__name__
    if 'name' in self.ATTRS:
      s = '%s %r' % (s, self.name)
    return '<%s>' % s

  def __repr__(self):
    return ElementTree.tostring(self._el, encoding=TOSTRING_ENCODING)


class Remote(_ManifestElement):
  """Remote represents a <remote> element of a Manifest."""

  ATTRS = ('name', 'alias', 'fetch', 'pushurl', 'review', 'revision')

  def GitName(self):
    """Return the git remote name for this Remote."""
    return self.alias or self.name

  def PushURL(self):
    """Return the effective push URL of this Remote."""
    return self.pushurl or self.fetch


class Include(_ManifestElement):
  """Include represents an <include> element of the manifest."""

  ATTRS = ('name',)


class Default(_ManifestElement):
  """Default represents a <default> element of a Manifest."""

  ATTRS = ('remote', 'revision', 'dest_branch', 'sync_j', 'sync_c', 'sync_s')


class Project(_ManifestElement):
  """Project represents a <project> element of a Manifest."""

  ATTRS = ('name', 'path', 'remote', 'revision', 'dest_branch', 'groups',
           'sync_c', 'sync_s', 'upstream', 'clone_depth', 'force_path')

  def Path(self):
    """Return the effective filesystem path of this Project."""
    return self.path or self.name

  def RemoteName(self):
    """Return the effective remote name of this Project."""
    return self.remote or self._manifest.Default().remote

  def Remote(self):
    """Return the Remote for this Project."""
    return self._manifest.GetRemote(self.RemoteName())

  def Revision(self):
    """Return the effective revision of this Project."""
    return (self.revision or self.Remote().revision or
            self._manifest.Default().revision)

  def Annotations(self):
    """Return a dictionary from annotation key to annotation value."""
    return {
        child.get('name'): child.get('value')
        for child in self._el
        if child.tag == 'annotation'
    }
