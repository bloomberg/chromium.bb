# Copyright (c) 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import base64
import https
import utils


DEPS_FILE_URL_SVN = 'https://src.chromium.org/chrome/trunk/src/DEPS?p=%s'
DEPS_FILE_URL_GIT = (
    'https://chromium.googlesource.com/chromium/src/+/%s/DEPS?format=text')


class _VarImpl(object):

  def __init__(self, local_scope):
    self._local_scope = local_scope

  def Lookup(self, var_name):
    if var_name in self._local_scope.get('vars', {}):
      return self._local_scope['vars'][var_name]
    raise Exception('Var is not defined: %s' % var_name)


def _ParseDEPS(content):
  """Parse the DEPS file of chromium."""
  local_scope = {}
  var = _VarImpl(local_scope)
  global_scope = {
      'Var': var.Lookup,
      'deps': {},
      'deps_os': {},
      'include_rules': [],
      'skip_child_includes': [],
      'hooks': [],
  }
  exec(content, global_scope, local_scope)

  local_scope.setdefault('deps', {})
  local_scope.setdefault('deps_os', {})

  return (local_scope['deps'], local_scope['deps_os'])


def _GetComponentName(path):
  """Return the component name of a path."""
  host_dirs = [
      'src/chrome/browser/resources/',
      'src/chrome/test/data/layout_tests/',
      'src/media/',
      'src/sdch/',
      'src/testing/',
      'src/third_party/WebKit/',
      'src/third_party/',
      'src/tools/',
      'src/',
  ]
  components_renamed = {
      'webkit': 'blink',
  }

  for host_dir in host_dirs:
    if path.startswith(host_dir):
      path = path[len(host_dir):]
      name = path.split('/')[0].lower()
      if name in components_renamed:
        return components_renamed[name].lower()
      else:
        return name.lower()

  # Unknown path, return the whole path as component name.
  return '_'.join(path.split('/'))


def _GetContentOfDEPS(chromium_revision):
  if utils.IsGitHash(chromium_revision):
    url = DEPS_FILE_URL_GIT
  else:
    url = DEPS_FILE_URL_SVN
  return https.SendRequest(url % chromium_revision)


def GetChromiumComponents(chromium_revision,
                          os_platform='unix',
                          deps_file_downloader=_GetContentOfDEPS):
  """Return a list of components used by Chrome of the given revision.

  Args:
    chromium_revision: The revision of the Chrome build.
    os_platform: The target platform of the Chrome build, eg. win, mac, etc.
    deps_file_downloader: A function that takes the chromium_revision as input,
                          and returns the content of the DEPS file. The returned
                          content is assumed to be trusted input and will be
                          evaluated as python code.

  Returns:
    A map from component path to parsed component name, repository URL,
    repository type and revision.
  """
  if os_platform.lower() == 'linux':
    os_platform = 'unix'

  is_git_hash = utils.IsGitHash(chromium_revision)

  # Download the content of DEPS file in chromium.
  deps_content = deps_file_downloader(chromium_revision)

  # Googlesource git returns text file encoded in base64, so decode it.
  if is_git_hash:
    deps_content = base64.b64decode(deps_content)

  all_deps = {}

  # Parse the content of DEPS file.
  deps, deps_os = _ParseDEPS(deps_content)
  all_deps.update(deps)
  if os_platform is not None:
    all_deps.update(deps_os.get(os_platform, {}))

  # Figure out components based on the dependencies.
  components = {}
  for component_path in all_deps:
    name = _GetComponentName(component_path)
    repository, revision = all_deps[component_path].split('@')
    is_git_hash = utils.IsGitHash(revision)
    if repository.startswith('/'):
      # TODO(stgao): Use git repo after chromium moves to git.
      repository = 'https://src.chromium.org/chrome%s' % repository
    if is_git_hash:
      repository_type = 'git'
    else:
      repository_type = 'svn'
    if not component_path.endswith('/'):
      component_path += '/'
    components[component_path] = {
        'path': component_path,
        'name': name,
        'repository': repository,
        'repository_type': repository_type,
        'revision': revision
    }

  # Add chromium as a component, depending on the repository type.
  if is_git_hash:
    repository = 'https://chromium.googlesource.com/chromium/src/'
    repository_type = 'git'
  else:
    repository = 'https://src.chromium.org/chrome/trunk'
    repository_type = 'svn'

  components['src/'] = {
      'path': 'src/',
      'name': 'chromium',
      'repository': repository,
      'repository_type': repository_type,
      'revision': chromium_revision
  }

  return components


def GetChromiumComponentRange(old_revision,
                              new_revision,
                              os_platform='unix',
                              deps_file_downloader=_GetContentOfDEPS):
  """Return a list of components with their revision ranges.

  Args:
    old_revision: The old revision of a Chrome build.
    new_revision: The new revision of a Chrome build.
    os_platform: The target platform of the Chrome build, eg. win, mac, etc.
    deps_file_downloader: A function that takes the chromium_revision as input,
                          and returns the content of the DEPS file. The returned
                          content is assumed to be trusted input and will be
                          evaluated as python code.

  Returns:
    A map from component path to its parsed regression and other information.
  """
  # Assume first revision is the old revision.
  old_components = GetChromiumComponents(old_revision, os_platform,
                                         deps_file_downloader)
  new_components = GetChromiumComponents(new_revision, os_platform,
                                         deps_file_downloader)

  components = {}
  for path in new_components:
    new_component = new_components[path]
    old_revision = None
    if path in old_components:
      old_component = old_components[path]
      old_revision = old_component['revision']
    components[path] = {
        'path': path,
        'rolled': new_component['revision'] != old_revision,
        'name': new_component['name'],
        'old_revision': old_revision,
        'new_revision': new_component['revision'],
        'repository': new_component['repository'],
        'repository_type': new_component['repository_type']
    }

  return components


if __name__ == '__main__':
  import json
  print json.dumps(GetChromiumComponents(
      'b4b1aea80b25a3b2f7952c9d95585e880421ef2b'), sort_keys=True, indent=2)
