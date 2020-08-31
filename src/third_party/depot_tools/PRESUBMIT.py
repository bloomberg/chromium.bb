# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Top-level presubmit script for depot tools.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts for
details on the presubmit API built into depot_tools.
"""

import fnmatch
import os
import sys


# CIPD ensure manifest for checking CIPD client itself.
CIPD_CLIENT_ENSURE_FILE_TEMPLATE = r'''
# Full supported.
$VerifiedPlatform linux-amd64 mac-amd64 windows-amd64 windows-386
# Best effort support.
$VerifiedPlatform linux-386 linux-ppc64 linux-ppc64le linux-s390x
$VerifiedPlatform linux-arm64 linux-armv6l
$VerifiedPlatform linux-mips64 linux-mips64le linux-mipsle

%s %s
'''

# Timeout for a test to be executed.
TEST_TIMEOUT_S = 330  # 5m 30s



def DepotToolsPylint(input_api, output_api):
  """Gather all the pylint logic into one place to make it self-contained."""
  white_list = [
    r'^[^/]*\.py$',
    r'^testing_support/[^/]*\.py$',
    r'^tests/[^/]*\.py$',
    r'^recipe_modules/.*\.py$',  # Allow recursive search in recipe modules.
  ]
  black_list = list(input_api.DEFAULT_BLACK_LIST)
  if os.path.exists('.gitignore'):
    with open('.gitignore') as fh:
      lines = [l.strip() for l in fh.readlines()]
      black_list.extend([fnmatch.translate(l) for l in lines if
                         l and not l.startswith('#')])
  if os.path.exists('.git/info/exclude'):
    with open('.git/info/exclude') as fh:
      lines = [l.strip() for l in fh.readlines()]
      black_list.extend([fnmatch.translate(l) for l in lines if
                         l and not l.startswith('#')])
  disabled_warnings = [
    'R0401',  # Cyclic import
    'W0613',  # Unused argument
  ]
  return input_api.canned_checks.GetPylint(
      input_api,
      output_api,
      white_list=white_list,
      black_list=black_list,
      disabled_warnings=disabled_warnings)


def CommonChecks(input_api, output_api, tests_to_black_list, run_on_python3):
  input_api.SetTimeout(TEST_TIMEOUT_S)

  results = []
  results.extend(input_api.canned_checks.CheckOwners(input_api, output_api))
  results.extend(input_api.canned_checks.CheckOwnersFormat(
      input_api, output_api))
  results.extend(input_api.canned_checks.CheckJsonParses(
      input_api, output_api))

  # Run only selected tests on Windows.
  tests_to_white_list = [r'.*test\.py$']
  if input_api.platform.startswith(('cygwin', 'win32')):
    print('Warning: skipping most unit tests on Windows')
    tests_to_black_list = [
        r'.*auth_test\.py$',
        r'.*git_common_test\.py$',
        r'.*git_hyper_blame_test\.py$',
        r'.*git_map_test\.py$',
        r'.*ninjalog_uploader_test\.py$',
        r'.*recipes_test\.py$',
    ]

  # TODO(maruel): Make sure at least one file is modified first.
  # TODO(maruel): If only tests are modified, only run them.
  tests = DepotToolsPylint(input_api, output_api)
  tests.extend(input_api.canned_checks.GetUnitTestsInDirectory(
      input_api,
      output_api,
      'tests',
      whitelist=tests_to_white_list,
      blacklist=tests_to_black_list,
      run_on_python3=run_on_python3))

  # Validate CIPD manifests.
  root = input_api.os_path.normpath(
    input_api.os_path.abspath(input_api.PresubmitLocalPath()))
  rel_file = lambda rel: input_api.os_path.join(root, rel)
  cipd_manifests = set(rel_file(input_api.os_path.join(*x)) for x in (
    ('cipd_manifest.txt',),
    ('bootstrap', 'manifest.txt'),
    ('bootstrap', 'manifest_bleeding_edge.txt'),

    # Also generate a file for the cipd client itself.
    ('cipd_client_version',),
  ))
  affected_manifests = input_api.AffectedFiles(
    include_deletes=False,
    file_filter=lambda x:
      input_api.os_path.normpath(x.AbsoluteLocalPath()) in cipd_manifests)
  for path in affected_manifests:
    path = path.AbsoluteLocalPath()
    if path.endswith('.txt'):
      tests.append(input_api.canned_checks.CheckCIPDManifest(
          input_api, output_api, path=path))
    else:
      pkg = 'infra/tools/cipd/${platform}'
      ver = input_api.ReadFile(path)
      tests.append(input_api.canned_checks.CheckCIPDManifest(
          input_api, output_api,
          content=CIPD_CLIENT_ENSURE_FILE_TEMPLATE % (pkg, ver)))
      tests.append(input_api.canned_checks.CheckCIPDClientDigests(
          input_api, output_api, client_version_file=path))

  results.extend(input_api.RunTests(tests))
  return results


def CheckChangeOnUpload(input_api, output_api):
  # Do not run integration tests on upload since they are way too slow.
  tests_to_black_list = [
      r'^checkout_test\.py$',
      r'^cipd_bootstrap_test\.py$',
      r'^gclient_smoketest\.py$',
  ]
  # TODO(ehmaldonado): Run Python 3 tests on upload once Python 3 is
  # bootstrapped on Linux and Mac.
  return CommonChecks(input_api, output_api, tests_to_black_list, False)


def CheckChangeOnCommit(input_api, output_api):
  output = []
  output.extend(CommonChecks(input_api, output_api, [], True))
  output.extend(input_api.canned_checks.CheckDoNotSubmit(
      input_api,
      output_api))
  return output
