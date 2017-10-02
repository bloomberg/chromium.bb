#!/usr/bin/python2
#
# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import print_function

import argparse
import logging
import os
import subprocess
import sys


HELP_MESSAGE = '''
This script helps to generate code coverage report. It uses Clang Source-based
Code coverage (https://clang.llvm.org/docs/SourceBasedCodeCoverage.html).

The output is a directory with HTML files that can be inspected via local web
server (e.g. "python -m SimpleHTTPServer").

In order to generate code coverage report, you need to build the target program
with "use_clang_coverage=true" GN flag. That flag is not compatible with the
following flags: "is_asan", "is_msan", etc and also with "optimize_for_fuzzing".

For Googlers, there are examples available at go/chrome-code-coverage-examples.

If you have any questions, please ask it on fuzzing@chromium.org.
'''

HTML_FILE_EXTENSION = '.html'

CHROME_SRC_PATH = os.path.dirname(os.path.dirname(os.path.dirname(
    os.path.abspath(__file__))))
LLVM_BUILD_PATH = os.path.join(CHROME_SRC_PATH, 'third_party', 'llvm-build')
LLVM_BIN_PATH = os.path.join(LLVM_BUILD_PATH, 'Release+Asserts', 'bin')
LLVM_COV_PATH = os.path.join(LLVM_BIN_PATH, 'llvm-cov')
LLVM_PROFDATA_PATH = os.path.join(LLVM_BIN_PATH, 'llvm-profdata')

LLVM_PROFILE_FILE_NAME = 'coverage.profraw'
LLVM_COVERAGE_FILE_NAME = 'coverage.profdata'

REPORT_FILENAME = 'report.html'

REPORT_TEMPLATE = '''<!DOCTYPE html>
<html>
<head><style>
th, td {{ border: 1px solid black; padding: 5px 10px; }}
</style></head>
<body>
{table_data}
</body></html>'''

SINGLE_FILE_START_MARKER = '<!doctype html><html>'
SINGLE_FILE_END_MARKER = '</body></html>'

SOURCE_FILENAME_START_MARKER = (
    "<body><div class='centered'><table><div class='source-name-title'><pre>")
SOURCE_FILENAME_END_MARKER = '</pre>'

STYLE_START_MARKER = '<style>'
STYLE_END_MARKER = '</style>'
STYLE_FILENAME = 'style.css'

ZERO_FUNCTION_FILE_TEXT = 'Files which contain no functions'


def CheckBuildInstrumentation(executable_path):
  """Verify that the given file has been built with coverage instrumentation."""
  with open(executable_path) as file_handle:
    data = file_handle.read()

  # For the threshold reference, tiny "Hello World" program has count of 34.
  if data.count('__llvm_profile') > 20:
    return

  logging.error('It looks like the target binary has been compilde without '
                'coverage instrumentation.')
  print('Have you used use_clang_coverage=true flag in GN args? [y/N]')
  answer = raw_input()
  if not answer.lower().startswith('y'):
    print('Exiting.')
    sys.exit(-1)


def CreateOutputDir(dir_path):
  """Create a directory for the script output files."""
  if not os.path.exists(dir_path):
    os.mkdir(dir_path)
    return

  if os.path.isdir(dir_path):
    logging.warning('%s already exists.' % dir_path)
    return

  logging.error('%s exists and does not point to a directory.' % dir_path)
  raise Exception('Invalid --output argument specified.')


def DownloadCoverageToolsIfNeeded():
  """Temporary solution to download llvm-profdata and llvm-cov tools."""
  # TODO(mmoroz): remove this function once tools get included to Clang bundle:
  # https://chromium-review.googlesource.com/c/chromium/src/+/688221
  clang_script_path = os.path.join(CHROME_SRC_PATH, 'tools', 'clang', 'scripts')
  sys.path.append(clang_script_path)
  import update as clang_update
  import urllib2

  def _GetRevisionFromStampFile(filepath):
    """Read the build stamp file created by tools/clang/scripts/update.py."""
    if not os.path.exists(filepath):
      return 0, 0
    with open(filepath) as file_handle:
      revision_stamp_data = file_handle.readline().strip()
    revision_stamp_data = revision_stamp_data.split('-')
    return int(revision_stamp_data[0]), int(revision_stamp_data[1])

  clang_revision, clang_sub_revision = _GetRevisionFromStampFile(
      clang_update.STAMP_FILE)

  coverage_revision_stamp_file =  os.path.join(
      os.path.dirname(clang_update.STAMP_FILE), 'cr_coverage_revision')
  coverage_revision, coverage_sub_revision = _GetRevisionFromStampFile(
      coverage_revision_stamp_file)

  if (coverage_revision == clang_revision and
      coverage_sub_revision == clang_sub_revision):
    # LLVM coverage tools are up to date, bail out.
    return

  package_version = '%d-%d' % (clang_revision, clang_sub_revision)
  coverage_tools_file = 'llvm-code-coverage-%s.tgz' % package_version

  # The code bellow follows the code from tools/clang/scripts/update.py.
  if sys.platform == 'win32' or sys.platform == 'cygwin':
    coverage_tools_url = clang_update.CDS_URL + '/Win/' + coverage_tools_file
  elif sys.platform == 'darwin':
    coverage_tools_url = clang_update.CDS_URL + '/Mac/' + coverage_tools_file
  else:
    assert sys.platform.startswith('linux')
    coverage_tools_url = (
        clang_update.CDS_URL + '/Linux_x64/' + coverage_tools_file)

  try:
    clang_update.DownloadAndUnpack(coverage_tools_url,
                                   clang_update.LLVM_BUILD_DIR)
    print('Coverage tools %s unpacked.' % package_version)
    with open(coverage_revision_stamp_file, 'w') as file_handle:
      file_handle.write(package_version)
      file_handle.write('\n')
  except urllib2.URLError:
    raise Exception(
        'Failed to download coverage tools: %s.' % coverage_tools_url)


def ExtractAndFixFilename(data, source_dir):
  """Extract full paths to source code files and replace with relative paths."""
  filename_start = data.find(SOURCE_FILENAME_START_MARKER)
  if filename_start == -1:
    logging.error('Failed to extract source code filename.')
    raise Exception('Failed to process coverage dump.')

  filename_start += len(SOURCE_FILENAME_START_MARKER)
  filename_end = data[filename_start:].find(SOURCE_FILENAME_END_MARKER)
  if filename_end == -1:
    logging.error('Failed to extract source code filename.')
    raise Exception('Failed to process coverage dump.')

  filename_end += filename_start

  filename = data[filename_start : filename_end]

  source_dir = os.path.abspath(source_dir)

  if not filename.startswith(source_dir):
    logging.error('Invalid source code path ("%s") specified.\n'
                  'Coverage dump refers to "%s".' % (source_dir, filename))
    raise Exception('Failed to process coverage dump.')

  filename = filename[len(source_dir) : ]
  filename = filename.lstrip('/\\')

  # Replace the filename with the shorter version.
  data = data[ : filename_start] + filename + data[filename_end : ]
  return filename, data


def GenerateReport(report_data):
  """Build HTML page with the summary report and links to individual files."""
  table_data = '<table>\n'
  report_lines = report_data.splitlines()

  # Write header.
  table_data += '  <tr>\n'
  for column in report_lines[0].split('  '):
    if not column:
      continue
    table_data += '    <th>%s</th>\n' % column
  table_data += '  </tr>\n'

  for line in report_lines[1:-1]:
    if not line or line.startswith('---'):
      continue

    if line.startswith(ZERO_FUNCTION_FILE_TEXT):
      table_data += '  <tr>\n'
      table_data += '    <td><b>%s</b></td>\n' % line
      table_data += '  </tr>\n'
      continue

    table_data += '  <tr>\n'

    columns = line.split()

    # First column is a file name, build a link.
    table_data += '    <td><a href="/%s">%s</a></td>\n' % (
        columns[0] + HTML_FILE_EXTENSION, columns[0])

    for column in line.split()[1:]:
      table_data += '    <td>%s</td>\n' % column
    table_data += '  </tr>\n'

  # Write the last "TOTAL" row.
  table_data += '  <tr style="font-weight:bold">\n'
  for column in report_lines[-1].split():
    table_data += '    <td>%s</td>\n' % column
  table_data += '  </tr>\n'
  table_data += '</table>\n'

  return REPORT_TEMPLATE.format(table_data=table_data)


def GenerateSources(executable_path, output_dir, source_dir, coverage_file):
  """Generate coverage visualization for source code files."""
  llvm_cov_command = [LLVM_COV_PATH, 'show', '-format=html', executable_path,
                      '-instr-profile=%s' % coverage_file]

  data = subprocess.check_output(llvm_cov_command)

  # Extract CSS style from the data.
  style_start = data.find(STYLE_START_MARKER)
  style_end = data.find(STYLE_END_MARKER)
  if style_end <= style_start or style_start == -1:
    logging.error('Failed to extract CSS style from coverage report.')
    raise Exception('Failed to process coverage dump.')

  style_data = data[style_start + len(STYLE_START_MARKER) : style_end]
  with open(os.path.join(output_dir, STYLE_FILENAME), 'w') as file_handle:
    file_handle.write(style_data)
  style_length = (
      len(style_data) + len(STYLE_START_MARKER) + len(STYLE_END_MARKER))

  # Extract every source code file. Use "ofset" to avoid creating new strings.
  offset = 0
  while True:
    file_start = data.find(SINGLE_FILE_START_MARKER, offset)
    if file_start == -1:
      break

    file_end = data.find(SINGLE_FILE_END_MARKER, offset)
    if file_end == -1:
      break

    file_end += len(SINGLE_FILE_END_MARKER)
    offset += file_end - file_start

    # Remove <style> as it's always the same and has been extracted separately.
    file_data = ReplaceStyleWithCss(data[file_start : file_end], style_length,
                                    STYLE_FILENAME)

    filename, file_data = ExtractAndFixFilename(file_data, source_dir)
    filepath = os.path.join(output_dir, filename)
    dirname = os.path.dirname(filepath)

    try:
      os.makedirs(dirname)
    except OSError:
      pass

    with open(filepath + HTML_FILE_EXTENSION, 'w') as file_handle:
      file_handle.write(file_data)


def GenerateSummary(executable_path, output_dir, coverage_file):
  """Generate code coverage summary report (i.e. a table with all files)."""
  llvm_cov_command = [LLVM_COV_PATH, 'report', executable_path,
                      '-instr-profile=%s' % coverage_file]

  data = subprocess.check_output(llvm_cov_command)
  report = GenerateReport(data)

  with open(os.path.join(output_dir, REPORT_FILENAME), 'w') as file_handle:
    file_handle.write(report)


def ProcessCoverageDump(profile_file, coverage_file):
  """Process and convert raw LLVM profile data into coverage data format."""
  print('Processing coverage dump and generating visualization.')
  merge_command = [LLVM_PROFDATA_PATH, 'merge', '-sparse', profile_file,
                   '-o', coverage_file]
  data = subprocess.check_output(merge_command)

  if not os.path.exists(coverage_file) or not os.path.getsize(coverage_file):
    logging.error(
        '%s is either not created or empty:\n%s' % (coverage_file, data))
    raise Exception('Failed to merge coverage information after command run.')


def ReplaceStyleWithCss(data, style_data_length, css_file_path):
  """Replace <style></style> data with the include of common style.css file."""
  style_start = data.find(STYLE_START_MARKER)
  # Since "style" data is always the same, try some optimization here.
  style_end = style_start + style_data_length
  if (style_end > len(data) or
      data[style_end - len(STYLE_END_MARKER) : style_end] != STYLE_END_MARKER):
    # Looks like our optimization has failed, find end of "style" data.
    style_end = data.find(STYLE_END_MARKER)
    if style_end <= style_start or style_start == -1:
      logging.error('Failed to extract CSS style from coverage report.')
      raise Exception('Failed to process coverage dump.')
    style_end += len(STYLE_END_MARKER)

  css_include = (
      '<link rel="stylesheet" type="text/css" href="/%s">' % css_file_path)
  result = '\n'.join([ data[ : style_start], css_include, data[style_end : ] ])
  return result


def RunCommand(command, profile_file):
  """Run the given command in order to generate raw LLVM profile data."""
  print('Running "%s".' % command)
  print('-' * 80)
  os.environ['LLVM_PROFILE_FILE'] = profile_file
  os.system(command)
  print('-' * 80)
  print('Finished command execution.')

  if not os.path.exists(profile_file) or not os.path.getsize(profile_file):
    logging.error('%s is either not created or empty.' % profile_file)
    raise Exception('Failed to dump coverage information during command run.')


def main():
  """The main routing for processing the arguments and generating coverage."""
  parser = argparse.ArgumentParser(
      description=HELP_MESSAGE,
      formatter_class=argparse.RawDescriptionHelpFormatter)
  parser.add_argument('--command', required=True,
                      help='The command to generate code coverage of')
  parser.add_argument('--source', required=False, default=CHROME_SRC_PATH,
                      help='Directory with the source code, if it differs from '
                           'local Chromium checkout: %s.' % CHROME_SRC_PATH)
  parser.add_argument('--output', required=True,
                      help='Directory where result will be written to.')

  args = parser.parse_args()

  executable_path = args.command.split()[0]
  CheckBuildInstrumentation(executable_path)

  DownloadCoverageToolsIfNeeded()

  CreateOutputDir(args.output)
  profile_file = os.path.join(args.output, LLVM_PROFILE_FILE_NAME)
  RunCommand(args.command, profile_file)

  coverage_file = os.path.join(args.output, LLVM_COVERAGE_FILE_NAME)
  ProcessCoverageDump(profile_file, coverage_file)

  GenerateSummary(executable_path, args.output, coverage_file)
  GenerateSources(executable_path, args.output, args.source, coverage_file)

  print('Done. The next steps would be:\n'
        '1. cd %s && python -m SimpleHTTPServer\n'
        '2. open http://127.0.0.1:8000/report.html' % args.output)


if __name__ == "__main__":
  main()
