# Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This module allows adding and deleting of projects to the local manifest."""

import logging
import platform
import optparse
import os
import sys
import xml.etree.ElementTree as ElementTree
from chromite.lib import cros_build_lib
from chromite.lib import git


class Manifest(object):
  """Class which provides an abstraction for manipulating the local manifest."""

  @classmethod
  def FromPath(cls, path, empty_if_missing=False):
    if os.path.isfile(path):
      with open(path) as f:
        return cls(f.read())
    elif empty_if_missing:
      cros_build_lib.Die('Manifest file, %r, not found' % path)
    return cls()

  def __init__(self, text=None):
    self._text = text or '<manifest>\n</manifest>'
    self.nodes = ElementTree.fromstring(self._text)

  def AddNonWorkonProject(self, name, path, remote=None, revision=None):
    """Add a new nonworkon project element to the manifest tree."""
    element = ElementTree.Element('project', name=name, path=path,
                                  remote=remote)
    element.attrib['workon'] = 'False'
    if revision is not None:
      element.attrib['revision'] = revision
    self.nodes.append(element)
    return element

  def GetProject(self, name, path=None):
    """Accessor method for getting a project node from the manifest tree.

    Returns:
      project element node from ElementTree, otherwise, None
    """
    if path is None:
      # Use a unique value that can't ever match.
      path = object()
    for project in self.nodes.findall('project'):
      if project.attrib['name'] == name or project.attrib['path'] == path:
        return project
    return None

  def ToString(self):
    # Reset the tail for each node, then just do a hacky replace.
    project = None
    for project in self.nodes.findall('project'):
      project.tail = '\n  '
    if project is not None:
      # Tweak the last project to not have the trailing space.
      project.tail = '\n'
    # Fix manifest tag text and tail.
    self.nodes.text = '\n  '
    self.nodes.tail = '\n'
    return ElementTree.tostring(self.nodes)

  def GetProjects(self):
    return list(self.nodes.findall('project'))


def _AddProjectsToManifestGroups(options, *projects):
  """Enable the given manifest groups for the configured repository."""

  groups_to_enable = ['name:%s' % x for x in projects]

  git_config = options.git_config

  enabled_groups = cros_build_lib.RunCommandCaptureOutput(
      ['git', 'config', '-f', git_config, '--get', 'manifest.groups'],
      error_code_ok=True, print_cmd=False).output.split(',')

  # Note that ordering actually matters, thus why the following code
  # is written this way.
  # Per repo behaviour, enforce an appropriate platform group if
  # we're converting from a default manifest group to a limited one.
  # Finally, note we reprocess the existing groups; this is to allow
  # us to cleanup any user screwups, or our own screwups.
  requested_groups = (
      ['minilayout', 'platform-%s' % (platform.system().lower(),)] +
      enabled_groups + list(groups_to_enable))

  processed_groups = set()
  finalized_groups = []

  for group in requested_groups:
    if group not in processed_groups:
      finalized_groups.append(group)
      processed_groups.add(group)

  cros_build_lib.RunCommandCaptureOutput(
      ['git', 'config', '-f', git_config, 'manifest.groups',
       ','.join(finalized_groups)], print_cmd=False)


def _UpgradeMinilayout(options):
  """Convert a repo checkout away from minilayout.xml to default.xml."""

  full_tree = Manifest.FromPath(options.default_manifest_path)
  local_manifest_exists = os.path.exists(options.local_manifest_path)

  new_groups = []
  if local_manifest_exists:
    local_tree = Manifest.FromPath(options.local_manifest_path)
    # Identify which projects need to be transferred across.
    projects = local_tree.GetProjects()
    new_groups = [x.attrib['name'] for x in projects]
    allowed = set(x.attrib['name'] for x in full_tree.GetProjects())
    transferred = [x for x in projects if x.attrib['name'] in allowed]
    for project in transferred:
      # Mangle local_manifest object, removing those projects;
      # note we'll still be adding those projects to the default groups,
      # including those that didn't intersect the main manifest.
      local_tree.nodes.remove(project)

  _AddProjectsToManifestGroups(options, *new_groups)

  if local_manifest_exists:
    # Rewrite the local_manifest now; if there is no settings left in
    # the local_manifest, wipe it.
    if local_tree.nodes.getchildren():
      with open(options.local_manifest_path, 'w') as f:
        f.write(local_tree.ToString())
    else:
      os.unlink(options.local_manifest_path)

  # Finally, move the symlink.
  os.unlink(options.manifest_sym_path)
  os.symlink('manifests/default.xml', options.manifest_sym_path)
  logging.info("Converted the checkout to manifest groups based minilayout.")


def main(argv):
  parser = optparse.OptionParser(usage='usage: %prog add [options] <name> '
                                       '<--workon | <path> --remote <remote> >')
  parser.add_option('-w', '--workon', action='store_true', dest='workon',
                    default=False, help='Is this a workon package?')
  parser.add_option('-r', '--remote', dest='remote',
                    default=None)
  parser.add_option('-v', '--revision', dest='revision',
                    default=None,
                    help="Use to override the manifest defined default "
                    "revision used for a given project.")
  parser.add_option('--upgrade-minilayout', default=False, action='store_true',
                    help="Upgrade a minilayout checkout into a full.xml "
                    "checkout utilizing manifest groups.")
  (options, args) = parser.parse_args(argv)

  repo_dir = git.FindRepoDir(os.getcwd())
  if not repo_dir:
    parser.error("This script must be invoked from within a repository "
                 "checkout.")

  options.git_config = os.path.join(repo_dir, 'manifests.git', 'config')
  options.repo_dir = repo_dir
  options.local_manifest_path = os.path.join(repo_dir, 'local_manifest.xml')
  # This constant is used only when we're doing an upgrade away from
  # minilayout.xml to default.xml.
  options.default_manifest_path = os.path.join(repo_dir, 'manifests',
                                               'default.xml')
  options.manifest_sym_path = os.path.join(repo_dir, 'manifest.xml')

  active_manifest = os.path.basename(os.readlink(options.manifest_sym_path))
  upgrade_required = active_manifest == 'minilayout.xml'

  if options.upgrade_minilayout:
    if args:
      parser.error("--upgrade-minilayout takes no arguments.")
    if not upgrade_required:
      print "This repository checkout isn't using minilayout.xml; nothing to do"
    else:
      _UpgradeMinilayout(options)
    return 0
  elif upgrade_required:
    logging.warn(
        "Your repository checkout is using the old minilayout.xml workflow; "
        "auto-upgrading it.")
    cros_build_lib.RunCommand(
        [sys.argv[0], '--upgrade-minilayout'], cwd=os.getcwd(),
         print_cmd=False)

  if not args:
    parser.error("No command specified.")
  elif args[0] != 'add':
    parser.error("Only supported subcommand is add right now.")
  elif options.workon:
    if len(args) != 2:
      parser.error(
          "Argument count is wrong for --workon; must be add <project>")
    name, path = args[1], None
  else:
    if options.remote is None:
      parser.error('Adding non-workon projects requires a remote.')
    elif len(args) != 3:
      parser.error(
          "Argument count is wrong for non-workon mode; "
          "must be add <project> <path> --remote <remote-arg>")
    name, path = args[1:]

  revision = options.revision
  if revision is not None:
    if (not git.IsRefsTags(revision) and
        not git.IsSHA1(revision)):
      revision = git.StripRefsHeads(revision, False)

  main_manifest = Manifest.FromPath(options.manifest_sym_path,
                                    empty_if_missing=False)
  local_manifest = Manifest.FromPath(options.local_manifest_path)

  main_element = main_manifest.GetProject(name, path=path)

  if options.workon:
    if main_element is None:
      parser.error('No project named %r in the default manifest.' % name)
    _AddProjectsToManifestGroups(options, main_element.attrib['name'])

  elif main_element is not None:
    if options.remote is not None:
      # Likely this project wasn't meant to be remote, so workon main element
      print "Project already exists in manifest. Using that as workon project."
      _AddProjectsToManifestGroups(options, main_element.attrib['name'])
    else:
      # Conflict will occur; complain.
      parser.error("Requested project name=%r path=%r will conflict with "
                   "your current manifest %s" % (name, path, active_manifest))

  elif local_manifest.GetProject(name, path=path) is not None:
    parser.error("Requested project name=%r path=%r conflicts with "
                 "your local_manifest.xml" % (name, path))

  else:
    element = local_manifest.AddNonWorkonProject(name=name, path=path,
                                                 remote=options.remote,
                                                 revision=revision)
    _AddProjectsToManifestGroups(options, element.attrib['name'])

    with open(options.local_manifest_path, 'w') as f:
      f.write(local_manifest.ToString())
  return 0
