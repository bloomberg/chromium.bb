#!/usr/bin/python
# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""This script helps to generate code coverage report.

  It uses Clang Source-based Code Coverage -
  https://clang.llvm.org/docs/SourceBasedCodeCoverage.html

  In order to generate code coverage report, you need to first add
  "use_clang_coverage=true" and "is_component_build=false" GN flags to args.gn
  file in your build output directory (e.g. out/coverage).

  Existing implementation requires "is_component_build=false" flag because
  coverage info for dynamic libraries may be missing and "is_component_build"
  is set to true by "is_debug" unless it is explicitly set to false.

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

  If you want to run tests that try to draw to the screen but don't have a
  display connected, you can run tests in headless mode with xvfb.

  Sample flow for running a test target with xvfb (e.g. unit_tests):

  python tools/code_coverage/coverage.py unit_tests -b out/coverage \\
      -o out/report -c 'python testing/xvfb.py out/coverage/unit_tests'

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
import logging
import os
import re
import shlex
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

# The default name of the html coverage report for a directory.
DIRECTORY_COVERAGE_HTML_REPORT_NAME = os.extsep.join(['report', 'html'])

# Name of the html index files for different views.
DIRECTORY_VIEW_INDEX_FILE = os.extsep.join(['directory_view_index', 'html'])
COMPONENT_VIEW_INDEX_FILE = os.extsep.join(['component_view_index', 'html'])
FILE_VIEW_INDEX_FILE = os.extsep.join(['file_view_index', 'html'])

# Used to extract a mapping between directories and components.
COMPONENT_MAPPING_URL = 'https://storage.googleapis.com/chromium-owners/component_map.json'

# Caches the results returned by _GetBuildArgs, don't use this variable
# directly, call _GetBuildArgs instead.
_BUILD_ARGS = None


class _CoverageSummary(object):
  """Encapsulates coverage summary representation."""

  def __init__(self,
               regions_total=0,
               regions_covered=0,
               functions_total=0,
               functions_covered=0,
               lines_total=0,
               lines_covered=0):
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


class _CoverageReportHtmlGenerator(object):
  """Encapsulates coverage html report generation.

  The generated html has a table that contains links to other coverage reports.
  """

  def __init__(self, output_path, table_entry_type):
    """Initializes _CoverageReportHtmlGenerator object.

    Args:
      output_path: Path to the html report that will be generated.
      table_entry_type: Type of the table entries to be displayed in the table
                        header. For example: 'Path', 'Component'.
    """
    css_file_name = os.extsep.join(['style', 'css'])
    css_absolute_path = os.path.abspath(os.path.join(OUTPUT_DIR, css_file_name))
    assert os.path.exists(css_absolute_path), (
        'css file doesn\'t exit. Please make sure "llvm-cov show -format=html" '
        'is called first, and the css file is generated at: "%s"' %
        css_absolute_path)

    self._css_absolute_path = css_absolute_path
    self._output_path = output_path
    self._table_entry_type = table_entry_type

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
    # Use relative paths instead of absolute paths to make the generated reports
    # portable.
    html_report_relative_path = _GetRelativePathToDirectoryOfFile(
        html_report_path, self._output_path)

    table_entry = self._CreateTableEntryFromCoverageSummary(
        summary, html_report_relative_path, name,
        os.path.basename(html_report_path) ==
        DIRECTORY_COVERAGE_HTML_REPORT_NAME)
    self._table_entries.append(table_entry)

  def CreateTotalsEntry(self, summary):
    """Creates an entry corresponds to the 'Totals' row in the html report."""
    self._total_entry = self._CreateTableEntryFromCoverageSummary(summary)

  def _CreateTableEntryFromCoverageSummary(self,
                                           summary,
                                           href=None,
                                           name=None,
                                           is_dir=None):
    """Creates an entry to display in the html report."""
    assert (href is None and name is None and is_dir is None) or (
        href is not None and name is not None and is_dir is not None), (
            'The only scenario when href or name or is_dir can be None is when '
            'creating an entry for the Totals row, and in that case, all three '
            'attributes must be None.')

    entry = {}
    if href is not None:
      entry['href'] = href
    if name is not None:
      entry['name'] = name
    if is_dir is not None:
      entry['is_dir'] = is_dir

    summary_dict = summary.Get()
    for feature in summary_dict:
      if summary_dict[feature]['total'] == 0:
        percentage = 0.0
      else:
        percentage = float(summary_dict[feature]['covered']) / summary_dict[
            feature]['total'] * 100

      color_class = self._GetColorClass(percentage)
      entry[feature] = {
          'total': summary_dict[feature]['total'],
          'covered': summary_dict[feature]['covered'],
          'percentage': '{:6.2f}'.format(percentage),
          'color_class': color_class
      }

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

  def WriteHtmlCoverageReport(self):
    """Writes html coverage report.

    In the report, sub-directories are displayed before files and within each
    category, entries are sorted alphabetically.
    """

    def EntryCmp(left, right):
      """Compare function for table entries."""
      if left['is_dir'] != right['is_dir']:
        return -1 if left['is_dir'] == True else 1

      return -1 if left['name'] < right['name'] else 1

    self._table_entries = sorted(self._table_entries, cmp=EntryCmp)

    css_path = os.path.join(OUTPUT_DIR, os.extsep.join(['style', 'css']))
    directory_view_path = os.path.join(OUTPUT_DIR, DIRECTORY_VIEW_INDEX_FILE)
    component_view_path = os.path.join(OUTPUT_DIR, COMPONENT_VIEW_INDEX_FILE)
    file_view_path = os.path.join(OUTPUT_DIR, FILE_VIEW_INDEX_FILE)

    html_header = self._header_template.render(
        css_path=_GetRelativePathToDirectoryOfFile(css_path, self._output_path),
        directory_view_href=_GetRelativePathToDirectoryOfFile(
            directory_view_path, self._output_path),
        component_view_href=_GetRelativePathToDirectoryOfFile(
            component_view_path, self._output_path),
        file_view_href=_GetRelativePathToDirectoryOfFile(
            file_view_path, self._output_path))

    html_table = self._table_template.render(
        entries=self._table_entries,
        total_entry=self._total_entry,
        table_entry_type=self._table_entry_type)
    html_footer = self._footer_template.render()

    with open(self._output_path, 'w') as html_file:
      html_file.write(html_header + html_table + html_footer)


def _GetHostPlatform():
  """Returns the host platform.

  This is separate from the target platform/os that coverage is running for.
  """
  if sys.platform == 'win32' or sys.platform == 'cygwin':
    return 'win'
  if sys.platform.startswith('linux'):
    return 'linux'
  else:
    assert sys.platform == 'darwin'
    return 'mac'


def _GetTargetOS():
  """Returns the target os specified in args.gn file.

  Returns an empty string is target_os is not specified.
  """
  build_args = _GetBuildArgs()
  return build_args['target_os'] if 'target_os' in build_args else ''


def _IsIOS():
  """Returns true if the target_os specified in args.gn file is ios"""
  return _GetTargetOS() == 'ios'


# TODO(crbug.com/759794): remove this function once tools get included to
# Clang bundle:
# https://chromium-review.googlesource.com/c/chromium/src/+/688221
def DownloadCoverageToolsIfNeeded():
  """Temporary solution to download llvm-profdata and llvm-cov tools."""

  def _GetRevisionFromStampFile(stamp_file_path):
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
      stamp_file_line = stamp_file.readline()
      if ',' in stamp_file_line:
        package_version = stamp_file_line.rstrip().split(',')[0]
      else:
        package_version = stamp_file_line.rstrip()

      clang_revision_str, clang_sub_revision_str = package_version.split('-')
      return int(clang_revision_str), int(clang_sub_revision_str)

  host_platform = _GetHostPlatform()
  clang_revision, clang_sub_revision = _GetRevisionFromStampFile(
      clang_update.STAMP_FILE)

  coverage_revision_stamp_file = os.path.join(
      os.path.dirname(clang_update.STAMP_FILE), 'cr_coverage_revision')
  coverage_revision, coverage_sub_revision = _GetRevisionFromStampFile(
      coverage_revision_stamp_file)

  has_coverage_tools = (
      os.path.exists(LLVM_COV_PATH) and os.path.exists(LLVM_PROFDATA_PATH))

  if (has_coverage_tools and coverage_revision == clang_revision and
      coverage_sub_revision == clang_sub_revision):
    # LLVM coverage tools are up to date, bail out.
    return

  package_version = '%d-%d' % (clang_revision, clang_sub_revision)
  coverage_tools_file = 'llvm-code-coverage-%s.tgz' % package_version

  # The code bellow follows the code from tools/clang/scripts/update.py.
  if host_platform == 'mac':
    coverage_tools_url = clang_update.CDS_URL + '/Mac/' + coverage_tools_file
  elif host_platform == 'linux':
    coverage_tools_url = (
        clang_update.CDS_URL + '/Linux_x64/' + coverage_tools_file)
  else:
    assert host_platform == 'win'
    coverage_tools_url = (clang_update.CDS_URL + '/Win/' + coverage_tools_file)

  try:
    clang_update.DownloadAndUnpack(coverage_tools_url,
                                   clang_update.LLVM_BUILD_DIR)
    logging.info('Coverage tools %s unpacked', package_version)
    with open(coverage_revision_stamp_file, 'w') as file_handle:
      file_handle.write('%s,%s' % (package_version, host_platform))
      file_handle.write('\n')
  except urllib2.URLError:
    raise Exception(
        'Failed to download coverage tools: %s.' % coverage_tools_url)


def _GeneratePerFileLineByLineCoverageInHtml(binary_paths, profdata_file_path,
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
  logging.debug('Generating per file line by line coverage reports using '
                '"llvm-cov show" command')
  subprocess_cmd = [
      LLVM_COV_PATH, 'show', '-format=html',
      '-output-dir={}'.format(OUTPUT_DIR),
      '-instr-profile={}'.format(profdata_file_path), binary_paths[0]
  ]
  subprocess_cmd.extend(
      ['-object=' + binary_path for binary_path in binary_paths[1:]])
  _AddArchArgumentForIOSIfNeeded(subprocess_cmd, len(binary_paths))
  subprocess_cmd.extend(filters)
  subprocess.check_call(subprocess_cmd)
  logging.debug('Finished running "llvm-cov show" command')


def _GenerateFileViewHtmlIndexFile(per_file_coverage_summary):
  """Generates html index file for file view."""
  file_view_index_file_path = os.path.join(OUTPUT_DIR, FILE_VIEW_INDEX_FILE)
  logging.debug('Generating file view html index file as: "%s".',
                file_view_index_file_path)
  html_generator = _CoverageReportHtmlGenerator(file_view_index_file_path,
                                                'Path')
  totals_coverage_summary = _CoverageSummary()

  for file_path in per_file_coverage_summary:
    totals_coverage_summary.AddSummary(per_file_coverage_summary[file_path])

    html_generator.AddLinkToAnotherReport(
        _GetCoverageHtmlReportPathForFile(file_path),
        os.path.relpath(file_path, SRC_ROOT_PATH),
        per_file_coverage_summary[file_path])

  html_generator.CreateTotalsEntry(totals_coverage_summary)
  html_generator.WriteHtmlCoverageReport()
  logging.debug('Finished generating file view html index file.')


def _CalculatePerDirectoryCoverageSummary(per_file_coverage_summary):
  """Calculates per directory coverage summary."""
  logging.debug('Calculating per-directory coverage summary')
  per_directory_coverage_summary = defaultdict(lambda: _CoverageSummary())

  for file_path in per_file_coverage_summary:
    summary = per_file_coverage_summary[file_path]
    parent_dir = os.path.dirname(file_path)
    while True:
      per_directory_coverage_summary[parent_dir].AddSummary(summary)

      if parent_dir == SRC_ROOT_PATH:
        break
      parent_dir = os.path.dirname(parent_dir)

  logging.debug('Finished calculating per-directory coverage summary')
  return per_directory_coverage_summary


def _GeneratePerDirectoryCoverageInHtml(per_directory_coverage_summary,
                                        per_file_coverage_summary):
  """Generates per directory coverage breakdown in html."""
  logging.debug('Writing per-directory coverage html reports')
  for dir_path in per_directory_coverage_summary:
    _GenerateCoverageInHtmlForDirectory(
        dir_path, per_directory_coverage_summary, per_file_coverage_summary)

  logging.debug('Finished writing per-directory coverage html reports')


def _GenerateCoverageInHtmlForDirectory(
    dir_path, per_directory_coverage_summary, per_file_coverage_summary):
  """Generates coverage html report for a single directory."""
  html_generator = _CoverageReportHtmlGenerator(
      _GetCoverageHtmlReportPathForDirectory(dir_path), 'Path')

  for entry_name in os.listdir(dir_path):
    entry_path = os.path.normpath(os.path.join(dir_path, entry_name))

    if entry_path in per_file_coverage_summary:
      entry_html_report_path = _GetCoverageHtmlReportPathForFile(entry_path)
      entry_coverage_summary = per_file_coverage_summary[entry_path]
    elif entry_path in per_directory_coverage_summary:
      entry_html_report_path = _GetCoverageHtmlReportPathForDirectory(
          entry_path)
      entry_coverage_summary = per_directory_coverage_summary[entry_path]
    else:
      # Any file without executable lines shouldn't be included into the report.
      # For example, OWNER and README.md files.
      continue

    html_generator.AddLinkToAnotherReport(entry_html_report_path,
                                          os.path.basename(entry_path),
                                          entry_coverage_summary)

  html_generator.CreateTotalsEntry(per_directory_coverage_summary[dir_path])
  html_generator.WriteHtmlCoverageReport()


def _GenerateDirectoryViewHtmlIndexFile():
  """Generates the html index file for directory view.

  Note that the index file is already generated under SRC_ROOT_PATH, so this
  file simply redirects to it, and the reason of this extra layer is for
  structural consistency with other views.
  """
  directory_view_index_file_path = os.path.join(OUTPUT_DIR,
                                                DIRECTORY_VIEW_INDEX_FILE)
  logging.debug('Generating directory view html index file as: "%s".',
                directory_view_index_file_path)
  src_root_html_report_path = _GetCoverageHtmlReportPathForDirectory(
      SRC_ROOT_PATH)
  _WriteRedirectHtmlFile(directory_view_index_file_path,
                         src_root_html_report_path)
  logging.debug('Finished generating directory view html index file.')


def _CalculatePerComponentCoverageSummary(component_to_directories,
                                          per_directory_coverage_summary):
  """Calculates per component coverage summary."""
  logging.debug('Calculating per-component coverage summary')
  per_component_coverage_summary = defaultdict(lambda: _CoverageSummary())

  for component in component_to_directories:
    for directory in component_to_directories[component]:
      absolute_directory_path = os.path.abspath(directory)
      if absolute_directory_path in per_directory_coverage_summary:
        per_component_coverage_summary[component].AddSummary(
            per_directory_coverage_summary[absolute_directory_path])

  logging.debug('Finished calculating per-component coverage summary')
  return per_component_coverage_summary


def _ExtractComponentToDirectoriesMapping():
  """Returns a mapping from components to directories."""
  component_mappings = json.load(urllib2.urlopen(COMPONENT_MAPPING_URL))
  directory_to_component = component_mappings['dir-to-component']

  component_to_directories = defaultdict(list)
  for directory in directory_to_component:
    component = directory_to_component[directory]
    component_to_directories[component].append(directory)

  return component_to_directories


def _GeneratePerComponentCoverageInHtml(per_component_coverage_summary,
                                        component_to_directories,
                                        per_directory_coverage_summary):
  """Generates per-component coverage reports in html."""
  logging.debug('Writing per-component coverage html reports.')
  for component in per_component_coverage_summary:
    _GenerateCoverageInHtmlForComponent(
        component, per_component_coverage_summary, component_to_directories,
        per_directory_coverage_summary)

  logging.debug('Finished writing per-component coverage html reports.')


def _GenerateCoverageInHtmlForComponent(
    component_name, per_component_coverage_summary, component_to_directories,
    per_directory_coverage_summary):
  """Generates coverage html report for a component."""
  component_html_report_path = _GetCoverageHtmlReportPathForComponent(
      component_name)
  component_html_report_dir = os.path.dirname(component_html_report_path)
  if not os.path.exists(component_html_report_dir):
    os.makedirs(component_html_report_dir)

  html_generator = _CoverageReportHtmlGenerator(component_html_report_path,
                                                'Path')

  for dir_path in component_to_directories[component_name]:
    dir_absolute_path = os.path.abspath(dir_path)
    if dir_absolute_path not in per_directory_coverage_summary:
      # Any directory without an excercised file shouldn't be included into the
      # report.
      continue

    html_generator.AddLinkToAnotherReport(
        _GetCoverageHtmlReportPathForDirectory(dir_path),
        os.path.relpath(dir_path, SRC_ROOT_PATH),
        per_directory_coverage_summary[dir_absolute_path])

  html_generator.CreateTotalsEntry(
      per_component_coverage_summary[component_name])
  html_generator.WriteHtmlCoverageReport()


def _GenerateComponentViewHtmlIndexFile(per_component_coverage_summary):
  """Generates the html index file for component view."""
  component_view_index_file_path = os.path.join(OUTPUT_DIR,
                                                COMPONENT_VIEW_INDEX_FILE)
  logging.debug('Generating component view html index file as: "%s".',
                component_view_index_file_path)
  html_generator = _CoverageReportHtmlGenerator(component_view_index_file_path,
                                                'Component')
  totals_coverage_summary = _CoverageSummary()

  for component in per_component_coverage_summary:
    totals_coverage_summary.AddSummary(
        per_component_coverage_summary[component])

    html_generator.AddLinkToAnotherReport(
        _GetCoverageHtmlReportPathForComponent(component), component,
        per_component_coverage_summary[component])

  html_generator.CreateTotalsEntry(totals_coverage_summary)
  html_generator.WriteHtmlCoverageReport()
  logging.debug('Finished generating component view html index file.')


def _OverwriteHtmlReportsIndexFile():
  """Overwrites the root index file to redirect to the default view."""
  html_index_file_path = os.path.join(OUTPUT_DIR,
                                      os.extsep.join(['index', 'html']))
  directory_view_index_file_path = os.path.join(OUTPUT_DIR,
                                                DIRECTORY_VIEW_INDEX_FILE)
  _WriteRedirectHtmlFile(html_index_file_path, directory_view_index_file_path)


def _WriteRedirectHtmlFile(from_html_path, to_html_path):
  """Writes a html file that redirects to another html file."""
  to_html_relative_path = _GetRelativePathToDirectoryOfFile(
      to_html_path, from_html_path)
  content = ("""
    <!DOCTYPE html>
    <html>
      <head>
        <!-- HTML meta refresh URL redirection -->
        <meta http-equiv="refresh" content="0; url=%s">
      </head>
    </html>""" % to_html_relative_path)
  with open(from_html_path, 'w') as f:
    f.write(content)


def _GetCoverageHtmlReportPathForFile(file_path):
  """Given a file path, returns the corresponding html report path."""
  assert os.path.isfile(file_path), '"%s" is not a file' % file_path
  html_report_path = os.extsep.join([os.path.abspath(file_path), 'html'])

  # '+' is used instead of os.path.join because both of them are absolute paths
  # and os.path.join ignores the first path.
  # TODO(crbug.com/809150): Think of a generic cross platform fix (Windows).
  return _GetCoverageReportRootDirPath() + html_report_path


def _GetCoverageHtmlReportPathForDirectory(dir_path):
  """Given a directory path, returns the corresponding html report path."""
  assert os.path.isdir(dir_path), '"%s" is not a directory' % dir_path
  html_report_path = os.path.join(
      os.path.abspath(dir_path), DIRECTORY_COVERAGE_HTML_REPORT_NAME)

  # '+' is used instead of os.path.join because both of them are absolute paths
  # and os.path.join ignores the first path.
  # TODO(crbug.com/809150): Think of a generic cross platform fix (Windows).
  return _GetCoverageReportRootDirPath() + html_report_path


def _GetCoverageHtmlReportPathForComponent(component_name):
  """Given a component, returns the corresponding html report path."""
  component_file_name = component_name.lower().replace('>', '-')
  html_report_name = os.extsep.join([component_file_name, 'html'])
  return os.path.join(_GetCoverageReportRootDirPath(), 'components',
                      html_report_name)


def _GetCoverageReportRootDirPath():
  """The root directory that contains all generated coverage html reports."""
  return os.path.join(os.path.abspath(OUTPUT_DIR), 'coverage')


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
    build_args = _GetBuildArgs()
    return 'use_goma' in build_args and build_args['use_goma'] == 'true'

  logging.info('Building %s', str(targets))
  if jobs_count is None and _IsGomaConfigured():
    jobs_count = DEFAULT_GOMA_JOBS

  subprocess_cmd = ['ninja', '-C', BUILD_DIR]
  if jobs_count is not None:
    subprocess_cmd.append('-j' + str(jobs_count))

  subprocess_cmd.extend(targets)
  subprocess.check_call(subprocess_cmd)
  logging.debug('Finished building %s', str(targets))


def _GetProfileRawDataPathsByExecutingCommands(targets, commands):
  """Runs commands and returns the relative paths to the profraw data files.

  Args:
    targets: A list of targets built with coverage instrumentation.
    commands: A list of commands used to run the targets.

  Returns:
    A list of relative paths to the generated profraw data files.
  """
  logging.debug('Executing the test commands')

  # Remove existing profraw data files.
  for file_or_dir in os.listdir(OUTPUT_DIR):
    if file_or_dir.endswith(PROFRAW_FILE_EXTENSION):
      os.remove(os.path.join(OUTPUT_DIR, file_or_dir))

  profraw_file_paths = []

  # Run all test targets to generate profraw data files.
  for target, command in zip(targets, commands):
    output_file_name = os.extsep.join([target + '_output', 'txt'])
    output_file_path = os.path.join(OUTPUT_DIR, output_file_name)
    logging.info('Running command: "%s", the output is redirected to "%s"',
                 command, output_file_path)

    if _IsIOSCommand(command):
      # On iOS platform, due to lack of write permissions, profraw files are
      # generated outside of the OUTPUT_DIR, and the exact paths are contained
      # in the output of the command execution.
      output = _ExecuteIOSCommand(target, command)
      profraw_file_paths.append(_GetProfrawDataFileByParsingOutput(output))
    else:
      # On other platforms, profraw files are generated inside the OUTPUT_DIR.
      output = _ExecuteCommand(target, command)

    with open(output_file_path, 'w') as output_file:
      output_file.write(output)

  logging.debug('Finished executing the test commands')

  if _IsIOS():
    return profraw_file_paths

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
  """Runs a single command and generates a profraw data file."""
  # Per Clang "Source-based Code Coverage" doc:
  #
  # "%p" expands out to the process ID.
  #
  # "%Nm" expands out to the instrumented binary's signature. When this pattern
  # is specified, the runtime creates a pool of N raw profiles which are used
  # for on-line profile merging. The runtime takes care of selecting a raw
  # profile from the pool, locking it, and updating it before the program exits.
  # If N is not specified (i.e the pattern is "%m"), it's assumed that N = 1.
  # N must be between 1 and 9. The merge pool specifier can only occur once per
  # filename pattern.
  #
  # "%p" is used when tests run in single process, however, it can't be used for
  # multi-process because each process produces an intermediate dump, which may
  # consume hundreds of gigabytes of disk space.
  #
  # For "%Nm", 4 is chosen because it creates some level of parallelism, but
  # it's not too big to consume too much computing resource or disk space.
  profile_pattern_string = '%p' if _IsFuzzerTarget(target) else '%4m'
  expected_profraw_file_name = os.extsep.join(
      [target, profile_pattern_string, PROFRAW_FILE_EXTENSION])
  expected_profraw_file_path = os.path.join(OUTPUT_DIR,
                                            expected_profraw_file_name)

  try:
    output = subprocess.check_output(
        shlex.split(command),
        env={'LLVM_PROFILE_FILE': expected_profraw_file_path})
  except subprocess.CalledProcessError as e:
    output = e.output
    logging.warning('Command: "%s" exited with non-zero return code', command)

  return output


def _IsFuzzerTarget(target):
  """Returns true if the target is a fuzzer target."""
  build_args = _GetBuildArgs()
  use_libfuzzer = ('use_libfuzzer' in build_args and
                   build_args['use_libfuzzer'] == 'true')
  return use_libfuzzer and target.endswith('_fuzzer')


def _ExecuteIOSCommand(target, command):
  """Runs a single iOS command and generates a profraw data file.

  iOS application doesn't have write access to folders outside of the app, so
  it's impossible to instruct the app to flush the profraw data file to the
  desired location. The profraw data file will be generated somewhere within the
  application's Documents folder, and the full path can be obtained by parsing
  the output.
  """
  assert _IsIOSCommand(command)

  # After running tests, iossim generates a profraw data file, it won't be
  # needed anyway, so dump it into the OUTPUT_DIR to avoid polluting the
  # checkout.
  iossim_profraw_file_path = os.path.join(
      OUTPUT_DIR, os.extsep.join(['iossim', PROFRAW_FILE_EXTENSION]))

  try:
    output = subprocess.check_output(
        shlex.split(command),
        env={'LLVM_PROFILE_FILE': iossim_profraw_file_path})
  except subprocess.CalledProcessError as e:
    # iossim emits non-zero return code even if tests run successfully, so
    # ignore the return code.
    output = e.output

  return output


def _GetProfrawDataFileByParsingOutput(output):
  """Returns the path to the profraw data file obtained by parsing the output.

  The output of running the test target has no format, but it is guaranteed to
  have a single line containing the path to the generated profraw data file.
  NOTE: This should only be called when target os is iOS.
  """
  assert _IsIOS()

  output_by_lines = ''.join(output).splitlines()
  profraw_file_pattern = re.compile('.*Coverage data at (.*coverage\.profraw).')

  for line in output_by_lines:
    result = profraw_file_pattern.match(line)
    if result:
      return result.group(1)

  assert False, ('No profraw data file was generated, did you call '
                 'coverage_util::ConfigureCoverageReportPath() in test setup? '
                 'Please refer to base/test/test_support_ios.mm for example.')


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
  logging.info('Creating the coverage profile data file')
  logging.debug('Merging profraw files to create profdata file')
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

  logging.debug('Finished merging profraw files')
  logging.info('Code coverage profile data is created as: %s',
               profdata_file_path)
  return profdata_file_path


def _GeneratePerFileCoverageSummary(binary_paths, profdata_file_path, filters):
  """Generates per file coverage summary using "llvm-cov export" command."""
  # llvm-cov export [options] -instr-profile PROFILE BIN [-object BIN,...]
  # [[-object BIN]] [SOURCES].
  # NOTE: For object files, the first one is specified as a positional argument,
  # and the rest are specified as keyword argument.
  logging.debug('Generating per-file code coverage summary using "llvm-cov '
                'export -summary-only" command')
  subprocess_cmd = [
      LLVM_COV_PATH, 'export', '-summary-only',
      '-instr-profile=' + profdata_file_path, binary_paths[0]
  ]
  subprocess_cmd.extend(
      ['-object=' + binary_path for binary_path in binary_paths[1:]])
  _AddArchArgumentForIOSIfNeeded(subprocess_cmd, len(binary_paths))
  subprocess_cmd.extend(filters)

  json_output = json.loads(subprocess.check_output(subprocess_cmd))
  assert len(json_output['data']) == 1
  files_coverage_data = json_output['data'][0]['files']

  per_file_coverage_summary = {}
  for file_coverage_data in files_coverage_data:
    file_path = file_coverage_data['filename']
    summary = file_coverage_data['summary']

    if summary['lines']['count'] == 0:
      continue

    per_file_coverage_summary[file_path] = _CoverageSummary(
        regions_total=summary['regions']['count'],
        regions_covered=summary['regions']['covered'],
        functions_total=summary['functions']['count'],
        functions_covered=summary['functions']['covered'],
        lines_total=summary['lines']['count'],
        lines_covered=summary['lines']['covered'])

  logging.debug('Finished generating per-file code coverage summary')
  return per_file_coverage_summary


def _AddArchArgumentForIOSIfNeeded(cmd_list, num_archs):
  """Appends -arch arguments to the command list if it's ios platform.

  iOS binaries are universal binaries, and require specifying the architecture
  to use, and one architecture needs to be specified for each binary.
  """
  if _IsIOS():
    cmd_list.extend(['-arch=x86_64'] * num_archs)


def _GetBinaryPath(command):
  """Returns a relative path to the binary to be run by the command.

  Currently, following types of commands are supported (e.g. url_unittests):
  1. Run test binary direcly: "out/coverage/url_unittests <arguments>"
  2. Use xvfb.
    2.1. "python testing/xvfb.py out/coverage/url_unittests <arguments>"
    2.2. "testing/xvfb.py out/coverage/url_unittests <arguments>"
  3. Use iossim to run tests on iOS platform, please refer to testing/iossim.mm
    for its usage.
    3.1. "out/Coverage-iphonesimulator/iossim
          <iossim_arguments> -c <app_arguments>
          out/Coverage-iphonesimulator/url_unittests.app"


  Args:
    command: A command used to run a target.

  Returns:
    A relative path to the binary.
  """
  xvfb_script_name = os.extsep.join(['xvfb', 'py'])

  command_parts = shlex.split(command)
  if os.path.basename(command_parts[0]) == 'python':
    assert os.path.basename(command_parts[1]) == xvfb_script_name, (
        'This tool doesn\'t understand the command: "%s"' % command)
    return command_parts[2]

  if os.path.basename(command_parts[0]) == xvfb_script_name:
    return command_parts[1]

  if _IsIOSCommand(command):
    # For a given application bundle, the binary resides in the bundle and has
    # the same name with the application without the .app extension.
    app_path = command_parts[-1].rstrip(os.path.sep)
    app_name = os.path.splitext(os.path.basename(app_path))[0]
    return os.path.join(app_path, app_name)

  return command_parts[0]


def _IsIOSCommand(command):
  """Returns true if command is used to run tests on iOS platform."""
  return os.path.basename(shlex.split(command)[0]) == 'iossim'


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
  build_args = _GetBuildArgs()

  if (CLANG_COVERAGE_BUILD_ARG not in build_args or
      build_args[CLANG_COVERAGE_BUILD_ARG] != 'true'):
    assert False, ('\'{} = true\' is required in args.gn.'
                  ).format(CLANG_COVERAGE_BUILD_ARG)


def _ValidateCurrentPlatformIsSupported():
  """Asserts that this script suports running on the current platform"""
  target_os = _GetTargetOS()
  if target_os:
    current_platform = target_os
  else:
    current_platform = _GetHostPlatform()

  assert current_platform in [
      'linux', 'mac', 'chromeos', 'ios'
  ], ('Coverage is only supported on linux, mac, chromeos and ios.')


def _GetBuildArgs():
  """Parses args.gn file and returns results as a dictionary.

  Returns:
    A dictionary representing the build args.
  """
  global _BUILD_ARGS
  if _BUILD_ARGS is not None:
    return _BUILD_ARGS

  _BUILD_ARGS = {}
  build_args_path = os.path.join(BUILD_DIR, 'args.gn')
  assert os.path.exists(build_args_path), ('"%s" is not a build directory, '
                                           'missing args.gn file.' % BUILD_DIR)
  with open(build_args_path) as build_args_file:
    build_args_lines = build_args_file.readlines()

  for build_arg_line in build_args_lines:
    build_arg_without_comments = build_arg_line.split('#')[0]
    key_value_pair = build_arg_without_comments.split('=')
    if len(key_value_pair) != 2:
      continue

    key = key_value_pair[0].strip()

    # Values are wrapped within a pair of double-quotes, so remove the leading
    # and trailing double-quotes.
    value = key_value_pair[1].strip().strip('"')
    _BUILD_ARGS[key] = value

  return _BUILD_ARGS


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


def _GetRelativePathToDirectoryOfFile(target_path, base_path):
  """Returns a target path relative to the directory of base_path.

  This method requires base_path to be a file, otherwise, one should call
  os.path.relpath directly.
  """
  assert os.path.dirname(base_path) != base_path, (
      'Base path: "%s" is a directory, please call os.path.relpath directly.' %
      base_path)
  base_dir = os.path.dirname(base_path)
  return os.path.relpath(target_path, base_dir)


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
      '-v',
      '--verbose',
      action='store_true',
      help='Prints additional output for diagnostics.')

  arg_parser.add_argument(
      '-l', '--log_file', type=str, help='Redirects logs to a file.')

  arg_parser.add_argument(
      'targets', nargs='+', help='The names of the test targets to run.')

  args = arg_parser.parse_args()
  return args


def Main():
  """Execute tool commands."""
  assert os.path.abspath(os.getcwd()) == SRC_ROOT_PATH, ('This script must be '
                                                         'called from the root '
                                                         'of checkout.')
  args = _ParseCommandArguments()
  global BUILD_DIR
  BUILD_DIR = args.build_dir
  global OUTPUT_DIR
  OUTPUT_DIR = args.output_dir

  assert len(args.targets) == len(args.command), ('Number of targets must be '
                                                  'equal to the number of test '
                                                  'commands.')

  # logging should be configured before it is used.
  log_level = logging.DEBUG if args.verbose else logging.INFO
  log_format = '[%(asctime)s %(levelname)s] %(message)s'
  log_file = args.log_file if args.log_file else None
  logging.basicConfig(filename=log_file, level=log_level, format=log_format)

  assert os.path.exists(BUILD_DIR), (
      'Build directory: {} doesn\'t exist. '
      'Please run "gn gen" to generate.').format(BUILD_DIR)
  _ValidateCurrentPlatformIsSupported()
  _ValidateBuildingWithClangCoverage()
  _VerifyTargetExecutablesAreInBuildDirectory(args.command)

  DownloadCoverageToolsIfNeeded()

  absolute_filter_paths = []
  if args.filters:
    absolute_filter_paths = _VerifyPathsAndReturnAbsolutes(args.filters)

  if not os.path.exists(OUTPUT_DIR):
    os.makedirs(OUTPUT_DIR)

  profdata_file_path = _CreateCoverageProfileDataForTargets(
      args.targets, args.command, args.jobs)
  binary_paths = [_GetBinaryPath(command) for command in args.command]

  logging.info('Generating code coverage report in html (this can take a while '
               'depending on size of target!)')
  per_file_coverage_summary = _GeneratePerFileCoverageSummary(
      binary_paths, profdata_file_path, absolute_filter_paths)
  _GeneratePerFileLineByLineCoverageInHtml(binary_paths, profdata_file_path,
                                           absolute_filter_paths)
  _GenerateFileViewHtmlIndexFile(per_file_coverage_summary)

  per_directory_coverage_summary = _CalculatePerDirectoryCoverageSummary(
      per_file_coverage_summary)
  _GeneratePerDirectoryCoverageInHtml(per_directory_coverage_summary,
                                      per_file_coverage_summary)
  _GenerateDirectoryViewHtmlIndexFile()

  component_to_directories = _ExtractComponentToDirectoriesMapping()
  per_component_coverage_summary = _CalculatePerComponentCoverageSummary(
      component_to_directories, per_directory_coverage_summary)
  _GeneratePerComponentCoverageInHtml(per_component_coverage_summary,
                                      component_to_directories,
                                      per_directory_coverage_summary)
  _GenerateComponentViewHtmlIndexFile(per_component_coverage_summary)

  # The default index file is generated only for the list of source files, needs
  # to overwrite it to display per directory coverage view by default.
  _OverwriteHtmlReportsIndexFile()

  html_index_file_path = 'file://' + os.path.abspath(
      os.path.join(OUTPUT_DIR, 'index.html'))
  logging.info('Index file for html report is generated as: %s',
               html_index_file_path)


if __name__ == '__main__':
  sys.exit(Main())
