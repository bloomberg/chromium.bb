# Copyright (c) 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import https
import utils


DEPS_FILE_URL = 'https://src.chromium.org/chrome/trunk/src/DEPS?p=%s'


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
  return https.SendRequest(DEPS_FILE_URL % chromium_revision)


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
  """
  if os_platform.lower() == 'linux':
    os_platform = 'unix'

  # Download the content of DEPS file in chromium.
  deps_content = deps_file_downloader(chromium_revision)

  all_deps = {}

  # Parse the content of DEPS file.
  deps, deps_os = _ParseDEPS(deps_content)
  all_deps.update(deps)
  if os_platform is not None:
    all_deps.update(deps_os.get(os_platform, {}))

  # Figure out components based on the dependencies.
  components = {}
  for component_path in all_deps.keys():
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

  # Add chromium as a component.
  # TODO(stgao): Move to git.
  components['src/'] = {
      'path': 'src/',
      'name': 'chromium',
      'repository': 'https://src.chromium.org/chrome/trunk',
      'repository_type': 'svn',
      'revision': chromium_revision
  }

  return components


def GetChromiumComponentRange(chromium_revision1,
                              chromium_revision2,
                              os_platform='unix',
                              deps_file_downloader=_GetContentOfDEPS):
  """Return a list of components with their revision ranges.

  Args:
    chromium_revision1: The revision of a Chrome build.
    chromium_revision2: The revision of another Chrome build.
    os_platform: The target platform of the Chrome build, eg. win, mac, etc.
    deps_file_downloader: A function that takes the chromium_revision as input,
                          and returns the content of the DEPS file. The returned
                          content is assumed to be trusted input and will be
                          evaluated as python code.
  """
  # TODO(stgao): support git.
  chromium_revision1 = int(chromium_revision1)
  chromium_revision2 = int(chromium_revision2)
  old_revision = str(min(chromium_revision1, chromium_revision2))
  new_revision = str(max(chromium_revision1, chromium_revision2))

  old_components = GetChromiumComponents(old_revision, os_platform,
                                         deps_file_downloader)
  new_components = GetChromiumComponents(new_revision, os_platform,
                                         deps_file_downloader)

  components = {}
  for path in new_components.keys():
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
  print json.dumps(GetChromiumComponents(284750), sort_keys=True, indent=2)
