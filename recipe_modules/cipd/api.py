# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from recipe_engine import recipe_api


class CIPDApi(recipe_api.RecipeApi):
  """CIPDApi provides support for CIPD."""
  def __init__(self, *args, **kwargs):
    super(CIPDApi, self).__init__(*args, **kwargs)
    self._cipd_executable = None
    self._cipd_version = None
    self._cipd_credentials = None

  def set_service_account_credentials(self, path):
    self._cipd_credentials = path

  @property
  def default_bot_service_account_credentials(self):
    # Path to a service account credentials to use to talk to CIPD backend.
    # Deployed by Puppet.
    if self.m.platform.is_win:
      return 'C:\\creds\\service_accounts\\service-account-cipd-builder.json'
    else:
      return '/creds/service_accounts/service-account-cipd-builder.json'

  def platform_suffix(self):
    """Use to get full package name that is platform indepdent.

    Example:
      >>> 'my/package/%s' % api.cipd.platform_suffix()
      'my/package/linux-amd64'
    """
    return '%s-%s' % (
        self.m.platform.name.replace('win', 'windows'),
        {
            32: '386',
            64: 'amd64',
        }[self.m.platform.bits],
    )

  def install_client(self, step_name='install cipd', version=None):
    """Ensures the client is installed.

    If you specify version as a hash, make sure its correct platform.
    """
    # TODO(seanmccullough): clean up older CIPD installations.
    step = self.m.python(
        name=step_name,
        script=self.resource('bootstrap.py'),
        args=[
          '--platform', self.platform_suffix(),
          '--dest-directory', self.m.path['start_dir'].join('cipd'),
          '--json-output', self.m.json.output(),
        ] +
        (['--version', version] if version else []),
        step_test_data=lambda: self.test_api.example_install_client(version)
    )
    self._cipd_executable = step.json.output['executable']

    step.presentation.step_text = (
        'cipd instance_id: %s' % step.json.output['instance_id'])
    return step

  def get_executable(self):
    return self._cipd_executable

  def build(self, input_dir, output_package, package_name, install_mode=None):
    assert self._cipd_executable
    assert not install_mode or install_mode in ['copy', 'symlink']
    return self.m.step(
        'build %s' % self.m.path.basename(package_name),
        [
          self._cipd_executable,
          'pkg-build',
          '--in', input_dir,
          '--name', package_name,
          '--out', output_package,
          '--json-output', self.m.json.output(),
        ] + (
          ['--install-mode', install_mode] if install_mode else []
        ),
        step_test_data=lambda: self.test_api.example_build(package_name)
    )

  def register(self, package_name, package_path, refs=None, tags=None):
    assert self._cipd_executable

    cmd = [
      self._cipd_executable,
      'pkg-register', package_path,
      '--json-output', self.m.json.output(),
    ]
    if self._cipd_credentials:
      cmd.extend(['--service-account-json', self._cipd_credentials])
    if refs:
      for ref in refs:
        cmd.extend(['--ref', ref])
    if tags:
      for tag, value in sorted(tags.items()):
        cmd.extend(['--tag', '%s:%s' % (tag, value)])
    return self.m.step(
        'register %s' % package_name,
        cmd,
        step_test_data=lambda: self.test_api.example_register(package_name)
    )

  def create(self, pkg_def, refs=None, tags=None):
    """Creates a package based on YAML package definition file.

    This builds and uploads the package in one step.
    """
    assert self._cipd_executable
    cmd = [
      self._cipd_executable,
      'create',
      '--pkg-def', pkg_def,
      '--json-output', self.m.json.output(),
    ]
    if self._cipd_credentials:
      cmd.extend(['--service-account-json', self._cipd_credentials])
    if refs:
      for ref in refs:
        cmd.extend(['--ref', ref])
    if tags:
      for tag, value in sorted(tags.items()):
        cmd.extend(['--tag', '%s:%s' % (tag, value)])
    return self.m.step('create %s' % self.m.path.basename(pkg_def), cmd)

  def ensure(self, root, packages):
    """Ensures that packages are installed in a given root dir.

    packages must be a mapping from package name to its version, where
      * name must be for right platform (see also ``platform_suffix``),
      * version could be either instance_id, or ref, or unique tag.

    If installing a package requires credentials, call
    ``set_service_account_credentials`` before calling this function.
    """
    assert self._cipd_executable

    package_list = ['%s %s' % (name, version)
                    for name, version in sorted(packages.items())]
    list_data = self.m.raw_io.input('\n'.join(package_list))
    cmd = [
      self._cipd_executable,
      'ensure',
      '--root', root,
      '--list', list_data,
      '--json-output', self.m.json.output(),
    ]
    if self._cipd_credentials:
      cmd.extend(['--service-account-json', self._cipd_credentials])
    return self.m.step(
        'ensure_installed', cmd,
        step_test_data=lambda: self.test_api.example_ensure(packages)
    )

  def set_tag(self, package_name, version, tags):
    assert self._cipd_executable

    cmd = [
      self._cipd_executable,
      'set-tag', package_name,
      '--version', version,
      '--json-output', self.m.json.output(),
    ]
    if self._cipd_credentials:
      cmd.extend(['--service-account-json', self._cipd_credentials])
    for tag, value in sorted(tags.items()):
      cmd.extend(['--tag', '%s:%s' % (tag, value)])

    return self.m.step(
      'cipd set-tag %s' % package_name,
      cmd,
      step_test_data=lambda: self.test_api.example_set_tag(
          package_name, version
      )
    )

  def set_ref(self, package_name, version, refs):
    assert self._cipd_executable

    cmd = [
      self._cipd_executable,
      'set-ref', package_name,
      '--version', version,
      '--json-output', self.m.json.output(),
    ]
    if self._cipd_credentials:
      cmd.extend(['--service-account-json', self._cipd_credentials])
    for r in refs:
      cmd.extend(['--ref', r])

    return self.m.step(
      'cipd set-ref %s' % package_name,
      cmd,
      step_test_data=lambda: self.test_api.example_set_ref(
          package_name, version
      )
    )

  def search(self, package_name, tag):
    assert self._cipd_executable
    assert ':' in tag, 'tag must be in a form "k:v"'

    cmd = [
      self._cipd_executable,
      'search', package_name,
      '--tag', tag,
      '--json-output', self.m.json.output(),
    ]
    if self._cipd_credentials:
      cmd.extend(['--service-account-json', self._cipd_credentials])

    return self.m.step(
      'cipd search %s %s' % (package_name, tag),
      cmd,
      step_test_data=lambda: self.test_api.example_search(package_name)
    )

  def describe(self, package_name, version,
               test_data_refs=None, test_data_tags=None):
    assert self._cipd_executable

    cmd = [
      self._cipd_executable,
      'describe', package_name,
      '--version', version,
      '--json-output', self.m.json.output(),
    ]
    if self._cipd_credentials:
      cmd.extend(['--service-account-json', self._cipd_credentials])

    return self.m.step(
      'cipd describe %s' % package_name,
      cmd,
      step_test_data=lambda: self.test_api.example_describe(
          package_name, version,
          test_data_refs=test_data_refs,
          test_data_tags=test_data_tags
      )
    )
