#!/usr/bin/python
# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Script that resets your Chrome GIT checkout."""

import optparse
import os
import re

from chromite.buildbot import repository
from chromite.lib import cros_build_lib as cros_lib


_CHROMIUM_ROOT = 'chromium'
_CHROMIUM_SRC_ROOT = os.path.join(_CHROMIUM_ROOT, 'src')
_CHROMIUM_SRC_INTERNAL = os.path.join(_CHROMIUM_ROOT, 'src-internal')
_CHROMIUM_CROS_DEPS = os.path.join(_CHROMIUM_SRC_ROOT, 'tools/cros.DEPS/DEPS')


def _LoadDEPS(deps_content):
  """Load contents of DEPS file into a dictionary.

  Arguments:
    deps_content: The contents of the .DEPS.git file.

  Returns:
    A dictionary indexed by the top level items in the structure - i.e.,
    'hooks', 'deps', 'os_deps'.
  """
  class FromImpl:
    """From syntax is not supported."""
    def __init__(self, module_name, var_name):
      raise NotImplementedError('The From() syntax is not supported in'
                                'chrome_set_ver.')

  class _VarImpl:
    def __init__(self, custom_vars, local_scope):
      self._custom_vars = custom_vars
      self._local_scope = local_scope

    def Lookup(self, var_name):
      """Implements the Var syntax."""
      if var_name in self._custom_vars:
        return self._custom_vars[var_name]
      elif var_name in self._local_scope.get('vars', {}):
        return self._local_scope['vars'][var_name]
      raise Exception('Var is not defined: %s' % var_name)

  locals = {}
  var = _VarImpl({}, locals)
  globals = {'From': FromImpl, 'Var': var.Lookup, 'deps_os': {}}
  exec(deps_content) in globals, locals
  return locals


def _CreateCrosSymlink(repo_root):
  """Create symlinks to CrOS projects specified in the cros_DEPS/DEPS file."""
  cros_deps_file = os.path.join(repo_root, _CHROMIUM_CROS_DEPS)
  _, merged_deps = GetParsedDeps(cros_deps_file)
  chromium_root = os.path.join(repo_root, _CHROMIUM_ROOT)

  mappings = GetPathToProjectMappings(merged_deps)
  for rel_path, project in mappings.iteritems():
    link_dir = os.path.join(chromium_root, rel_path)
    target_dir = os.path.join(repo_root,
                              cros_lib.GetProjectDir(repo_root, project))
    path_to_target = os.path.relpath(target_dir, os.path.dirname(link_dir))
    if not os.path.exists(link_dir):
      os.symlink(path_to_target, link_dir)


def _ExtractProjectFromUrl(repo_url):
  """Get the Gerrit project name from an url."""
  # Example: 'ssh://gerrit-int.chromium.org:12121/my/project.git' would parse to
  # 'my/project'.  See unit test for more examples.  For 'URL's like
  # '../abc/efg' leave as-is to support unit tests.
  mo = re.match(r'(?:[\w\+]+://[^/]+/)?(.*)', repo_url)
  return os.path.splitext(mo.group(1))[0]


def _ExtractProjectFromEntry(entry):
  """From a deps entry extract the Gerrit project name.

  Arguments:
    entry: The deps entry in the format ${url_prefix}/${project_name}@${hash}.
  """
  # We only support Gerrit urls, where the path is the project name.
  repo_url = entry.partition('@')[0]
  return _ExtractProjectFromUrl(repo_url)


def GetPathToProjectMappings(deps):
  """Get dictionary relating path to Gerrit project names.

  Arguments:
   deps: a dictionary indexed by repo paths.  The same format as the 'deps'
         entry in the '.DEPS.git' file.
  """
  mappings = {}
  for rel_path, entry in deps.iteritems():
    mappings[rel_path] = _ExtractProjectFromEntry(entry)

  return mappings


class ProjectException(Exception):
  """Raised by Project class when a Pin error """
  pass


class Project(object):
  """Encapsulates functionality to pin a project to a specific commit."""
  def __init__(self, repo_root, project_url, rel_path):
    """Construct the class.

    Arguments:
      repo_root: The root of the repo checkout.
      project_url: The Gerrit url of the project.
      rel_path: The path the project is expected to be checked out to.  Relative
                to <repo_root>/chromium.
    """
    self.repo_root = repo_root
    self.chromium_root = os.path.join(self.repo_root, _CHROMIUM_ROOT)
    self.project_url = project_url
    self.rel_path = rel_path
    self.abs_path = os.path.join(self.chromium_root, self.rel_path)
    self.manifest_rel_path = os.path.join(_CHROMIUM_ROOT, self.rel_path)
    self.project_name = _ExtractProjectFromUrl(self.project_url)

  def _ResetProject(self, commit_hash):
    """Actually pin project to the specified commit hash."""
    if not cros_lib.DoesCommitExistInRepo(self.abs_path, commit_hash):
      cros_lib.Die('Commit %s not found in %s.\n'
                   "You probably need to run 'repo sync --jobs=<jobs>' "
                   'to update your checkout.'
                   % (commit_hash, self.abs_path))

    result = cros_lib.RunCommand(['git', 'checkout', commit_hash],
                                 error_code_ok=True, cwd=self.abs_path)
    if result.returncode != 0:
      cros_lib.Warning('Failed to pin project %s.\n'
                       'You probably have uncommited local changes.'
                       % self.abs_path)

  def _PrepareProject(self):
    """Make sure the project is synced properly and is ready for pinning."""
    handler = cros_lib.ParseFullManifest(self.repo_root)
    path_to_project_dict = dict(([attrs['path'], project]) for project, attrs
                                in handler.projects.iteritems())

    # TODO(rcui): Handle case where a dependency never makes it to the manifest
    # (i.e., dep path added as double checkout, and then gets deleted). We need
    # to delete those.  crosbug/22123.
    if not cros_lib.IsDirectoryAGitRepoRoot(self.abs_path):
      if self.manifest_rel_path in path_to_project_dict:
        raise ProjectException('%s in full layout manifest but not in working '
                               "tree. Please run 'repo sync %s'"
                               % (self.manifest_rel_path,
                                  path_to_project_dict[self.manifest_rel_path]))
      else:
        cros_lib.Warning('Project %s is not in the manifest.  Automatically '
                         'checking out to %s.\n'
                         % (self.project_url, self.abs_path))
        repository.CloneGitRepo(self.abs_path, self.project_url)
        cros_lib.RunCommand(['git', 'checkout',
                            cros_lib.GetGitRepoRevision(self.abs_path)],
                            cwd=self.abs_path)
    elif not cros_lib.IsProjectManagedByRepo(self.abs_path):
      if self.manifest_rel_path in path_to_project_dict:
        # If path is now in the manifest, tell user to manually delete our
        # managed checkout and re-sync.
        raise ProjectException('%s needs to be replaced.  Please remove the '
                               "directory and run 'repo sync %s'"
                               % (self.manifest_rel_path,
                                  path_to_project_dict[self.manifest_rel_path]))
      else:
        # If not managed by Repo we need to perform sync.
        cros_lib.RunCommand(['git', 'pull', '--rebase', self.project_url],
                            cwd=self.abs_path)
    elif not os.path.islink(self.abs_path):
      # Skip symlinks - we don't want to error out for the cros.DEPS projects.
      if self.manifest_rel_path not in path_to_project_dict:
        # If it is 'managed by repo' but not in the manifest, repo tried
        # deleting it but failed because of local changes.
        raise ProjectException('%s is no longer in the manifest but has local '
                               'changes.  Please remove and try again.'
                               % self.manifest_rel_path)
      elif self.project_name != path_to_project_dict[self.manifest_rel_path]:
        cros_lib.Die('.DEPS.git for %s conflicts with manifest.xml!  Running '
                     'with older .DEPS.git files are not yet supported.  '
                     "Please run'repo sync --jobs=<jobs>' to sync everything "
                     'up.' % self.manifest_rel_path)

  def Pin(self, commit_hash):
    """Attempt to pin the project to the specified commit hash.

    Arguments:
      commit_hash: The commit to pin the project to.

    Raises:
      ProjectException when an error occurs.
    """
    self._PrepareProject()
    if cros_lib.GetCurrentBranch(self.abs_path):
      cros_lib.Warning("Not pinning project %s that's checked out to a "
                       'development branch.' % self.rel_path)
    elif (commit_hash and
          (commit_hash != cros_lib.GetGitRepoRevision(self.abs_path))):
      print 'Pinning project %s' % self.rel_path
      self._ResetProject(commit_hash)
    else:
      cros_lib.Debug('Skipping project %s, already pinned' % self.rel_path)


def _ResetGitCheckout(repo_root, deps):
  """Reset chrome repos to hashes specified in the DEPS file.

  Arguments:
   chromium_root: directory where chromium is checked out - level above 'src'.
   deps: a dictionary indexed by repo paths.  The same format as the 'deps'
         entry in the '.DEPS.git' file.
  """
  errors = []
  for rel_path, project_hash in deps.iteritems():
    repo_url, _, commit_hash = project_hash.partition('@')
    project = Project(repo_root, repo_url, rel_path)
    try:
      project.Pin(commit_hash)
    except ProjectException as e:
      errors.append(str(e))

  if errors:
    cros_lib.Die('Errors encountered:\n* ' + '\n* '.join(errors))


def _RunHooks(chromium_root, hooks):
  """Run the hooks contained in the DEPS file.

  Arguments:
    chromium_root: directory where chromium is checked out - level above 'src'.
    hooks: a list of hook dictionaries.  The same format as the 'hooks' entry
           in the '.DEPS.git' file.
  """
  for hook in hooks:
    print '[running hook] %s' % ' '.join(hook['action'])
    cros_lib.RunCommand(hook['action'], cwd=chromium_root)


def _MergeDeps(dest, update):
  """Merge the dependencies specified in two dictionaries.

  Arguments:
    dest: The dictionary that will be merged into.
    update: The dictionary whose elements will be merged into dest.
  """
  assert(not set(dest.keys()).intersection(set(update.keys())))
  dest.update(update)
  return dest


def GetParsedDeps(deps_file):
  """Returns the full parsed DEPS file dictionary, and merged deps.

  Arguments:
    deps_file: Path to the .DEPS.git file.

  Returns:
    An (x,y) tuple.  x is a dictionary containing the contents of the DEPS file,
    and y is a dictionary containing the result of merging unix and common deps.
  """
  with open(deps_file, 'rU') as f:
    deps = _LoadDEPS(f.read())

  merged_deps = deps.get('deps', {})
  unix_deps = deps.get('deps_os', {}).get('unix', {})
  merged_deps = _MergeDeps(merged_deps, unix_deps)
  return deps, merged_deps


def main(argv):

  usage = 'usage: %prog [-d <DEPS.git file>] [command]'
  parser = optparse.OptionParser(usage=usage)

  # TODO(rcui): have -d accept a URL.
  parser.add_option('-d', '--deps', default=None,
                    help=('DEPS file to use. Defaults to '
                         '<chrome_src_root>/.DEPS.git'))
  parser.add_option('--internal', action='store_false', dest='internal',
                    default=True, help='Allow chrome-internal URLs')
  parser.add_option('--runhooks', action='store_true', dest='runhooks',
                    default=False, help="Run hooks as well.")
  parser.add_option('-v', '--verbose', default=False, action='store_true',
                    help='Run with debug output.')
  (options, _inputs) = parser.parse_args(argv)

  # Set cros_build_lib debug level to hide RunCommand spew.
  if options.verbose:
    cros_lib.DebugLevel.SetDebugLevel(cros_lib.DebugLevel.DEBUG)
  else:
    cros_lib.DebugLevel.SetDebugLevel(cros_lib.DebugLevel.WARNING)

  if cros_lib.IsInsideChroot():
    ssh_path = '/usr/bin/ssh_no_update'
    if os.path.isfile(ssh_path):
      os.environ['GIT_SSH'] = ssh_path
    else:
      cros_lib.Warning("Can't find %s.  Run build_packages or setup_board to "
                       'update your chooot.' % ssh_path)

  repo_root = cros_lib.FindRepoCheckoutRoot()
  chromium_src_root = os.path.join(repo_root, _CHROMIUM_SRC_ROOT)
  if not os.path.isdir(chromium_src_root):
    cros_lib.Die('chromium src/ dir not found')

  # Add DEPS files to parse.
  deps_files_to_parse = []
  if options.deps:
    deps_files_to_parse.append(options.deps)
  else:
    deps_files_to_parse.append(os.path.join(chromium_src_root, '.DEPS.git'))

  internal_deps = os.path.join(repo_root, _CHROMIUM_SRC_INTERNAL, '.DEPS.git')
  if os.path.exists(internal_deps):
    deps_files_to_parse.append(internal_deps)

  deps_files_to_parse.append(os.path.join(repo_root, _CHROMIUM_CROS_DEPS))

  # Prepare source tree for resetting.
  chromium_root = os.path.join(repo_root, _CHROMIUM_ROOT)
  _CreateCrosSymlink(repo_root)

  # Parse DEPS files and store hooks.
  hook_dicts = []
  for deps_file in deps_files_to_parse:
    deps, merged_deps = GetParsedDeps(deps_file)
    _ResetGitCheckout(repo_root, merged_deps)
    hook_dicts.append(deps.get('hooks', {}))

  # Run hooks after checkout has been reset properly.
  if options.runhooks:
    for hooks in hook_dicts:
      _RunHooks(chromium_root, hooks)
