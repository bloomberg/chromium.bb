# Copyright (c) 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Rather than pass this to all of the checks, we override the global excluded
# list with this one.
_EXCLUDED_PATHS = (
  # Exclude all of third_party/ except for BUILD.gns that we maintain.
  r'third_party[\\\/].*(?<!BUILD.gn)$',
  # Exclude everything under third_party/chromium_quic/{src|build}
  r'third_party/chromium_quic/(src|build)/.*',
  # Output directories (just in case)
  r'.*\bDebug[\\\/].*',
  r'.*\bRelease[\\\/].*',
  r'.*\bxcodebuild[\\\/].*',
  r'.*\bout[\\\/].*',
  # There is no point in processing a patch file.
  r'.+\.diff$',
  r'.+\.patch$',
)


def _CheckDeps(input_api, output_api):
  results = []
  import sys
  original_sys_path = sys.path
  try:
    sys.path = sys.path + [input_api.os_path.join(
        input_api.PresubmitLocalPath(), 'buildtools', 'checkdeps')]
    import checkdeps
    from cpp_checker import CppChecker
    from rules import Rule
  finally:
    sys.path = original_sys_path

  deps_checker = checkdeps.DepsChecker(input_api.PresubmitLocalPath())
  deps_checker.CheckDirectory(input_api.PresubmitLocalPath())
  deps_results = deps_checker.results_formatter.GetResults()
  for violation in deps_results:
    results.append(output_api.PresubmitError(violation))
  return results


def _CommonChecks(input_api, output_api):
  results = []
  # PanProjectChecks include:
  #   CheckLongLines (@ 80 cols)
  #   CheckChangeHasNoTabs
  #   CheckChangeHasNoStrayWhitespace
  #   CheckLicense
  #   CheckChangeWasUploaded (if committing)
  #   CheckChangeHasDescription
  #   CheckDoNotSubmitInDescription
  #   CheckDoNotSubmitInFiles
  results.extend(input_api.canned_checks.PanProjectChecks(
    input_api, output_api, owners_check=False));

  # No carriage return characters, files end with one EOL (\n).
  results.extend(input_api.canned_checks.CheckChangeHasNoCrAndHasOnlyOneEol(
    input_api, output_api));

  # Gender inclusivity
  results.extend(input_api.canned_checks.CheckGenderNeutral(
    input_api, output_api))

  # TODO(bug) format required
  results.extend(input_api.canned_checks.CheckChangeTodoHasOwner(
    input_api, output_api))

  # Linter.
  # - We disable c++11 header checks since Open Screen allows them.
  # - We disable whitespace/braces because of various false positives.
  # - There are some false positives with 'explicit' checks, but it's useful
  #   enough to keep.
  results.extend(input_api.canned_checks.CheckChangeLintsClean(
    input_api, output_api,
    lint_filters = ['-build/c++11', '-whitespace/braces'],
    verbose_level=4))

  # clang-format
  results.extend(input_api.canned_checks.CheckPatchFormatted(
    input_api, output_api, bypass_warnings=False))

  # GN formatting
  results.extend(input_api.canned_checks.CheckGNFormatted(
    input_api, output_api))

  # buildtools/checkdeps
  results.extend(_CheckDeps(input_api, output_api))
  return results


def CheckChangeOnUpload(input_api, output_api):
  input_api.DEFAULT_FILES_TO_SKIP = _EXCLUDED_PATHS;
  results = []
  results.extend(_CommonChecks(input_api, output_api))
  results.extend(
      input_api.canned_checks.CheckChangedLUCIConfigs(input_api, output_api))
  return results


def CheckChangeOnCommit(input_api, output_api):
  input_api.DEFAULT_FILES_TO_SKIP = _EXCLUDED_PATHS;
  results = []
  results.extend(_CommonChecks(input_api, output_api))
  return results
