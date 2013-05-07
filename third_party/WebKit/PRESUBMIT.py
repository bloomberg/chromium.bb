# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Top-level presubmit script for Blink.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into gcl.
"""


_EXCLUDED_PATHS = ()


def _CheckForVersionControlConflictsInFile(input_api, f):
    pattern = input_api.re.compile('^(?:<<<<<<<|>>>>>>>) |^=======$')
    errors = []
    for line_num, line in f.ChangedContents():
        if pattern.match(line):
            errors.append('    %s:%d %s' % (f.LocalPath(), line_num, line))
    return errors


def _CheckForVersionControlConflicts(input_api, output_api):
    """Usually this is not intentional and will cause a compile failure."""
    errors = []
    for f in input_api.AffectedFiles():
        errors.extend(_CheckForVersionControlConflictsInFile(input_api, f))

    results = []
    if errors:
        results.append(output_api.PresubmitError(
            'Version control conflict markers found, please resolve.', errors))
    return results


def _CommonChecks(input_api, output_api):
    """Checks common to both upload and commit."""
    # We should figure out what license checks we actually want to use.
    license_header = r'.*'

    results = []
    results.extend(input_api.canned_checks.PanProjectChecks(
        input_api, output_api, excluded_paths=_EXCLUDED_PATHS,
        maxlen=800, license_header=license_header))
    results.extend(_CheckForVersionControlConflicts(input_api, output_api))
    results.extend(_CheckPatchFiles(input_api, output_api))
    results.extend(_CheckTestExpectations(input_api, output_api))
    return results


def _CheckSubversionConfig(input_api, output_api):
  """Verifies the subversion config file is correctly setup.

  Checks that autoprops are enabled, returns an error otherwise.
  """
  join = input_api.os_path.join
  if input_api.platform == 'win32':
    appdata = input_api.environ.get('APPDATA', '')
    if not appdata:
      return [output_api.PresubmitError('%APPDATA% is not configured.')]
    path = join(appdata, 'Subversion', 'config')
  else:
    home = input_api.environ.get('HOME', '')
    if not home:
      return [output_api.PresubmitError('$HOME is not configured.')]
    path = join(home, '.subversion', 'config')

  error_msg = (
      'Please look at http://dev.chromium.org/developers/coding-style to\n'
      'configure your subversion configuration file. This enables automatic\n'
      'properties to simplify the project maintenance.\n'
      'Pro-tip: just download and install\n'
      'http://src.chromium.org/viewvc/chrome/trunk/tools/build/slave/config\n')

  try:
    lines = open(path, 'r').read().splitlines()
    # Make sure auto-props is enabled and check for 2 Chromium standard
    # auto-prop.
    if (not '*.cc = svn:eol-style=LF' in lines or
        not '*.pdf = svn:mime-type=application/pdf' in lines or
        not 'enable-auto-props = yes' in lines):
      return [
          output_api.PresubmitNotifyResult(
              'It looks like you have not configured your subversion config '
              'file or it is not up-to-date.\n' + error_msg)
      ]
  except (OSError, IOError):
    return [
        output_api.PresubmitNotifyResult(
            'Can\'t find your subversion config file.\n' + error_msg)
    ]
  return []


def _CheckPatchFiles(input_api, output_api):
  problems = [f.LocalPath() for f in input_api.AffectedFiles()
      if f.LocalPath().endswith(('.orig', '.rej'))]
  if problems:
    return [output_api.PresubmitError(
        "Don't commit .rej and .orig files.", problems)]
  else:
    return []


def _CheckTestExpectations(input_api, output_api):
    local_paths = [f.LocalPath() for f in input_api.AffectedFiles()]
    if any(path.startswith('LayoutTests') for path in local_paths):
        lint_path = input_api.os_path.join(input_api.PresubmitLocalPath(),
            'Tools', 'Scripts', 'lint-test-expectations')
        _, errs = input_api.subprocess.Popen(
            [input_api.python_executable, lint_path],
            stdout=input_api.subprocess.PIPE,
            stderr=input_api.subprocess.PIPE).communicate()
        if not errs:
            return [output_api.PresubmitError(
                "lint-test-expectations failed "
                "to produce output; check by hand. ")]
        if errs.strip() != 'Lint succeeded.':
            return [output_api.PresubmitError(errs)]
    return []


def _CheckStyle(input_api, output_api):
    style_checker_path = input_api.os_path.join(input_api.PresubmitLocalPath(),
        'Tools', 'Scripts', 'check-webkit-style')
    args = ([input_api.python_executable, style_checker_path, '--diff-files']
        + [f.LocalPath() for f in input_api.AffectedFiles()])
    results = []

    try:
        child = input_api.subprocess.Popen(args,
                                           stderr=input_api.subprocess.PIPE)
        _, stderrdata = child.communicate()
        if child.returncode != 0:
            results.append(output_api.PresubmitError(
                'check-webkit-style failed', [stderrdata]))
    except Exception as e:
        results.append(output_api.PresubmitNotifyResult(
            'Could not run check-webkit-style', [str(e)]))

    return results


def CheckChangeOnUpload(input_api, output_api):
    results = []
    results.extend(_CommonChecks(input_api, output_api))
    results.extend(_CheckStyle(input_api, output_api))
    return results


def CheckChangeOnCommit(input_api, output_api):
    results = []
    results.extend(_CommonChecks(input_api, output_api))
    results.extend(input_api.canned_checks.CheckTreeIsOpen(
        input_api, output_api,
        json_url='http://blink-status.appspot.com/current?format=json'))
    results.extend(input_api.canned_checks.CheckChangeHasDescription(
        input_api, output_api))
    results.extend(_CheckSubversionConfig(input_api, output_api))
    return results

def GetPreferredTrySlaves(project, change):
    return ['linux_layout_rel', 'mac_layout_rel', 'win_layout_rel']
