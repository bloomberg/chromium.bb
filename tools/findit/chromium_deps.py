# Copyright (c) 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import base64
import json
import os
import time
import urllib2

from common import utils


_THIS_DIR = os.path.abspath(os.path.dirname(__file__))
CONFIG = json.loads(open(os.path.join(_THIS_DIR,
                                      'deps_config.json'), 'r').read())


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


def _GetComponentName(path, host_dirs):
  """Return the component name of a path."""
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


def _GetContentOfDEPS(revision, retries=5, sleep_time=0.1):
  chromium_git_file_url_template = CONFIG['chromium_git_file_url']

  deps_file_name = '.DEPS.git'
  count = 0
  while True:
    count += 1

    url = chromium_git_file_url_template % (revision, deps_file_name)
    http_status_code, content = utils.GetHttpClient().Get(url, timeout=60)

    if http_status_code == 404 and deps_file_name != 'DEPS':
      deps_file_name = 'DEPS'
      count = 0
      continue
    elif http_status_code == 200:
      # Googlesource git returns text file encoded in base64, so decode it.
      return base64.b64decode(content)

    if count < retries:
      time.sleep(sleep_time)
    else:
      break

  return ''


def GetChromiumComponents(chromium_revision,
                          os_platform='unix',
                          deps_file_downloader=_GetContentOfDEPS):
  """Return a list of components used by Chrome of the given revision.

  Args:
    chromium_revision: Revision of the Chrome build: svn revision, or git hash.
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

  chromium_git_base_url = CONFIG['chromium_git_base_url']

  if not utils.IsGitHash(chromium_revision):
    # Convert svn revision or commit position to Git hash.
    cr_rev_url_template = CONFIG['cr_rev_url']
    url = cr_rev_url_template % chromium_revision
    # TODO(stgao): Add retry in HttpClient.
    _, content = utils.GetHttpClient().Get(url, timeout=60)
    cr_rev_data = json.loads(content)
    if 'git_sha' not in cr_rev_data:
      raise Exception('Failed to convert svn revision to git hash')
    chromium_revision = cr_rev_data['git_sha']

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
  host_dirs = CONFIG['host_directories']
  for component_path, component_repo_url in all_deps.iteritems():
    if component_repo_url is None:
      # For some platform like iso, some component is ignored.
      continue

    name = _GetComponentName(component_path, host_dirs)
    repository, revision = component_repo_url.split('@')
    is_git_hash = utils.IsGitHash(revision)
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
  components['src/'] = {
      'path': 'src/',
      'name': 'chromium',
      'repository': chromium_git_base_url,
      'repository_type': 'git',
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
