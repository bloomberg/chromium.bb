# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Main Python API for analyzing binary size."""

import argparse
import calendar
import collections
import datetime
import gzip
import logging
import os
import re
import subprocess
import sys

import describe
import file_format
import function_signature
import helpers
import linker_map_parser
import models
import ninja_parser
import paths


def _OpenMaybeGz(path, mode=None):
  """Calls `gzip.open()` if |path| ends in ".gz", otherwise calls `open()`."""
  if path.endswith('.gz'):
    if mode and 'w' in mode:
      return gzip.GzipFile(path, mode, 1)
    return gzip.open(path, mode)
  return open(path, mode or 'r')


def _UnmangleRemainingSymbols(symbols, tool_prefix):
  """Uses c++filt to unmangle any symbols that need it."""
  to_process = [s for s in symbols if s.name.startswith('_Z')]
  if not to_process:
    return

  logging.info('Unmangling %d names', len(to_process))
  proc = subprocess.Popen([tool_prefix + 'c++filt'], stdin=subprocess.PIPE,
                          stdout=subprocess.PIPE)
  stdout = proc.communicate('\n'.join(s.name for s in to_process))[0]
  assert proc.returncode == 0

  for i, line in enumerate(stdout.splitlines()):
    to_process[i].name = line


def _NormalizeNames(symbols):
  """Ensures that all names are formatted in a useful way.

  This includes:
    - Assigning of |full_name|.
    - Stripping of return types in |full_name| and |name| (for functions).
    - Stripping parameters from |name|.
    - Moving "vtable for" and the like to be suffixes rather than prefixes.
  """
  found_prefixes = set()
  for symbol in symbols:
    if symbol.name.startswith('*'):
      # See comment in _CalculatePadding() about when this
      # can happen.
      continue

    # E.g.: vtable for FOO
    idx = symbol.name.find(' for ', 0, 30)
    if idx != -1:
      found_prefixes.add(symbol.name[:idx + 4])
      symbol.name = symbol.name[idx + 5:] + ' [' + symbol.name[:idx] + ']'

    # E.g.: virtual thunk to FOO
    idx = symbol.name.find(' to ', 0, 30)
    if idx != -1:
      found_prefixes.add(symbol.name[:idx + 3])
      symbol.name = symbol.name[idx + 4:] + ' [' + symbol.name[:idx] + ']'

    # Strip out return type, and identify where parameter list starts.
    if symbol.section == 't':
      symbol.full_name, symbol.name = function_signature.Parse(symbol.name)

    # Remove anonymous namespaces (they just harm clustering).
    non_anonymous = symbol.name.replace('(anonymous namespace)::', '')
    if symbol.name != non_anonymous:
      symbol.is_anonymous = True
      symbol.name = non_anonymous
      symbol.full_name = symbol.full_name.replace(
          '(anonymous namespace)::', '')

    if symbol.section != 't' and '(' in symbol.name:
      # Pretty rare. Example:
      # blink::CSSValueKeywordsHash::findValueImpl(char const*)::value_word_list
      symbol.full_name = symbol.name
      symbol.name = re.sub(r'\(.*\)', '', symbol.full_name)

    # Don't bother storing both if they are the same.
    if symbol.full_name == symbol.name:
      symbol.full_name = ''

  logging.debug('Found name prefixes of: %r', found_prefixes)


def _NormalizeObjectPaths(symbols):
  """Ensures that all paths are formatted in a useful way."""
  for symbol in symbols:
    path = symbol.object_path
    if path.startswith('obj/'):
      # Convert obj/third_party/... -> third_party/...
      path = path[4:]
    elif path.startswith('../../'):
      # Convert ../../third_party/... -> third_party/...
      path = path[6:]
    if path.endswith(')'):
      # Convert foo/bar.a(baz.o) -> foo/bar.a/baz.o
      start_idx = path.index('(')
      path = os.path.join(path[:start_idx], path[start_idx + 1:-1])
    symbol.object_path = path


def _NormalizeSourcePath(path):
  if path.startswith('gen/'):
    # Convert gen/third_party/... -> third_party/...
    return path[4:]
  if path.startswith('../../'):
    # Convert ../../third_party/... -> third_party/...
    return path[6:]
  return path


def _ExtractSourcePaths(symbols, output_directory):
  """Fills in the .source_path attribute of all symbols.

  Returns True if source paths were found.
  """
  mapper = ninja_parser.SourceFileMapper(output_directory)
  not_found_paths = set()

  for symbol in symbols:
    object_path = symbol.object_path
    if symbol.source_path or not object_path:
      continue
    # We don't have source info for prebuilt .a files.
    if not os.path.isabs(object_path) and not object_path.startswith('..'):
      source_path = mapper.FindSourceForPath(object_path)
      if source_path:
        symbol.source_path = _NormalizeSourcePath(source_path)
      elif object_path not in not_found_paths:
        not_found_paths.add(object_path)
        logging.warning('Could not find source path for %s', object_path)
  logging.debug('Parsed %d .ninja files.', mapper.GetParsedFileCount())
  return len(not_found_paths) == 0


def _CalculatePadding(symbols):
  """Populates the |padding| field based on symbol addresses.

  Symbols must already be sorted by |address|.
  """
  seen_sections = []
  for i, symbol in enumerate(symbols[1:]):
    prev_symbol = symbols[i]
    if prev_symbol.section_name != symbol.section_name:
      assert symbol.section_name not in seen_sections, (
          'Input symbols must be sorted by section, then address.')
      seen_sections.append(symbol.section_name)
      continue
    if symbol.address <= 0 or prev_symbol.address <= 0:
      continue
    # Padding-only symbols happen for ** symbol gaps.
    prev_is_padding_only = prev_symbol.size_without_padding == 0
    if symbol.address == prev_symbol.address and not prev_is_padding_only:
      assert False, 'Found duplicate symbols:\n%r\n%r' % (prev_symbol, symbol)
    # Even with symbols at the same address removed, overlaps can still
    # happen. In this case, padding will be negative (and this is fine).
    padding = symbol.address - prev_symbol.end_address
    # These thresholds were found by manually auditing arm32 Chrome.
    # E.g.: Set them to 0 and see what warnings get logged.
    # TODO(agrieve): See if these thresholds make sense for architectures
    #     other than arm32.
    if not symbol.name.startswith('*') and (
        symbol.section in 'rd' and padding >= 256 or
        symbol.section in 't' and padding >= 64):
      # For nm data, this is caused by data that has no associated symbol.
      # The linker map file lists them with no name, but with a file.
      # Example:
      #   .data 0x02d42764 0x120 .../V8SharedWorkerGlobalScope.o
      # Where as most look like:
      #   .data.MANGLED_NAME...
      logging.debug('Large padding of %d between:\n  A) %r\n  B) %r' % (
                    padding, prev_symbol, symbol))
      continue
    symbol.padding = padding
    symbol.size += padding
    assert symbol.size >= 0, (
        'Symbol has negative size (likely not sorted propertly): '
        '%r\nprev symbol: %r' % (symbol, prev_symbol))


def _ClusterSymbols(symbols):
  """Returns a new list of symbols with some symbols moved into groups.

  Groups include:
   * Symbols that have [clone] in their name (created by compiler optimization).
   * Star symbols (such as "** merge strings", and "** symbol gap")
  """
  # http://unix.stackexchange.com/questions/223013/function-symbol-gets-part-suffix-after-compilation
  # Example name suffixes:
  #     [clone .part.322]
  #     [clone .isra.322]
  #     [clone .constprop.1064]

  # Step 1: Create name map, find clones, collect star syms into replacements.
  logging.debug('Creating name -> symbol map')
  clone_indices = []
  indices_by_full_name = {}
  # (name, full_name) -> [(index, sym),...]
  replacements_by_name = collections.defaultdict(list)
  for i, symbol in enumerate(symbols):
    if symbol.name.startswith('**'):
      # "symbol gap 3" -> "symbol gaps"
      name = re.sub(r'\s+\d+$', 's', symbol.name)
      replacements_by_name[(name, None)].append((i, symbol))
    elif symbol.full_name:
      if symbol.full_name.endswith(']') and ' [clone ' in symbol.full_name:
        clone_indices.append(i)
      else:
        indices_by_full_name[symbol.full_name] = i

  # Step 2: Collect same-named clone symbols.
  logging.debug('Grouping all clones')
  group_names_by_index = {}
  for i in clone_indices:
    symbol = symbols[i]
    # Multiple attributes could exist, so search from left-to-right.
    stripped_name = symbol.name[:symbol.name.index(' [clone ')]
    stripped_full_name = symbol.full_name[:symbol.full_name.index(' [clone ')]
    name_tup = (stripped_name, stripped_full_name)
    replacement_list = replacements_by_name[name_tup]

    if not replacement_list:
      # First occurance, check for non-clone symbol.
      non_clone_idx = indices_by_full_name.get(stripped_name)
      if non_clone_idx is not None:
        non_clone_symbol = symbols[non_clone_idx]
        replacement_list.append((non_clone_idx, non_clone_symbol))
        group_names_by_index[non_clone_idx] = stripped_name

    replacement_list.append((i, symbol))
    group_names_by_index[i] = stripped_name

  # Step 3: Undo clustering when length=1.
  # Removing these groups means Diff() logic must know about [clone] suffix.
  to_clear = []
  for name_tup, replacement_list in replacements_by_name.iteritems():
    if len(replacement_list) == 1:
      to_clear.append(name_tup)
  for name_tup in to_clear:
    del replacements_by_name[name_tup]

  # Step 4: Replace first symbol from each cluster with a SymbolGroup.
  before_symbol_count = sum(len(x) for x in replacements_by_name.itervalues())
  logging.debug('Creating %d symbol groups from %d symbols. %d clones had only '
                'one symbol.', len(replacements_by_name), before_symbol_count,
                len(to_clear))

  len_delta = len(replacements_by_name) - before_symbol_count
  grouped_symbols = [None] * (len(symbols) + len_delta)
  dest_index = 0
  src_index = 0
  seen_names = set()
  replacement_names_by_index = {}
  for name_tup, replacement_list in replacements_by_name.iteritems():
    for tup in replacement_list:
      replacement_names_by_index[tup[0]] = name_tup

  sorted_items = replacement_names_by_index.items()
  sorted_items.sort(key=lambda tup: tup[0])
  for index, name_tup in sorted_items:
    count = index - src_index
    grouped_symbols[dest_index:dest_index + count] = (
        symbols[src_index:src_index + count])
    src_index = index + 1
    dest_index += count
    if name_tup not in seen_names:
      seen_names.add(name_tup)
      group_symbols = [tup[1] for tup in replacements_by_name[name_tup]]
      grouped_symbols[dest_index] = models.SymbolGroup(
          group_symbols, name=name_tup[0], full_name=name_tup[1],
          section_name=group_symbols[0].section_name)
      dest_index += 1

  assert len(grouped_symbols[dest_index:None]) == len(symbols[src_index:None])
  grouped_symbols[dest_index:None] = symbols[src_index:None]
  logging.debug('Finished making groups.')
  return grouped_symbols


def LoadAndPostProcessSizeInfo(path):
  """Returns a SizeInfo for the given |path|."""
  logging.debug('Loading results from: %s', path)
  size_info = file_format.LoadSizeInfo(path)
  _PostProcessSizeInfo(size_info)
  return size_info


def _PostProcessSizeInfo(size_info):
  logging.info('Normalizing symbol names')
  _NormalizeNames(size_info.raw_symbols)
  logging.info('Calculating padding')
  _CalculatePadding(size_info.raw_symbols)
  logging.info('Grouping decomposed functions')
  size_info.symbols = models.SymbolGroup(
      _ClusterSymbols(size_info.raw_symbols))
  logging.info('Processed %d symbols', len(size_info.raw_symbols))


def CreateSizeInfo(map_path, lazy_paths=None, no_source_paths=False,
                   raw_only=False):
  """Creates a SizeInfo from the given map file."""
  if not no_source_paths:
    # output_directory needed for source file information.
    lazy_paths.VerifyOutputDirectory()
  # tool_prefix needed for c++filt.
  lazy_paths.VerifyToolPrefix()

  with _OpenMaybeGz(map_path) as map_file:
    section_sizes, raw_symbols = (
        linker_map_parser.MapFileParser().Parse(map_file))

  if not no_source_paths:
    logging.info('Extracting source paths from .ninja files')
    all_found = _ExtractSourcePaths(raw_symbols, lazy_paths.output_directory)
    assert all_found, (
        'One or more source file paths could not be found. Likely caused by '
        '.ninja files being generated at a different time than the .map file.')
  # Map file for some reason doesn't unmangle all names.
  # Unmangle prints its own log statement.
  _UnmangleRemainingSymbols(raw_symbols, lazy_paths.tool_prefix)
  logging.info('Normalizing object paths')
  _NormalizeObjectPaths(raw_symbols)
  size_info = models.SizeInfo(section_sizes, raw_symbols)

  # Name normalization not strictly required, but makes for smaller files.
  if raw_only:
    logging.info('Normalizing symbol names')
    _NormalizeNames(size_info.raw_symbols)
  else:
    _PostProcessSizeInfo(size_info)

  if logging.getLogger().isEnabledFor(logging.DEBUG):
    for line in describe.DescribeSizeInfoCoverage(size_info):
      logging.info(line)
  logging.info('Recorded info for %d symbols', len(size_info.raw_symbols))
  return size_info


def _DetectGitRevision(directory):
  try:
    git_rev = subprocess.check_output(
        ['git', '-C', directory, 'rev-parse', 'HEAD'])
    return git_rev.rstrip()
  except Exception:
    logging.warning('Failed to detect git revision for file metadata.')
    return None


def BuildIdFromElf(elf_path, tool_prefix):
  args = [tool_prefix + 'readelf', '-n', elf_path]
  stdout = subprocess.check_output(args)
  match = re.search(r'Build ID: (\w+)', stdout)
  assert match, 'Build ID not found from running: ' + ' '.join(args)
  return match.group(1)


def _SectionSizesFromElf(elf_path, tool_prefix):
  args = [tool_prefix + 'readelf', '-S', '--wide', elf_path]
  stdout = subprocess.check_output(args)
  section_sizes = {}
  # Matches  [ 2] .hash HASH 00000000006681f0 0001f0 003154 04   A  3   0  8
  for match in re.finditer(r'\[[\s\d]+\] (\..*)$', stdout, re.MULTILINE):
    items = match.group(1).split()
    section_sizes[items[0]] = int(items[4], 16)
  return section_sizes


def _ParseGnArgs(args_path):
  """Returns a list of normalized "key=value" strings."""
  args = {}
  with open(args_path) as f:
    for l in f:
      # Strips #s even if within string literal. Not a problem in practice.
      parts = l.split('#')[0].split('=')
      if len(parts) != 2:
        continue
      args[parts[0].strip()] = parts[1].strip()
  return ["%s=%s" % x for x in sorted(args.iteritems())]


def AddArguments(parser):
  parser.add_argument('size_file', help='Path to output .size file.')
  parser.add_argument('--elf-file', required=True,
                      help='Path to input ELF file. Currently used for '
                           'capturing metadata. Pass "" to skip '
                           'metadata collection.')
  parser.add_argument('--map-file',
                      help='Path to input .map(.gz) file. Defaults to '
                           '{{elf_file}}.map(.gz)?')
  parser.add_argument('--no-source-paths', action='store_true',
                      help='Do not use .ninja files to map '
                           'object_path -> source_path')
  parser.add_argument('--tool-prefix', default='',
                      help='Path prefix for c++filt.')
  parser.add_argument('--output-directory',
                      help='Path to the root build directory.')


def Run(args, parser):
  if not args.size_file.endswith('.size'):
    parser.error('size_file must end with .size')

  if args.map_file:
    if (not args.map_file.endswith('.map')
        and not args.map_file.endswith('.map.gz')):
      parser.error('Expected --map-file to end with .map or .map.gz')
    map_file_path = args.map_file
  else:
    map_file_path = args.elf_file + '.map'
    if not os.path.exists(map_file_path):
      map_file_path += '.gz'
    if not os.path.exists(map_file_path):
      parser.error('Could not find .map(.gz)? file. Use --map-file.')

  lazy_paths = paths.LazyPaths(args=args, input_file=args.elf_file)
  metadata = None
  if args.elf_file:
    logging.debug('Constructing metadata')
    git_rev = _DetectGitRevision(os.path.dirname(args.elf_file))
    build_id = BuildIdFromElf(args.elf_file, lazy_paths.tool_prefix)
    timestamp_obj = datetime.datetime.utcfromtimestamp(os.path.getmtime(
        args.elf_file))
    timestamp = calendar.timegm(timestamp_obj.timetuple())
    gn_args = _ParseGnArgs(os.path.join(lazy_paths.output_directory, 'args.gn'))

    def relative_to_out(path):
      return os.path.relpath(path, lazy_paths.VerifyOutputDirectory())

    metadata = {
        models.METADATA_GIT_REVISION: git_rev,
        models.METADATA_MAP_FILENAME: relative_to_out(map_file_path),
        models.METADATA_ELF_FILENAME: relative_to_out(args.elf_file),
        models.METADATA_ELF_MTIME: timestamp,
        models.METADATA_ELF_BUILD_ID: build_id,
        models.METADATA_GN_ARGS: gn_args,
    }

  size_info = CreateSizeInfo(map_file_path, lazy_paths,
                             no_source_paths=args.no_source_paths,
                             raw_only=True)

  if metadata:
    size_info.metadata = metadata
    logging.debug('Validating section sizes')
    elf_section_sizes = _SectionSizesFromElf(args.elf_file,
                                             lazy_paths.tool_prefix)
    for k, v in elf_section_sizes.iteritems():
      assert v == size_info.section_sizes.get(k), (
          'ELF file and .map file do not match.')

  logging.info('Recording metadata: \n  %s',
               '\n  '.join(describe.DescribeMetadata(size_info.metadata)))
  logging.info('Saving result to %s', args.size_file)
  file_format.SaveSizeInfo(size_info, args.size_file)
  logging.info('Done')
