# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Methods for converting model objects to human-readable formats."""

import datetime
import itertools
import time

import models


def _PrettySize(size):
  # Arbitrarily chosen cut-off.
  if abs(size) < 2000:
    return '%d bytes' % size
  # Always show 3 digits.
  size /= 1024.0
  if abs(size) < 10:
    return '%.2fkb' % size
  elif abs(size) < 100:
    return '%.1fkb' % size
  elif abs(size) < 1024:
    return '%dkb' % size
  size /= 1024.0
  if abs(size) < 10:
    return '%.2fmb' % size
  # We shouldn't be seeing sizes > 100mb.
  return '%.1fmb' % size


class Describer(object):
  def __init__(self, verbose=False):
    self.verbose = verbose

  def _DescribeSectionSizes(self, section_sizes):
    relevant_names = models.SECTION_TO_SECTION_NAME.values()
    section_names = sorted(k for k in section_sizes.iterkeys()
                           if k in relevant_names or k.startswith('.data'))
    total_bytes = sum(v for k, v in section_sizes.iteritems()
                      if k in section_names and k != '.bss')
    yield 'Section Sizes (Total={:,} bytes):'.format(total_bytes)
    for name in section_names:
      size = section_sizes[name]
      if name == '.bss':
        yield '{}: {:,} bytes (not included in totals)'.format(name, size)
      else:
        percent = float(size) / total_bytes if total_bytes else 0
        yield '{}: {:,} bytes ({:.1%})'.format(name, size, percent)

    if self.verbose:
      yield ''
      yield 'Other section sizes:'
      section_names = sorted(k for k in section_sizes.iterkeys()
                             if k not in section_names)
      for name in section_names:
        yield '{}: {:,} bytes'.format(name, section_sizes[name])

  def _DescribeSymbol(self, sym):
    # SymbolGroups are passed here when we don't want to expand them.
    if sym.IsGroup():
      if self.verbose:
        yield ('{} {:<8} {} (count={})  padding={}  '
               'size_without_padding={}').format(
                    sym.section, sym.size, sym.name, len(sym), sym.padding,
                    sym.size_without_padding)
      else:
        yield '{} {:<8} {} (count={})'.format(sym.section, sym.size, sym.name,
                                              len(sym))
      return

    if self.verbose:
      yield '{}@0x{:<8x}  size={}  padding={}  size_without_padding={}'.format(
          sym.section, sym.address, sym.size, sym.padding,
          sym.size_without_padding)
      yield '    source_path={} \tobject_path={}'.format(
          sym.source_path, sym.object_path)
      if sym.full_name:
        yield '    is_anonymous={}  full_name={}'.format(
            int(sym.is_anonymous), sym.full_name)
      if sym.name:
        yield '    is_anonymous={}  name={}'.format(
            int(sym.is_anonymous), sym.name)
    else:
      yield '{}@0x{:<8x}  {:<7} {}'.format(
          sym.section, sym.address, sym.size,
          sym.source_path or sym.object_path or '{no path}')
      if sym.name:
        yield '    {}'.format(sym.name)

  def _DescribeSymbolGroup(self, group, prefix_func=None):
    total_size = group.size
    yield 'Showing {:,} symbols with total size: {} bytes'.format(
        len(group), total_size)
    code_syms = group.WhereInSection('t')
    code_size = code_syms.size
    ro_size = code_syms.Inverted().WhereInSection('r').size
    yield '.text={:<10} .rodata={:<10} other={:<10} total={}'.format(
        _PrettySize(code_size), _PrettySize(ro_size),
        _PrettySize(total_size - code_size - ro_size),
        _PrettySize(total_size))
    yield 'First columns are: running total, type, size'

    running_total = 0
    prefix = ''
    sorted_syms = group if group.is_sorted else group.Sorted()

    prefix = ''
    for s in sorted_syms:
      if group.IsBss() or not s.IsBss():
        running_total += s.size
      for l in self._DescribeSymbol(s):
        if l[:4].isspace():
          yield '{} {}'.format(' ' * (8 + len(prefix)), l)
        else:
          if prefix_func:
            prefix = prefix_func(s)
          yield '{}{:8} {}'.format(prefix, running_total, l)

  def _DescribeSymbolDiff(self, diff):
    template = ('{} symbols added (+), {} changed (~), {} removed (-), '
                '{} unchanged ({})')
    unchanged_msg = '=' if self.verbose else 'not shown'
    header_str = (template.format(
            diff.added_count, diff.changed_count, diff.removed_count,
            diff.unchanged_count, unchanged_msg))

    def prefix_func(sym):
      if diff.IsAdded(sym):
        return '+ '
      if diff.IsRemoved(sym):
        return '- '
      if sym.size:
        return '~ '
      return '= '

    diff = diff if self.verbose else diff.WhereNotUnchanged()
    group_desc = self._DescribeSymbolGroup(diff, prefix_func=prefix_func)
    return itertools.chain((header_str,), group_desc)

  def GenerateLines(self, obj):
    if isinstance(obj, models.SizeInfo):
      metadata_desc = 'Metadata: %s' % DescribeSizeInfoMetadata(obj)
      section_desc = self._DescribeSectionSizes(obj.section_sizes)
      group_desc = self.GenerateLines(obj.symbols)
      return itertools.chain((metadata_desc,), section_desc, ('',), group_desc)

    if isinstance(obj, models.SymbolDiff):
      return self._DescribeSymbolDiff(obj)

    if isinstance(obj, models.SymbolGroup):
      return self._DescribeSymbolGroup(obj)

    if isinstance(obj, models.Symbol):
      return self._DescribeSymbol(obj)

    return (repr(obj),)


def DescribeSizeInfoCoverage(size_info):
  """Yields lines describing how accurate |size_info| is."""
  for section in models.SECTION_TO_SECTION_NAME:
    if section == 'd':
      expected_size = sum(v for k, v in size_info.section_sizes.iteritems()
                          if k.startswith('.data'))
    else:
      expected_size = size_info.section_sizes[
          models.SECTION_TO_SECTION_NAME[section]]

    def one_stat(group):
      template = ('Section %s has %.1f%% of %d bytes accounted for from '
                  '%d symbols. %d bytes are unaccounted for. Padding '
                  'accounts for %d bytes')
      actual_size = group.size
      count = len(group)
      padding = group.padding
      size_percent = 100.0 * actual_size / expected_size
      return (template % (section, size_percent, actual_size, count,
                          expected_size - actual_size, padding))

    in_section = size_info.symbols.WhereInSection(section)
    yield one_stat(in_section)

    star_syms = in_section.WhereNameMatches(r'^\*')
    attributed_syms = star_syms.Inverted().WhereHasAnyAttribution()
    anonymous_syms = attributed_syms.Inverted()
    if star_syms or anonymous_syms:
      missing_size = star_syms.size + anonymous_syms.size
      yield ('+ Without %d merge sections and %d anonymous entries ('
                  'accounting for %d bytes):') % (
          len(star_syms),  len(anonymous_syms), missing_size)
      yield '+ ' + one_stat(attributed_syms)


def _UtcToLocal(utc):
  epoch = time.mktime(utc.timetuple())
  offset = (datetime.datetime.fromtimestamp(epoch) -
            datetime.datetime.utcfromtimestamp(epoch))
  return utc + offset


def DescribeSizeInfoMetadata(size_info):
  display_dict = size_info.metadata.copy()
  timestamp = display_dict.get(models.METADATA_ELF_MTIME)
  if timestamp:
    timestamp_obj = datetime.datetime.utcfromtimestamp(timestamp)
    display_dict[models.METADATA_ELF_MTIME] = (
        _UtcToLocal(timestamp_obj).strftime('%Y-%m-%d %H:%M:%S'))
  return ' '.join(sorted('%s=%s' % t for t in display_dict.iteritems()))


def GenerateLines(obj, verbose=False):
  return Describer(verbose).GenerateLines(obj)


def WriteLines(lines, func):
  for l in lines:
    func(l)
    func('\n')
