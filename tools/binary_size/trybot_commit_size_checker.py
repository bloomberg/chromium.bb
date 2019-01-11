#!/usr/bin/env python
# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Creates several files used by the size trybot to monitor size regressions."""

import argparse
import collections
import json
import logging
import os
import sys

sys.path.append(os.path.join(os.path.dirname(__file__), 'libsupersize'))
import archive
import diagnose_bloat
import diff
import describe
import html_report
import models

_NDJSON_FILENAME = 'supersize_diff.ndjson'
_HTML_REPORT_BASE_URL = (
    'https://storage.googleapis.com/chrome-supersize/viewer.html?load_url=')
_MAX_DEX_METHOD_COUNT_INCREASE = 50
_MAX_NORMALIZED_INCREASE = 16 * 1024
_MAX_PAK_INCREASE = 1024

_DEX_DETAILS = 'Refer to Dex Method Diff for list of added/removed methods.'
_NORMALIZED_APK_SIZE_DETAILS = (
    'See https://chromium.googlesource.com/chromium/src/+/master/docs/speed/'
    'binary_size/metrics.md#Normalized-APK-Size '
    'for an explanation of Normalized APK Size')

_FAILURE_GUIDANCE = """
Please look at size breakdowns, try to understand the growth, and see if it can
be mitigated.

There is guidance at:

https://chromium.googlesource.com/chromium/src/+/master/docs/speed/apk_size_regressions.md#Debugging-Apk-Size-Increase

If the growth is expected / justified, then you can bypass this bot failure by
adding "Binary-Size: $JUSTIFICATION" footer to your commit message (must go at
the bottom of the message, similar to "Bug:").

Here are some examples:

Binary-Size: Increase is due to translations and so cannot be avoided.
Binary-Size: Increase is due to new images, which are already optimally encoded.
Binary-Size: Increase is temporary due to a "new way" / "old way" refactoring.
    It should go away once the "old way" is removed.
Binary-Size: Increase is temporary and will be reverted before next branch cut.
Binary-Size: Increase needed to reduce RAM of a common user flow.
Binary-Size: Increase needed to reduce runtime of a common user flow.
Binary-Size: Increase needed to implement a feature, and I've already spent a
    non-trivial amount of time trying to reduce its size.
"""


class _SizeDelta(collections.namedtuple(
    'SizeDelta', ['name', 'units', 'expected', 'actual', 'details'])):

  @property
  def explanation(self):
    ret = '{}: {} {} (max is {} {})'.format(
        self.name, self.actual, self.units, self.expected, self.units)
    if self.details and not self.IsAllowable():
      ret += '\n' + self.details
    return ret

  def IsAllowable(self):
    return self.actual <= self.expected

  def __cmp__(self, other):
    return cmp(self.name, other.name)


def _CreateAndWriteMethodCountDelta(symbols):
  dex_symbols = symbols.WhereInSection(models.SECTION_DEX_METHOD)
  dex_added = dex_symbols.WhereDiffStatusIs(models.DIFF_STATUS_ADDED)
  dex_removed = dex_symbols.WhereDiffStatusIs(models.DIFF_STATUS_REMOVED)
  dex_added_count, dex_removed_count = len(dex_added), len(dex_removed)
  dex_net_added = dex_added_count - dex_removed_count

  lines = ['Added: {}'.format(dex_added_count)]
  lines.extend(sorted(s.full_name for s in dex_added))
  lines.append('')
  lines.append('Removed: {}'.format(dex_removed_count))
  lines.extend(sorted(s.full_name for s in dex_removed))

  return lines, _SizeDelta('Dex Methods', 'methods',
                           _MAX_DEX_METHOD_COUNT_INCREASE, dex_net_added,
                           _DEX_DETAILS)


def _CreateAndWriteResourceSizesDelta(apk_name, before_dir, after_dir):
  sizes_diff = diagnose_bloat.ResourceSizesDiff(apk_name)
  sizes_diff.ProduceDiff(before_dir, after_dir)

  return sizes_diff.Summary(), _SizeDelta(
      'Normalized APK Size', 'bytes', _MAX_NORMALIZED_INCREASE,
      sizes_diff.summary_stat.value, _NORMALIZED_APK_SIZE_DETAILS)


def _CreateAndWriteSupersizeDiff(apk_name, before_dir, after_dir):
  before_size_path = os.path.join(before_dir, apk_name + '.size')
  after_size_path = os.path.join(after_dir, apk_name + '.size')
  before = archive.LoadAndPostProcessSizeInfo(before_size_path)
  after = archive.LoadAndPostProcessSizeInfo(after_size_path)
  size_info_delta = diff.Diff(before, after, sort=True)

  lines = list(describe.GenerateLines(size_info_delta))
  return lines, size_info_delta


def _CreateUncompressedPakSizeDeltas(symbols):
  pak_symbols = symbols.Filter(lambda s:
      s.size > 0 and
      bool(s.flags & models.FLAG_UNCOMPRESSED) and
      s.section_name == models.SECTION_PAK_NONTRANSLATED)
  return [
      _SizeDelta('Uncompressed Pak Entry "{}"'.format(pak.full_name), 'bytes',
                 _MAX_PAK_INCREASE, pak.after_symbol.size, None)
      for pak in pak_symbols
  ]


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('--author', required=True, help='CL author')
  parser.add_argument(
      '--apk-name', required=True, help='Name of the apk (ex. Name.apk)')
  parser.add_argument(
      '--before-dir',
      required=True,
      help='Directory containing the APK from reference build.')
  parser.add_argument(
      '--after-dir',
      required=True,
      help='Directory containing APK for the new build.')
  parser.add_argument(
      '--results-path',
      required=True,
      help='Output path for the trybot result .json file.')
  parser.add_argument(
      '--staging-dir',
      required=True,
      help='Directory to write summary files to.')
  parser.add_argument('-v', '--verbose', action='store_true')
  args = parser.parse_args()

  if args.verbose:
    logging.basicConfig(level=logging.INFO)

  logging.info('Creating Supersize diff')
  supersize_diff_lines, delta_size_info = _CreateAndWriteSupersizeDiff(
      args.apk_name, args.before_dir, args.after_dir)

  changed_symbols = delta_size_info.raw_symbols.WhereDiffStatusIs(
      models.DIFF_STATUS_UNCHANGED).Inverted()

  # Monitor dex method growth since this correlates closely with APK size and
  # may affect our dex file structure.
  logging.info('Checking dex symbols')
  dex_delta_lines, dex_delta = _CreateAndWriteMethodCountDelta(changed_symbols)
  size_deltas = {dex_delta}

  # Check for uncompressed .pak file entries being added to avoid unnecessary
  # bloat.
  logging.info('Checking pak symbols')
  size_deltas.update(_CreateUncompressedPakSizeDeltas(changed_symbols))

  # Normalized APK Size is the main metric we use to monitor binary size.
  logging.info('Creating sizes diff')
  resource_sizes_lines, resource_sizes_delta = (
      _CreateAndWriteResourceSizesDelta(args.apk_name, args.before_dir,
                                        args.after_dir))
  size_deltas.add(resource_sizes_delta)

  # .ndjson can be consumed by the html viewer.
  logging.info('Creating HTML Report')
  ndjson_path = os.path.join(args.staging_dir, _NDJSON_FILENAME)
  html_report.BuildReportFromSizeInfo(ndjson_path, delta_size_info)

  passing_deltas = set(m for m in size_deltas if m.IsAllowable())
  failing_deltas = size_deltas - passing_deltas

  is_roller = '-autoroll' in args.author
  failing_checks_text = '\n'.join(d.explanation for d in sorted(failing_deltas))
  passing_checks_text = '\n'.join(d.explanation for d in sorted(passing_deltas))
  checks_text = """\
FAILING:
{}

PASSING:
{}
""".format(failing_checks_text, passing_checks_text)

  if failing_deltas:
    checks_text += _FAILURE_GUIDANCE

  status_code = 1 if failing_deltas and not is_roller else 0
  summary = '<br>' + '<br>'.join(resource_sizes_lines)
  if 'Empty Resource Sizes Diff' in summary:
    summary = '<br>No size metrics were affected.'
  if failing_deltas:
    summary += '<br><br>Failed Size Checks:<br>'
    summary += failing_checks_text.replace('\n', '<br>')
    summary += '<br>Look at "Size Assertion Results" for guidance.'

  results_json = {
      'status_code': status_code,
      'summary': summary,
      'archive_filenames': [_NDJSON_FILENAME],
      'links': [
          {
              'name': '>>> Size Assertion Results <<<',
              'lines': checks_text.splitlines(),
          },
          {
              'name': '>>> Dex Method Diff <<<',
              'lines': dex_delta_lines,
          },
          {
              'name': '>>> SuperSize Text Diff <<<',
              'lines': supersize_diff_lines,
          },
          {
              'name': '>>> Supersize HTML Diff <<<',
              'url': _HTML_REPORT_BASE_URL + '{{' + _NDJSON_FILENAME + '}}',
          },
      ],
  }
  with open(args.results_path, 'w') as f:
    json.dump(results_json, f)


if __name__ == '__main__':
  main()
