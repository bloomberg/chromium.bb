# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

DEPS = [
  'recipe_engine/path',
  'recipe_engine/platform',
  'recipe_engine/properties',
  'recipe_engine/step',
  'cipd',
]

def RunSteps(api):
  # First, you need a cipd client.
  api.cipd.install_client('install cipd')
  api.cipd.install_client('install cipd', version='deadbeaf')
  assert api.cipd.get_executable()

  # Need to set service account credentials.
  api.cipd.set_service_account_credentials(
      api.cipd.default_bot_service_account_credentials)

  package_name = 'public/package/%s' % api.cipd.platform_suffix()
  package_instance_id = '7f751b2237df2fdf3c1405be00590fefffbaea2d'
  packages = {package_name: package_instance_id}

  cipd_root = api.path['slave_build'].join('packages')
  # Some packages don't require credentials to be installed or queried.
  api.cipd.ensure(cipd_root, packages)
  step = api.cipd.search(package_name, tag='git_revision:40-chars-long-hash')
  api.cipd.describe(package_name,
                    version=step.json.output['result'][0]['instance_id'])

  # Others do, so provide creds first.
  api.cipd.set_service_account_credentials('fake-credentials.json')
  private_package_name = 'private/package/%s' % api.cipd.platform_suffix()
  packages[private_package_name] = 'latest'
  api.cipd.ensure(cipd_root, packages)
  step = api.cipd.search(private_package_name, tag='key:value')
  api.cipd.describe(private_package_name,
                    version=step.json.output['result'][0]['instance_id'],
                    test_data_tags=['custom:tagged', 'key:value'],
                    test_data_refs=['latest'])

  # The rest of commands expect credentials to be set.

  # Build & register new package version.
  api.cipd.build('fake-input-dir', 'fake-package-path', 'infra/fake-package')
  api.cipd.build('fake-input-dir', 'fake-package-path', 'infra/fake-package',
                 install_mode='copy')
  api.cipd.register('infra/fake-package', 'fake-package-path',
                    refs=['fake-ref-1', 'fake-ref-2'],
                    tags={'fake_tag_1': 'fake_value_1',
                          'fake_tag_2': 'fake_value_2'})

  # Create (build & register).
  api.cipd.create(api.path['slave_build'].join('fake-package.yaml'),
                  refs=['fake-ref-1', 'fake-ref-2'],
                  tags={'fake_tag_1': 'fake_value_1',
                        'fake_tag_2': 'fake_value_2'})

  # Set tag or ref of an already existing package.
  api.cipd.set_tag('fake-package',
                   version='long/weird/ref/which/doesn/not/fit/into/40chars',
                   tags={'dead': 'beaf', 'more': 'value'})
  api.cipd.set_ref('fake-package', version='latest', refs=['any', 'some'])
  # Search by the new tag.
  api.cipd.search('fake-package/%s' % api.cipd.platform_suffix(),
                  tag='dead:beaf')


def GenTests(api):
  yield (
    # This is very common dev workstation, but not all devs are on it.
    api.test('basic') +
    api.platform('linux', 64)
  )

  yield (
    api.test('mac64') +
    api.platform('mac', 64)
  )

  yield (
    api.test('win64') +
    api.platform('win', 64)
  )

  yield (
    api.test('install-failed') +
    api.step_data('install cipd', retcode=1)
  )

  yield (
    api.test('describe-failed') +
    api.platform('linux', 64) +
    api.override_step_data(
      'cipd describe public/package/linux-amd64',
      api.cipd.example_error(
        'package "public/package/linux-amd64-ubuntu14_04" not registered',
      )
    )
  )

  yield (
    api.test('describe-many-instances') +
    api.platform('linux', 64) +
    api.override_step_data(
      'cipd search fake-package/linux-amd64 dead:beaf',
      api.cipd.example_search(
        'public/package/linux-amd64-ubuntu14_04',
        instances=3
      )
    )
  )
