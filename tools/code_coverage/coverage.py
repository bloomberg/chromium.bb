#!/usr/bin/python
# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""This script helps to generate code coverage report.

  It uses Clang Source-based Code Coverage -
  https://clang.llvm.org/docs/SourceBasedCodeCoverage.html

  In order to generate code coverage report, you need to first add
  "use_clang_coverage=true" GN flag to args.gn file in your build
  output directory (e.g. out/coverage).

  It is recommended to add "is_component_build=false" flag as well because:
  1. It is incompatible with other sanitizer flags (like "is_asan", "is_msan")
     and others like "optimize_for_fuzzing".
  2. If it is not set explicitly, "is_debug" overrides it to true.

  Example usage:

  gn gen out/coverage --args='use_clang_coverage=true is_component_build=false'
  gclient runhooks
  python tools/code_coverage/coverage.py crypto_unittests url_unittests \\
      -b out/coverage -o out/report -c 'out/coverage/crypto_unittests' \\
      -c 'out/coverage/url_unittests --gtest_filter=URLParser.PathURL' \\
      -f url/ -f crypto/

  The command above builds crypto_unittests and url_unittests targets and then
  runs them with specified command line arguments. For url_unittests, it only
  runs the test URLParser.PathURL. The coverage report is filtered to include
  only files and sub-directories under url/ and crypto/ directories.

  If you are building a fuzz target, you need to add "use_libfuzzer=true" GN
  flag as well.

  Sample workflow for a fuzz target (e.g. pdfium_fuzzer):

  python tools/code_coverage/coverage.py pdfium_fuzzer \\
      -b out/coverage -o out/report \\
      -c 'out/coverage/pdfium_fuzzer -runs=<runs> <corpus_dir>' \\
      -f third_party/pdfium

  where:
    <corpus_dir> - directory containing samples files for this format.
    <runs> - number of times to fuzz target function. Should be 0 when you just
             want to see the coverage on corpus and don't want to fuzz at all.

  For more options, please refer to tools/code_coverage/coverage.py -h.
"""

from __future__ import print_function

import sys

import argparse
import json
import os
import subprocess
import urllib2

sys.path.append(
    os.path.join(
        os.path.dirname(__file__), os.path.pardir, os.path.pardir, 'tools',
        'clang', 'scripts'))
import update as clang_update

sys.path.append(
    os.path.join(
        os.path.dirname(__file__), os.path.pardir, os.path.pardir,
        'third_party'))
import jinja2
from collections import defaultdict

# Absolute path to the root of the checkout.
SRC_ROOT_PATH = os.path.abspath(
    os.path.join(os.path.dirname(__file__), os.path.pardir, os.path.pardir))

# Absolute path to the code coverage tools binary.
LLVM_BUILD_DIR = clang_update.LLVM_BUILD_DIR
LLVM_COV_PATH = os.path.join(LLVM_BUILD_DIR, 'bin', 'llvm-cov')
LLVM_PROFDATA_PATH = os.path.join(LLVM_BUILD_DIR, 'bin', 'llvm-profdata')

# Build directory, the value is parsed from command line arguments.
BUILD_DIR = None

# Output directory for generated artifacts, the value is parsed from command
# line arguemnts.
OUTPUT_DIR = None

# Default number of jobs used to build when goma is configured and enabled.
DEFAULT_GOMA_JOBS = 100

# Name of the file extension for profraw data files.
PROFRAW_FILE_EXTENSION = 'profraw'

# Name of the final profdata file, and this file needs to be passed to
# "llvm-cov" command in order to call "llvm-cov show" to inspect the
# line-by-line coverage of specific files.
PROFDATA_FILE_NAME = 'coverage.profdata'

# Build arg required for generating code coverage data.
CLANG_COVERAGE_BUILD_ARG = 'use_clang_coverage'

# A set of targets that depend on target "testing/gtest", this set is generated
# by 'gn refs "testing/gtest"', and it is lazily initialized when needed.
GTEST_TARGET_NAMES = None

# The default name of the html coverage report for a directory.
DIRECTORY_COVERAGE_HTML_REPORT_NAME = os.extsep.join(['report', 'html'])


class _CoverageSummary(object):
  """Encapsulates coverage summary representation."""

  def __init__(self, regions_total, regions_covered, functions_total,
               functions_covered, lines_total, lines_covered):
    """Initializes _CoverageSummary object."""
    self._summary = {
        'regions': {
            'total': regions_total,
            'covered': regions_covered
        },
        'functions': {
            'total': functions_total,
            'covered': functions_covered
        },
        'lines': {
            'total': lines_total,
            'covered': lines_covered
        }
    }

  def Get(self):
    """Returns summary as a dictionary."""
    return self._summary

  def AddSummary(self, other_summary):
    """Adds another summary to this one element-wise."""
    for feature in self._summary:
      self._summary[feature]['total'] += other_summary.Get()[feature]['total']
      self._summary[feature]['covered'] += other_summary.Get()[feature][
          'covered']


class _DirectoryCoverageReportHtmlGenerator(object):
  """Encapsulates coverage html report generation for a directory.

  The generated html has a table that contains links to the coverage report of
  its sub-directories and files. Please refer to ./directory_example_report.html
  for an example of the generated html file.
  """

  def __init__(self):
    """Initializes _DirectoryCoverageReportHtmlGenerator object."""
    css_file_name = os.extsep.join(['style', 'css'])
    css_absolute_path = os.path.abspath(os.path.join(OUTPUT_DIR, css_file_name))
    assert os.path.exists(css_absolute_path), (
        'css file doesn\'t exit. Please make sure "llvm-cov show -format=html" '
        'is called first, and the css file is generated at: "%s"' %
        css_absolute_path)

    self._css_absolute_path = css_absolute_path
    self._table_entries = []
    self._total_entry = {}
    template_dir = os.path.join(
        os.path.dirname(os.path.realpath(__file__)), 'html_templates')

    jinja_env = jinja2.Environment(
        loader=jinja2.FileSystemLoader(template_dir), trim_blocks=True)
    self._header_template = jinja_env.get_template('header.html')
    self._table_template = jinja_env.get_template('table.html')
    self._footer_template = jinja_env.get_template('footer.html')

  def AddLinkToAnotherReport(self, html_report_path, name, summary):
    """Adds a link to another html report in this report.

    The link to be added is assumed to be an entry in this directory.
    """
    table_entry = self._CreateTableEntryFromCoverageSummary(
        summary, html_report_path, name,
        os.path.basename(html_report_path) ==
        DIRECTORY_COVERAGE_HTML_REPORT_NAME)
    self._table_entries.append(table_entry)

  def CreateTotalsEntry(self, summary):
    """Creates an entry corresponds to the 'TOTALS' row in the html report."""
    self._total_entry = self._CreateTableEntryFromCoverageSummary(summary)

  def _CreateTableEntryFromCoverageSummary(self,
                                           summary,
                                           href=None,
                                           name=None,
                                           is_dir=None):
    """Creates an entry to display in the html report."""
    entry = {}
    summary_dict = summary.Get()
    for feature in summary_dict:
      percentage = round((float(summary_dict[feature]['covered']
                               ) / summary_dict[feature]['total']) * 100, 2)
      color_class = self._GetColorClass(percentage)
      entry[feature] = {
          'total': summary_dict[feature]['total'],
          'covered': summary_dict[feature]['covered'],
          'percentage': percentage,
          'color_class': color_class
      }

    if href != None:
      entry['href'] = href
    if name != None:
      entry['name'] = name
    if is_dir != None:
      entry['is_dir'] = is_dir

    return entry

  def _GetColorClass(self, percentage):
    """Returns the css color class based on coverage percentage."""
    if percentage >= 0 and percentage < 80:
      return 'red'
    if percentage >= 80 and percentage < 100:
      return 'yellow'
    if percentage == 100:
      return 'green'

    assert False, 'Invalid coverage percentage: "%d"' % percentage

  def WriteHtmlCoverageReport(self, output_path):
    """Write html coverage report for the directory.

    In the report, sub-directories are displayed before files and within each
    category, entries are sorted alphabetically.

    Args:
      output_path: A path to the html report.
    """

    def EntryCmp(left, right):
      """Compare function for table entries."""
      if left['is_dir'] != right['is_dir']:
        return -1 if left['is_dir'] == True else 1

      return left['name'] < right['name']

    self._table_entries = sorted(self._table_entries, cmp=EntryCmp)

    css_path = os.path.join(OUTPUT_DIR, os.extsep.join(['style', 'css']))
    html_header = self._header_template.render(
        css_path=os.path.relpath(css_path, os.path.dirname(output_path)))
    html_table = self._table_template.render(
        entries=self._table_entries, total_entry=self._total_entry)
    html_footer = self._footer_template.render()

    with open(output_path, 'w') as html_file:
      html_file.write(html_header + html_table + html_footer)


def _GetPlatform():
  """Returns current running platform."""
  if sys.platform == 'win32' or sys.platform == 'cygwin':
    return 'win'
  if sys.platform.startswith('linux'):
    return 'linux'
  else:
    assert sys.platform == 'darwin'
    return 'mac'


# TODO(crbug.com/759794): remove this function once tools get included to
# Clang bundle:
# https://chromium-review.googlesource.com/c/chromium/src/+/688221
def DownloadCoverageToolsIfNeeded():
  """Temporary solution to download llvm-profdata and llvm-cov tools."""

  def _GetRevisionFromStampFile(stamp_file_path, platform):
    """Returns a pair of revision number by reading the build stamp file.

    Args:
      stamp_file_path: A path the build stamp file created by
                       tools/clang/scripts/update.py.
    Returns:
      A pair of integers represeting the main and sub revision respectively.
    """
    if not os.path.exists(stamp_file_path):
      return 0, 0

    with open(stamp_file_path) as stamp_file:
      for stamp_file_line in stamp_file.readlines():
        if ',' in stamp_file_line:
          package_version, target_os = stamp_file_line.rstrip().split(',')
        else:
          package_version = stamp_file_line.rstrip()
          target_os = ''

        if target_os and platform != target_os:
          continue

        clang_revision_str, clang_sub_revision_str = package_version.split('-')
        return int(clang_revision_str), int(clang_sub_revision_str)

    assert False, 'Coverage is only supported on target_os - linux, mac.'

  platform = _GetPlatform()
  clang_revision, clang_sub_revision = _GetRevisionFromStampFile(
      clang_update.STAMP_FILE, platform)

  coverage_revision_stamp_file = os.path.join(
      os.path.dirname(clang_update.STAMP_FILE), 'cr_coverage_revision')
  coverage_revision, coverage_sub_revision = _GetRevisionFromStampFile(
      coverage_revision_stamp_file, platform)

  has_coverage_tools = (
      os.path.exists(LLVM_COV_PATH) and os.path.exists(LLVM_PROFDATA_PATH))

  if (has_coverage_tools and coverage_revision == clang_revision and
      coverage_sub_revision == clang_sub_revision):
    # LLVM coverage tools are up to date, bail out.
    return clang_revision

  package_version = '%d-%d' % (clang_revision, clang_sub_revision)
  coverage_tools_file = 'llvm-code-coverage-%s.tgz' % package_version

  # The code bellow follows the code from tools/clang/scripts/update.py.
  if platform == 'mac':
    coverage_tools_url = clang_update.CDS_URL + '/Mac/' + coverage_tools_file
  else:
    assert platform == 'linux'
    coverage_tools_url = (
        clang_update.CDS_URL + '/Linux_x64/' + coverage_tools_file)

  try:
    clang_update.DownloadAndUnpack(coverage_tools_url,
                                   clang_update.LLVM_BUILD_DIR)
    print('Coverage tools %s unpacked' % package_version)
    with open(coverage_revision_stamp_file, 'w') as file_handle:
      file_handle.write('%s,%s' % (package_version, platform))
      file_handle.write('\n')
  except urllib2.URLError:
    raise Exception(
        'Failed to download coverage tools: %s.' % coverage_tools_url)


def _GenerateLineByLineFileCoverageInHtml(binary_paths, profdata_file_path,
                                          filters):
  """Generates per file line-by-line coverage in html using 'llvm-cov show'.

  For a file with absolute path /a/b/x.cc, a html report is generated as:
  OUTPUT_DIR/coverage/a/b/x.cc.html. An index html file is also generated as:
  OUTPUT_DIR/index.html.

  Args:
    binary_paths: A list of paths to the instrumented binaries.
    profdata_file_path: A path to the profdata file.
    filters: A list of directories and files to get coverage for.
  """
  # llvm-cov show [options] -instr-profile PROFILE BIN [-object BIN,...]
  # [[-object BIN]] [SOURCES]
  # NOTE: For object files, the first one is specified as a positional argument,
  # and the rest are specified as keyword argument.
  subprocess_cmd = [
      LLVM_COV_PATH, 'show', '-format=html',
      '-output-dir={}'.format(OUTPUT_DIR),
      '-instr-profile={}'.format(profdata_file_path), binary_paths[0]
  ]
  subprocess_cmd.extend(
      ['-object=' + binary_path for binary_path in binary_paths[1:]])
  subprocess_cmd.extend(filters)

  subprocess.check_call(subprocess_cmd)


def _GeneratePerDirectoryCoverageInHtml(binary_paths, profdata_file_path,
                                        filters):
  """Generates coverage breakdown per directory."""
  per_file_coverage_summary = _GeneratePerFileCoverageSummary(
      binary_paths, profdata_file_path, filters)

  per_directory_coverage_summary = defaultdict(
      lambda: _CoverageSummary(0, 0, 0, 0, 0, 0))

  # Calculate per directory code coverage summaries.
  for file_path in per_file_coverage_summary:
    summary = per_file_coverage_summary[file_path]
    parent_dir = os.path.dirname(file_path)
    while True:
      per_directory_coverage_summary[parent_dir].AddSummary(summary)

      if parent_dir == SRC_ROOT_PATH:
        break
      parent_dir = os.path.dirname(parent_dir)

  for dir_path in per_directory_coverage_summary:
    _GenerateCoverageInHtmlForDirectory(
        dir_path, per_directory_coverage_summary, per_file_coverage_summary)


def _GenerateCoverageInHtmlForDirectory(
    dir_path, per_directory_coverage_summary, per_file_coverage_summary):
  """Generates coverage html report for a single directory."""
  html_generator = _DirectoryCoverageReportHtmlGenerator()

  for entry_name in os.listdir(dir_path):
    entry_path = os.path.normpath(os.path.join(dir_path, entry_name))
    entry_html_report_path = _GetCoverageHtmlReportPath(entry_path)

    # Use relative paths instead of absolute paths to make the generated
    # reports portable.
    html_report_path = _GetCoverageHtmlReportPath(dir_path)
    entry_html_report_relative_path = os.path.relpath(
        entry_html_report_path, os.path.dirname(html_report_path))

    if entry_path in per_directory_coverage_summary:
      html_generator.AddLinkToAnotherReport(
          entry_html_report_relative_path, os.path.basename(entry_path),
          per_directory_coverage_summary[entry_path])
    elif entry_path in per_file_coverage_summary:
      html_generator.AddLinkToAnotherReport(
          entry_html_report_relative_path, os.path.basename(entry_path),
          per_file_coverage_summary[entry_path])

  html_generator.CreateTotalsEntry(per_directory_coverage_summary[dir_path])
  html_generator.WriteHtmlCoverageReport(html_report_path)


def _OverwriteHtmlReportsIndexFile():
  """Overwrites the index file to link to the source root directory report."""
  html_index_file_path = os.path.join(OUTPUT_DIR,
                                      os.extsep.join(['index', 'html']))
  src_root_html_report_path = _GetCoverageHtmlReportPath(SRC_ROOT_PATH)
  src_root_html_report_relative_path = os.path.relpath(
      src_root_html_report_path, os.path.dirname(html_index_file_path))
  content = ("""
    <!DOCTYPE html>
    <html>
      <head>
        <!-- HTML meta refresh URL redirection -->
        <meta http-equiv="refresh" content="0; url=%s">
      </head>
    </html>""" % src_root_html_report_relative_path)
  with open(html_index_file_path, 'w') as f:
    f.write(content)


def _GetCoverageHtmlReportPath(file_or_dir_path):
  """Given a file or directory, returns the corresponding html report path."""
  html_path = (
      os.path.join(os.path.abspath(OUTPUT_DIR), 'coverage') +
      os.path.abspath(file_or_dir_path))
  if os.path.isdir(file_or_dir_path):
    return os.path.join(html_path, DIRECTORY_COVERAGE_HTML_REPORT_NAME)
  else:
    return os.extsep.join([html_path, 'html'])


def _CreateCoverageProfileDataForTargets(targets, commands, jobs_count=None):
  """Builds and runs target to generate the coverage profile data.

  Args:
    targets: A list of targets to build with coverage instrumentation.
    commands: A list of commands used to run the targets.
    jobs_count: Number of jobs to run in parallel for building. If None, a
                default value is derived based on CPUs availability.

  Returns:
    A relative path to the generated profdata file.
  """
  _BuildTargets(targets, jobs_count)
  profraw_file_paths = _GetProfileRawDataPathsByExecutingCommands(
      targets, commands)
  profdata_file_path = _CreateCoverageProfileDataFromProfRawData(
      profraw_file_paths)

  for profraw_file_path in profraw_file_paths:
    os.remove(profraw_file_path)

  return profdata_file_path


def _BuildTargets(targets, jobs_count):
  """Builds target with Clang coverage instrumentation.

  This function requires current working directory to be the root of checkout.

  Args:
    targets: A list of targets to build with coverage instrumentation.
    jobs_count: Number of jobs to run in parallel for compilation. If None, a
                default value is derived based on CPUs availability.
  """

  def _IsGomaConfigured():
    """Returns True if goma is enabled in the gn build args.

    Returns:
      A boolean indicates whether goma is configured for building or not.
    """
    build_args = _ParseArgsGnFile()
    return 'use_goma' in build_args and build_args['use_goma'] == 'true'

  print('Building %s' % str(targets))

  if jobs_count is None and _IsGomaConfigured():
    jobs_count = DEFAULT_GOMA_JOBS

  subprocess_cmd = ['ninja', '-C', BUILD_DIR]
  if jobs_count is not None:
    subprocess_cmd.append('-j' + str(jobs_count))

  subprocess_cmd.extend(targets)
  subprocess.check_call(subprocess_cmd)


def _GetProfileRawDataPathsByExecutingCommands(targets, commands):
  """Runs commands and returns the relative paths to the profraw data files.

  Args:
    targets: A list of targets built with coverage instrumentation.
    commands: A list of commands used to run the targets.

  Returns:
    A list of relative paths to the generated profraw data files.
  """
  # Remove existing profraw data files.
  for file_or_dir in os.listdir(OUTPUT_DIR):
    if file_or_dir.endswith(PROFRAW_FILE_EXTENSION):
      os.remove(os.path.join(OUTPUT_DIR, file_or_dir))

  # Run all test targets to generate profraw data files.
  for target, command in zip(targets, commands):
    _ExecuteCommand(target, command)

  profraw_file_paths = []
  for file_or_dir in os.listdir(OUTPUT_DIR):
    if file_or_dir.endswith(PROFRAW_FILE_EXTENSION):
      profraw_file_paths.append(os.path.join(OUTPUT_DIR, file_or_dir))

  # Assert one target/command generates at least one profraw data file.
  for target in targets:
    assert any(
        os.path.basename(profraw_file).startswith(target)
        for profraw_file in profraw_file_paths), (
            'Running target: %s failed to generate any profraw data file, '
            'please make sure the binary exists and is properly instrumented.' %
            target)

  return profraw_file_paths


def _ExecuteCommand(target, command):
  """Runs a single command and generates a profraw data file.

  Args:
    target: A target built with coverage instrumentation.
    command: A command used to run the target.
  """
  # Per Clang "Source-based Code Coverage" doc:
  # "%Nm" expands out to the instrumented binary's signature. When this pattern
  # is specified, the runtime creates a pool of N raw profiles which are used
  # for on-line profile merging. The runtime takes care of selecting a raw
  # profile from the pool, locking it, and updating it before the program exits.
  # If N is not specified (i.e the pattern is "%m"), it's assumed that N = 1.
  # N must be between 1 and 9. The merge pool specifier can only occur once per
  # filename pattern.
  #
  # 4 is chosen because it creates some level of parallelism, but it's not too
  # big to consume too much computing resource or disk space.
  expected_profraw_file_name = os.extsep.join(
      [target, '%4m', PROFRAW_FILE_EXTENSION])
  expected_profraw_file_path = os.path.join(OUTPUT_DIR,
                                            expected_profraw_file_name)
  output_file_name = os.extsep.join([target + '_output', 'txt'])
  output_file_path = os.path.join(OUTPUT_DIR, output_file_name)

  print('Running command: "%s", the output is redirected to "%s"' %
        (command, output_file_path))
  output = subprocess.check_output(
      command.split(), env={
          'LLVM_PROFILE_FILE': expected_profraw_file_path
      })
  with open(output_file_path, 'w') as output_file:
    output_file.write(output)


def _CreateCoverageProfileDataFromProfRawData(profraw_file_paths):
  """Returns a relative path to the profdata file by merging profraw data files.

  Args:
    profraw_file_paths: A list of relative paths to the profraw data files that
                        are to be merged.

  Returns:
    A relative path to the generated profdata file.

  Raises:
    CalledProcessError: An error occurred merging profraw data files.
  """
  print('Creating the profile data file')

  profdata_file_path = os.path.join(OUTPUT_DIR, PROFDATA_FILE_NAME)

  try:
    subprocess_cmd = [
        LLVM_PROFDATA_PATH, 'merge', '-o', profdata_file_path, '-sparse=true'
    ]
    subprocess_cmd.extend(profraw_file_paths)
    subprocess.check_call(subprocess_cmd)
  except subprocess.CalledProcessError as error:
    print('Failed to merge profraw files to create profdata file')
    raise error

  return profdata_file_path


def _GeneratePerFileCoverageSummary(binary_paths, profdata_file_path, filters):
  """Generates per file coverage summary using "llvm-cov export" command."""
  # llvm-cov export [options] -instr-profile PROFILE BIN [-object BIN,...]
  # [[-object BIN]] [SOURCES].
  # NOTE: For object files, the first one is specified as a positional argument,
  # and the rest are specified as keyword argument.
  subprocess_cmd = [
      LLVM_COV_PATH, 'export', '-summary-only',
      '-instr-profile=' + profdata_file_path, binary_paths[0]
  ]
  subprocess_cmd.extend(
      ['-object=' + binary_path for binary_path in binary_paths[1:]])
  subprocess_cmd.extend(filters)

  json_output = json.loads(subprocess.check_output(subprocess_cmd))
  assert len(json_output['data']) == 1
  files_coverage_data = json_output['data'][0]['files']

  per_file_coverage_summary = {}
  for file_coverage_data in files_coverage_data:
    file_path = file_coverage_data['filename']
    summary = file_coverage_data['summary']

    # TODO(crbug.com/797345): Currently, [SOURCES] parameter doesn't apply to
    # llvm-cov export command, so work it around by manually filter the paths.
    # Remove this logic once the bug is fixed and clang has rolled past it.
    if filters and not any(
        os.path.abspath(file_path).startswith(os.path.abspath(filter))
        for filter in filters):
      continue

    if summary['lines']['count'] == 0:
      continue

    per_file_coverage_summary[file_path] = _CoverageSummary(
        regions_total=summary['regions']['count'],
        regions_covered=summary['regions']['covered'],
        functions_total=summary['functions']['count'],
        functions_covered=summary['functions']['covered'],
        lines_total=summary['lines']['count'],
        lines_covered=summary['lines']['covered'])

  return per_file_coverage_summary


def _GetBinaryPath(command):
  """Returns a relative path to the binary to be run by the command.

  Args:
    command: A command used to run a target.

  Returns:
    A relative path to the binary.
  """
  return command.split()[0]


def _VerifyTargetExecutablesAreInBuildDirectory(commands):
  """Verifies that the target executables specified in the commands are inside
  the given build directory."""
  for command in commands:
    binary_path = _GetBinaryPath(command)
    binary_absolute_path = os.path.abspath(os.path.normpath(binary_path))
    assert binary_absolute_path.startswith(os.path.abspath(BUILD_DIR)), (
        'Target executable "%s" in command: "%s" is outside of '
        'the given build directory: "%s".' % (binary_path, command, BUILD_DIR))


def _ValidateBuildingWithClangCoverage():
  """Asserts that targets are built with Clang coverage enabled."""
  build_args = _ParseArgsGnFile()

  if (CLANG_COVERAGE_BUILD_ARG not in build_args or
      build_args[CLANG_COVERAGE_BUILD_ARG] != 'true'):
    assert False, ('\'{} = true\' is required in args.gn.'
                  ).format(CLANG_COVERAGE_BUILD_ARG)


def _ParseArgsGnFile():
  """Parses args.gn file and returns results as a dictionary.

  Returns:
    A dictionary representing the build args.
  """
  build_args_path = os.path.join(BUILD_DIR, 'args.gn')
  assert os.path.exists(build_args_path), ('"%s" is not a build directory, '
                                           'missing args.gn file.' % BUILD_DIR)
  with open(build_args_path) as build_args_file:
    build_args_lines = build_args_file.readlines()

  build_args = {}
  for build_arg_line in build_args_lines:
    build_arg_without_comments = build_arg_line.split('#')[0]
    key_value_pair = build_arg_without_comments.split('=')
    if len(key_value_pair) != 2:
      continue

    key = key_value_pair[0].strip()
    value = key_value_pair[1].strip()
    build_args[key] = value

  return build_args


def _VerifyPathsAndReturnAbsolutes(paths):
  """Verifies that the paths specified in |paths| exist and returns absolute
  versions.

  Args:
    paths: A list of files or directories.
  """
  absolute_paths = []
  for path in paths:
    absolute_path = os.path.join(SRC_ROOT_PATH, path)
    assert os.path.exists(absolute_path), ('Path: "%s" doesn\'t exist.' % path)

    absolute_paths.append(absolute_path)

  return absolute_paths


def _ParseCommandArguments():
  """Adds and parses relevant arguments for tool comands.

  Returns:
    A dictionary representing the arguments.
  """
  arg_parser = argparse.ArgumentParser()
  arg_parser.usage = __doc__

  arg_parser.add_argument(
      '-b',
      '--build-dir',
      type=str,
      required=True,
      help='The build directory, the path needs to be relative to the root of '
      'the checkout.')

  arg_parser.add_argument(
      '-o',
      '--output-dir',
      type=str,
      required=True,
      help='Output directory for generated artifacts.')

  arg_parser.add_argument(
      '-c',
      '--command',
      action='append',
      required=True,
      help='Commands used to run test targets, one test target needs one and '
      'only one command, when specifying commands, one should assume the '
      'current working directory is the root of the checkout.')

  arg_parser.add_argument(
      '-f',
      '--filters',
      action='append',
      required=False,
      help='Directories or files to get code coverage for, and all files under '
      'the directories are included recursively.')

  arg_parser.add_argument(
      '-j',
      '--jobs',
      type=int,
      default=None,
      help='Run N jobs to build in parallel. If not specified, a default value '
      'will be derived based on CPUs availability. Please refer to '
      '\'ninja -h\' for more details.')

  arg_parser.add_argument(
      'targets', nargs='+', help='The names of the test targets to run.')

  args = arg_parser.parse_args()
  return args


def Main():
  """Execute tool commands."""
  assert _GetPlatform() in ['linux', 'mac'], (
      'Coverage is only supported on linux and mac platforms.')
  assert os.path.abspath(os.getcwd()) == SRC_ROOT_PATH, ('This script must be '
                                                         'called from the root '
                                                         'of checkout.')
  DownloadCoverageToolsIfNeeded()

  args = _ParseCommandArguments()
  global BUILD_DIR
  BUILD_DIR = args.build_dir
  global OUTPUT_DIR
  OUTPUT_DIR = args.output_dir

  assert len(args.targets) == len(args.command), ('Number of targets must be '
                                                  'equal to the number of test '
                                                  'commands.')
  assert os.path.exists(BUILD_DIR), (
      'Build directory: {} doesn\'t exist. '
      'Please run "gn gen" to generate.').format(BUILD_DIR)
  _ValidateBuildingWithClangCoverage()
  _VerifyTargetExecutablesAreInBuildDirectory(args.command)

  absolute_filter_paths = []
  if args.filters:
    absolute_filter_paths = _VerifyPathsAndReturnAbsolutes(args.filters)

  if not os.path.exists(OUTPUT_DIR):
    os.makedirs(OUTPUT_DIR)

  profdata_file_path = _CreateCoverageProfileDataForTargets(
      args.targets, args.command, args.jobs)
  binary_paths = [_GetBinaryPath(command) for command in args.command]

  print('Generating code coverage report in html (this can take a while '
        'depending on size of target!)')
  _GenerateLineByLineFileCoverageInHtml(binary_paths, profdata_file_path,
                                        absolute_filter_paths)
  _GeneratePerDirectoryCoverageInHtml(binary_paths, profdata_file_path,
                                      absolute_filter_paths)

  # The default index file is generated only for the list of source files, needs
  # to overwrite it to display per directory code coverage breakdown.
  _OverwriteHtmlReportsIndexFile()

  html_index_file_path = 'file://' + os.path.abspath(
      os.path.join(OUTPUT_DIR, 'index.html'))
  print('\nCode coverage profile data is created as: %s' % profdata_file_path)
  print('Index file for html report is generated as: %s' % html_index_file_path)


if __name__ == '__main__':
  sys.exit(Main())
