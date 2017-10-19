# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Main Python API for analyzing binary size."""

import argparse
import calendar
import collections
import datetime
import gzip
import itertools
import logging
import os
import posixpath
import re
import subprocess
import sys
import tempfile
import zipfile

import concurrent
import describe
import file_format
import function_signature
import linker_map_parser
import models
import ninja_parser
import nm
import paths


# Effect of _MAX_SAME_NAME_ALIAS_COUNT (as of Oct 2017, with min_pss = max):
# 1: shared .text symbols = 1772874 bytes, file size = 9.43MiB (645476 symbols).
# 2: shared .text symbols = 1065654 bytes, file size = 9.58MiB (669952 symbols).
# 6: shared .text symbols = 464058 bytes, file size = 10.11MiB (782693 symbols).
# 10: shared .text symbols = 365648 bytes, file size =10.24MiB (813758 symbols).
# 20: shared .text symbols = 86202 bytes, file size = 10.38MiB (854548 symbols).
# 40: shared .text symbols = 48424 bytes, file size = 10.50MiB (890396 symbols).
# 50: shared .text symbols = 41860 bytes, file size = 10.54MiB (902304 symbols).
# max: shared .text symbols = 0 bytes, file size = 11.10MiB (1235449 symbols).
_MAX_SAME_NAME_ALIAS_COUNT = 40  # 50kb is basically negligable.


def _OpenMaybeGz(path, mode=None):
  """Calls `gzip.open()` if |path| ends in ".gz", otherwise calls `open()`."""
  if path.endswith('.gz'):
    if mode and 'w' in mode:
      return gzip.GzipFile(path, mode, 1)
    return gzip.open(path, mode)
  return open(path, mode or 'r')


def _StripLinkerAddedSymbolPrefixes(raw_symbols):
  """Removes prefixes sometimes added to symbol names during link

  Removing prefixes make symbol names match up with those found in .o files.
  """
  for symbol in raw_symbols:
    full_name = symbol.full_name
    if full_name.startswith('startup.'):
      symbol.flags |= models.FLAG_STARTUP
      symbol.full_name = full_name[8:]
    elif full_name.startswith('unlikely.'):
      symbol.flags |= models.FLAG_UNLIKELY
      symbol.full_name = full_name[9:]
    elif full_name.startswith('rel.local.'):
      symbol.flags |= models.FLAG_REL_LOCAL
      symbol.full_name = full_name[10:]
    elif full_name.startswith('rel.'):
      symbol.flags |= models.FLAG_REL
      symbol.full_name = full_name[4:]


def _UnmangleRemainingSymbols(raw_symbols, tool_prefix):
  """Uses c++filt to unmangle any symbols that need it."""
  to_process = [s for s in raw_symbols if s.full_name.startswith('_Z')]
  if not to_process:
    return

  logging.info('Unmangling %d names', len(to_process))
  proc = subprocess.Popen([tool_prefix + 'c++filt'], stdin=subprocess.PIPE,
                          stdout=subprocess.PIPE)
  stdout = proc.communicate('\n'.join(s.full_name for s in to_process))[0]
  assert proc.returncode == 0

  for i, line in enumerate(stdout.splitlines()):
    to_process[i].full_name = line


def _NormalizeNames(raw_symbols):
  """Ensures that all names are formatted in a useful way.

  This includes:
    - Deriving |name| and |template_name| from |full_name|.
    - Stripping of return types (for functions).
    - Moving "vtable for" and the like to be suffixes rather than prefixes.
  """
  found_prefixes = set()
  for symbol in raw_symbols:
    full_name = symbol.full_name
    if full_name.startswith('*'):
      # See comment in _CalculatePadding() about when this
      # can happen.
      symbol.template_name = full_name
      symbol.name = full_name
      continue

    # Remove [clone] suffix, and set flag accordingly.
    # Search from left-to-right, as multiple [clone]s can exist.
    # Example name suffixes:
    #     [clone .part.322]  # GCC
    #     [clone .isra.322]  # GCC
    #     [clone .constprop.1064]  # GCC
    #     [clone .11064]  # clang
    # http://unix.stackexchange.com/questions/223013/function-symbol-gets-part-suffix-after-compilation
    idx = full_name.find(' [clone ')
    if idx != -1:
      full_name = full_name[:idx]
      symbol.flags |= models.FLAG_CLONE

    # Clones for C symbols.
    if symbol.section == 't':
      idx = full_name.rfind('.')
      if idx != -1 and full_name[idx + 1:].isdigit():
        new_name = full_name[:idx]
        # Generated symbols that end with .123 but are not clones.
        # Find these via:
        #   size_info.symbols.WhereInSection('t').WhereIsGroup().SortedByCount()
        if new_name not in ('__tcf_0', 'startup'):
          full_name = new_name
          symbol.flags |= models.FLAG_CLONE
          # Remove .part / .isra / .constprop.
          idx = full_name.rfind('.', 0, idx)
          if idx != -1:
            full_name = full_name[:idx]

    # E.g.: vtable for FOO
    idx = full_name.find(' for ', 0, 30)
    if idx != -1:
      found_prefixes.add(full_name[:idx + 4])
      full_name = '{} [{}]'.format(full_name[idx + 5:], full_name[:idx])

    # E.g.: virtual thunk to FOO
    idx = full_name.find(' to ', 0, 30)
    if idx != -1:
      found_prefixes.add(full_name[:idx + 3])
      full_name = '{} [{}]'.format(full_name[idx + 4:], full_name[:idx])

    # Strip out return type, and split out name, template_name.
    # Function parsing also applies to non-text symbols. E.g. Function statics.
    symbol.full_name, symbol.template_name, symbol.name = (
        function_signature.Parse(full_name))

    # Remove anonymous namespaces (they just harm clustering).
    symbol.template_name = symbol.template_name.replace(
        '(anonymous namespace)::', '')
    symbol.full_name = symbol.full_name.replace(
        '(anonymous namespace)::', '')
    non_anonymous_name = symbol.name.replace('(anonymous namespace)::', '')
    if symbol.name != non_anonymous_name:
      symbol.flags |= models.FLAG_ANONYMOUS
      symbol.name = non_anonymous_name

    # Allow using "is" to compare names (and should help with RAM).
    function_signature.InternSameNames(symbol)

  logging.debug('Found name prefixes of: %r', found_prefixes)


def _NormalizeObjectPath(path):
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
  return path


def _NormalizeSourcePath(path):
  """Returns (is_generated, normalized_path)"""
  if path.startswith('gen/'):
    # Convert gen/third_party/... -> third_party/...
    return True, path[4:]
  if path.startswith('../../'):
    # Convert ../../third_party/... -> third_party/...
    return False, path[6:]
  return True, path


def _ExtractSourcePathsAndNormalizeObjectPaths(raw_symbols, source_mapper):
  """Fills in the |source_path| attribute and normalizes |object_path|."""
  if source_mapper:
    logging.info('Looking up source paths from ninja files')
    for symbol in raw_symbols:
      object_path = symbol.object_path
      if object_path:
        # We don't have source info for prebuilt .a files.
        if not os.path.isabs(object_path) and not object_path.startswith('..'):
          source_path = source_mapper.FindSourceForPath(object_path)
          if source_path:
            symbol.generated_source, symbol.source_path = (
                _NormalizeSourcePath(source_path))
        symbol.object_path = _NormalizeObjectPath(object_path)
    assert source_mapper.unmatched_paths_count == 0, (
        'One or more source file paths could not be found. Likely caused by '
        '.ninja files being generated at a different time than the .map file.')
  else:
    logging.info('Normalizing object paths')
    for symbol in raw_symbols:
      if symbol.object_path:
        symbol.object_path = _NormalizeObjectPath(symbol.object_path)


def _ComputeAncestorPath(path_list, symbol_count):
  """Returns the common ancestor of the given paths."""
  if not path_list:
    return ''

  prefix = os.path.commonprefix(path_list)
  # Check if all paths were the same.
  if prefix == path_list[0]:
    return prefix

  # Put in buckets to cut down on the number of unique paths.
  if symbol_count >= 100:
    symbol_count_str = '100+'
  elif symbol_count >= 50:
    symbol_count_str = '50-99'
  elif symbol_count >= 20:
    symbol_count_str = '20-49'
  elif symbol_count >= 10:
    symbol_count_str = '10-19'
  else:
    symbol_count_str = str(symbol_count)

  # Put the path count as a subdirectory so that grouping by path will show
  # "{shared}" as a bucket, and the symbol counts as leafs.
  if not prefix:
    return os.path.join('{shared}', symbol_count_str)
  return os.path.join(os.path.dirname(prefix), '{shared}', symbol_count_str)


def _CompactLargeAliasesIntoSharedSymbols(raw_symbols):
  """Converts symbols with large number of aliases into single symbols.

  The merged symbol's path fields are changed to common-ancestor paths in
  the form: common/dir/{shared}/$SYMBOL_COUNT

  Assumes aliases differ only by path (not by name).
  """
  num_raw_symbols = len(raw_symbols)
  num_shared_symbols = 0
  src_cursor = 0
  dst_cursor = 0
  while src_cursor < num_raw_symbols:
    symbol = raw_symbols[src_cursor]
    raw_symbols[dst_cursor] = symbol
    dst_cursor += 1
    aliases = symbol.aliases
    if aliases and len(aliases) > _MAX_SAME_NAME_ALIAS_COUNT:
      symbol.source_path = _ComputeAncestorPath(
          [s.source_path for s in aliases if s.source_path], len(aliases))
      symbol.object_path = _ComputeAncestorPath(
          [s.object_path for s in aliases if s.object_path], len(aliases))
      symbol.generated_source = all(s.generated_source for s in aliases)
      symbol.aliases = None
      num_shared_symbols += 1
      src_cursor += len(aliases)
    else:
      src_cursor += 1
  raw_symbols[dst_cursor:] = []
  num_removed = src_cursor - dst_cursor
  logging.debug('Converted %d aliases into %d shared-path symbols',
                num_removed, num_shared_symbols)


def _ConnectNmAliases(raw_symbols):
  """Ensures |aliases| is set correctly for all symbols."""
  prev_sym = raw_symbols[0]
  for sym in raw_symbols[1:]:
    # Don't merge bss symbols.
    if sym.address > 0 and prev_sym.address == sym.address:
      # Don't merge padding-only symbols (** symbol gaps).
      if prev_sym.size > 0:
        # Don't merge if already merged.
        if prev_sym.aliases is None or prev_sym.aliases is not sym.aliases:
          if prev_sym.aliases:
            prev_sym.aliases.append(sym)
          else:
            prev_sym.aliases = [prev_sym, sym]
          sym.aliases = prev_sym.aliases
    prev_sym = sym


def _AssignNmAliasPathsAndCreatePathAliases(raw_symbols, object_paths_by_name):
  num_found_paths = 0
  num_unknown_names = 0
  num_path_mismatches = 0
  num_aliases_created = 0
  ret = []
  for symbol in raw_symbols:
    ret.append(symbol)
    full_name = symbol.full_name
    if (symbol.IsBss() or
        not full_name or
        full_name[0] in '*.' or  # e.g. ** merge symbols, .Lswitch.table
        full_name == 'startup'):
      continue

    object_paths = object_paths_by_name.get(full_name)
    if object_paths:
      num_found_paths += 1
    else:
      if num_unknown_names < 10:
        logging.warning('Symbol not found in any .o files: %r', symbol)
      num_unknown_names += 1
      continue

    if symbol.object_path and symbol.object_path not in object_paths:
      if num_path_mismatches < 10:
        logging.warning('Symbol path reported by .map not found by nm.')
        logging.warning('sym=%r', symbol)
        logging.warning('paths=%r', object_paths)
      object_paths.append(symbol.object_path)
      object_paths.sort()
      num_path_mismatches += 1

    symbol.object_path = object_paths[0]

    if len(object_paths) > 1:
      # Create one symbol for each object_path.
      aliases = symbol.aliases or [symbol]
      symbol.aliases = aliases
      num_aliases_created += len(object_paths) - 1
      for object_path in object_paths[1:]:
        new_sym = models.Symbol(
            symbol.section_name, symbol.size, address=symbol.address,
            full_name=full_name, object_path=object_path, aliases=aliases)
        aliases.append(new_sym)
        ret.append(new_sym)

  logging.debug('Cross-referenced %d symbols with nm output. '
                'num_unknown_names=%d num_path_mismatches=%d '
                'num_aliases_created=%d',
                num_found_paths, num_unknown_names, num_path_mismatches,
                num_aliases_created)
  return ret


def _DiscoverMissedObjectPaths(raw_symbols, elf_object_paths):
  # Missing object paths are caused by .a files added by -l flags, which are not
  # listed as explicit inputs within .ninja rules.
  parsed_inputs = set(elf_object_paths)
  missed_inputs = set()
  for symbol in raw_symbols:
    path = symbol.object_path
    if path.endswith(')'):
      # Convert foo/bar.a(baz.o) -> foo/bar.a
      path = path[:path.index('(')]
    if path and path not in parsed_inputs:
      missed_inputs.add(path)
  return missed_inputs


def _CreateMergeStringsReplacements(merge_string_syms,
                                    list_of_positions_by_object_path):
  """Creates replacement symbols for |merge_syms|."""
  ret = []
  NAME_PREFIX = models.STRING_LITERAL_NAME_PREFIX
  assert len(merge_string_syms) == len(list_of_positions_by_object_path)
  tups = itertools.izip((str(x) for x in xrange(len(merge_string_syms))),
                        merge_string_syms,
                        list_of_positions_by_object_path)
  for name_suffix, merge_sym, positions_by_object_path in tups:
    merge_sym_address = merge_sym.address
    new_symbols = []
    ret.append(new_symbols)
    for object_path, positions in positions_by_object_path.iteritems():
      for offset, size in positions:
        address = merge_sym_address + offset
        symbol = models.Symbol(
            '.rodata', size, address, NAME_PREFIX + name_suffix,
            object_path=object_path)
        new_symbols.append(symbol)

  logging.debug('Created %d string literal symbols', sum(len(x) for x in ret))
  logging.debug('Sorting string literals')
  for symbols in ret:
    symbols.sort(key=lambda x: x.address)

  logging.debug('Deduping string literals')
  num_removed = 0
  size_removed = 0
  num_aliases = 0
  for i, symbols in enumerate(ret):
    if not symbols:
      continue
    prev_symbol = symbols[0]
    new_symbols = [prev_symbol]
    for symbol in symbols[1:]:
      padding = symbol.address - prev_symbol.end_address
      if (prev_symbol.address == symbol.address and
          prev_symbol.size == symbol.size):
        # String is an alias.
        num_aliases += 1
        aliases = prev_symbol.aliases
        if aliases:
          aliases.append(symbol)
          symbol.aliases = aliases
        else:
          aliases = [prev_symbol, symbol]
          prev_symbol.aliases = aliases
          symbol.aliases = aliases
      elif padding + symbol.size <= 0:
        # String is a substring of prior one.
        num_removed += 1
        size_removed += symbol.size
        continue
      elif padding < 0:
        # String overlaps previous one. Adjust to not overlap.
        symbol.address -= padding
        symbol.size += padding
      new_symbols.append(symbol)
      prev_symbol = symbol
    ret[i] = new_symbols
    # Aliases come out in random order, so sort to be deterministic.
    ret[i].sort(key=lambda s: (s.address, s.object_path))

  logging.debug(
      'Removed %d overlapping string literals (%d bytes) & created %d aliases',
                num_removed, size_removed, num_aliases)
  return ret


def _CalculatePadding(raw_symbols):
  """Populates the |padding| field based on symbol addresses.

  Symbols must already be sorted by |address|.
  """
  seen_sections = []
  for i, symbol in enumerate(raw_symbols[1:]):
    prev_symbol = raw_symbols[i]
    if prev_symbol.section_name != symbol.section_name:
      assert symbol.section_name not in seen_sections, (
          'Input symbols must be sorted by section, then address.')
      seen_sections.append(symbol.section_name)
      continue
    if symbol.address <= 0 or prev_symbol.address <= 0:
      continue

    if symbol.address == prev_symbol.address:
      if symbol.aliases and symbol.aliases is prev_symbol.aliases:
        symbol.padding = prev_symbol.padding
        symbol.size = prev_symbol.size
        continue
      # Padding-only symbols happen for ** symbol gaps.
      assert prev_symbol.size_without_padding == 0, (
          'Found duplicate symbols:\n%r\n%r' % (prev_symbol, symbol))

    padding = symbol.address - prev_symbol.end_address
    # These thresholds were found by experimenting with arm32 Chrome.
    # E.g.: Set them to 0 and see what warnings get logged, then take max value.
    # TODO(agrieve): See if these thresholds make sense for architectures
    #     other than arm32.
    if (not symbol.full_name.startswith('*') and
        not symbol.full_name.startswith(models.STRING_LITERAL_NAME_PREFIX) and (
        symbol.section in 'rd' and padding >= 256 or
        symbol.section in 't' and padding >= 64)):
      # Should not happen.
      logging.warning('Large padding of %d between:\n  A) %r\n  B) %r' % (
                      padding, prev_symbol, symbol))
    symbol.padding = padding
    symbol.size += padding
    assert symbol.size >= 0, (
        'Symbol has negative size (likely not sorted propertly): '
        '%r\nprev symbol: %r' % (symbol, prev_symbol))


def _AddNmAliases(raw_symbols, names_by_address):
  """Adds symbols that were removed by identical code folding."""
  # Step 1: Create list of (index_of_symbol, name_list).
  logging.debug('Creating alias list')
  replacements = []
  num_new_symbols = 0
  for i, s in enumerate(raw_symbols):
    # Don't alias padding-only symbols (e.g. ** symbol gap)
    if s.size_without_padding == 0:
      continue
    name_list = names_by_address.get(s.address)
    if name_list:
      if s.full_name not in name_list:
        logging.warning('Name missing from aliases: %s %s', s.full_name,
                        name_list)
        continue
      replacements.append((i, name_list))
      num_new_symbols += len(name_list) - 1

  if float(num_new_symbols) / len(raw_symbols) < .05:
    logging.warning('Number of aliases is oddly low (%.0f%%). It should '
                    'usually be around 25%%. Ensure --tool-prefix is correct. ',
                    float(num_new_symbols) / len(raw_symbols) * 100)

  # Step 2: Create new symbols as siblings to each existing one.
  logging.debug('Creating %d new symbols from nm output', num_new_symbols)
  src_cursor_end = len(raw_symbols)
  raw_symbols += [None] * num_new_symbols
  dst_cursor_end = len(raw_symbols)
  for src_index, name_list in reversed(replacements):
    # Copy over symbols that come after the current one.
    chunk_size = src_cursor_end - src_index - 1
    dst_cursor_end -= chunk_size
    src_cursor_end -= chunk_size
    raw_symbols[dst_cursor_end:dst_cursor_end + chunk_size] = (
        raw_symbols[src_cursor_end:src_cursor_end + chunk_size])
    sym = raw_symbols[src_index]
    src_cursor_end -= 1

    # Create symbols (does not bother reusing the existing symbol).
    for i, full_name in enumerate(name_list):
      dst_cursor_end -= 1
      # Do not set |aliases| in order to avoid being pruned by
      # _CompactLargeAliasesIntoSharedSymbols(), which assumes aliases differ
      # only by path. The field will be set afterwards by _ConnectNmAliases().
      raw_symbols[dst_cursor_end] = models.Symbol(
          sym.section_name, sym.size, address=sym.address, full_name=full_name)

  assert dst_cursor_end == src_cursor_end


def LoadAndPostProcessSizeInfo(path):
  """Returns a SizeInfo for the given |path|."""
  logging.debug('Loading results from: %s', path)
  size_info = file_format.LoadSizeInfo(path)
  logging.info('Normalizing symbol names')
  _NormalizeNames(size_info.raw_symbols)
  logging.info('Calculating padding')
  _CalculatePadding(size_info.raw_symbols)
  logging.info('Loaded %d symbols', len(size_info.raw_symbols))
  return size_info


def CreateMetadata(map_path, elf_path, apk_path, tool_prefix, output_directory):
  metadata = None
  if elf_path:
    logging.debug('Constructing metadata')
    git_rev = _DetectGitRevision(os.path.dirname(elf_path))
    architecture = _ArchFromElf(elf_path, tool_prefix)
    build_id = BuildIdFromElf(elf_path, tool_prefix)
    timestamp_obj = datetime.datetime.utcfromtimestamp(os.path.getmtime(
        elf_path))
    timestamp = calendar.timegm(timestamp_obj.timetuple())
    relative_tool_prefix = paths.ToSrcRootRelative(tool_prefix)

    metadata = {
        models.METADATA_GIT_REVISION: git_rev,
        models.METADATA_ELF_ARCHITECTURE: architecture,
        models.METADATA_ELF_MTIME: timestamp,
        models.METADATA_ELF_BUILD_ID: build_id,
        models.METADATA_TOOL_PREFIX: relative_tool_prefix,
    }

    if output_directory:
      relative_to_out = lambda path: os.path.relpath(path, output_directory)
      gn_args = _ParseGnArgs(os.path.join(output_directory, 'args.gn'))
      metadata[models.METADATA_MAP_FILENAME] = relative_to_out(map_path)
      metadata[models.METADATA_ELF_FILENAME] = relative_to_out(elf_path)
      metadata[models.METADATA_GN_ARGS] = gn_args

      if apk_path:
        metadata[models.METADATA_APK_FILENAME] = relative_to_out(apk_path)
  return metadata


def CreateSizeInfo(map_path, elf_path, tool_prefix, output_directory,
                   normalize_names=True, track_string_literals=True):
  """Creates a SizeInfo.

  Args:
    map_path: Path to the linker .map(.gz) file to parse.
    elf_path: Path to the corresponding unstripped ELF file. Used to find symbol
        aliases and inlined functions. Can be None.
    tool_prefix: Prefix for c++filt & nm (required).
    output_directory: Build output directory. If None, source_paths and symbol
        alias information will not be recorded.
    normalize_names: Whether to normalize symbol names.
    track_string_literals: Whether to break down "** merge string" sections into
        smaller symbols (requires output_directory).
  """
  source_mapper = None
  if output_directory:
    # Start by finding the elf_object_paths, so that nm can run on them while
    # the linker .map is being parsed.
    logging.info('Parsing ninja files.')
    source_mapper, elf_object_paths = ninja_parser.Parse(
        output_directory, elf_path)
    logging.debug('Parsed %d .ninja files.', source_mapper.parsed_file_count)
    assert not elf_path or elf_object_paths, (
        'Failed to find link command in ninja files for ' +
        os.path.relpath(elf_path, output_directory))

  if elf_path:
    # Run nm on the elf file to retrieve the list of symbol names per-address.
    # This list is required because the .map file contains only a single name
    # for each address, yet multiple symbols are often coalesced when they are
    # identical. This coalescing happens mainly for small symbols and for C++
    # templates. Such symbols make up ~500kb of libchrome.so on Android.
    elf_nm_result = nm.CollectAliasesByAddressAsync(elf_path, tool_prefix)

    # Run nm on all .o/.a files to retrieve the symbol names within them.
    # The list is used to detect when mutiple .o files contain the same symbol
    # (e.g. inline functions), and to update the object_path / source_path
    # fields accordingly.
    # Looking in object files is required because the .map file choses a
    # single path for these symbols.
    # Rather than record all paths for each symbol, set the paths to be the
    # common ancestor of all paths.
    if output_directory:
      bulk_analyzer = nm.BulkObjectFileAnalyzer(tool_prefix, output_directory)
      bulk_analyzer.AnalyzePaths(elf_object_paths)

  logging.info('Parsing Linker Map')
  with _OpenMaybeGz(map_path) as map_file:
    section_sizes, raw_symbols = (
        linker_map_parser.MapFileParser().Parse(map_file))

  if elf_path:
    logging.debug('Validating section sizes')
    elf_section_sizes = _SectionSizesFromElf(elf_path, tool_prefix)
    for k, v in elf_section_sizes.iteritems():
      if v != section_sizes.get(k):
        logging.error('ELF file and .map file do not agree on section sizes.')
        logging.error('.map file: %r', section_sizes)
        logging.error('readelf: %r', elf_section_sizes)
        sys.exit(1)

  if elf_path and output_directory:
    missed_object_paths = _DiscoverMissedObjectPaths(
        raw_symbols, elf_object_paths)
    bulk_analyzer.AnalyzePaths(missed_object_paths)
    bulk_analyzer.SortPaths()
    if track_string_literals:
      merge_string_syms = [
          s for s in raw_symbols if s.full_name == '** merge strings']
      # More likely for there to be a bug in supersize than an ELF to not have a
      # single string literal.
      assert merge_string_syms
      string_positions = [(s.address, s.size) for s in merge_string_syms]
      bulk_analyzer.AnalyzeStringLiterals(elf_path, string_positions)

  logging.info('Stripping linker prefixes from symbol names')
  _StripLinkerAddedSymbolPrefixes(raw_symbols)
  # Map file for some reason doesn't unmangle all names.
  # Unmangle prints its own log statement.
  _UnmangleRemainingSymbols(raw_symbols, tool_prefix)

  if elf_path:
    logging.info(
        'Adding symbols removed by identical code folding (as reported by nm)')
    # This normally does not block (it's finished by this time).
    names_by_address = elf_nm_result.get()
    _AddNmAliases(raw_symbols, names_by_address)

    if output_directory:
      object_paths_by_name = bulk_analyzer.GetSymbolNames()
      logging.debug('Fetched path information for %d symbols from %d files',
                    len(object_paths_by_name),
                    len(elf_object_paths) + len(missed_object_paths))

      # For aliases, this provides path information where there wasn't any.
      logging.info('Creating aliases for symbols shared by multiple paths')
      raw_symbols = _AssignNmAliasPathsAndCreatePathAliases(
          raw_symbols, object_paths_by_name)

      if track_string_literals:
        logging.info('Waiting for string literal extraction to complete.')
        list_of_positions_by_object_path = bulk_analyzer.GetStringPositions()
      bulk_analyzer.Close()

      if track_string_literals:
        logging.info('Deconstructing ** merge strings into literals')
        replacements = _CreateMergeStringsReplacements(merge_string_syms,
            list_of_positions_by_object_path)
        for merge_sym, literal_syms in itertools.izip(
            merge_string_syms, replacements):
          # Don't replace if no literals were found.
          if literal_syms:
            # Re-find the symbols since aliases cause their indices to change.
            idx = raw_symbols.index(merge_sym)
            # This assignment is a bit slow (causes array to be shifted), but
            # is fast enough since len(merge_string_syms) < 10.
            raw_symbols[idx:idx + 1] = literal_syms

  _ExtractSourcePathsAndNormalizeObjectPaths(raw_symbols, source_mapper)
  logging.info('Converting excessive aliases into shared-path symbols')
  _CompactLargeAliasesIntoSharedSymbols(raw_symbols)
  logging.debug('Connecting nm aliases')
  _ConnectNmAliases(raw_symbols)

  # Padding not really required, but it is useful to check for large padding and
  # log a warning.
  logging.info('Calculating padding')
  _CalculatePadding(raw_symbols)

  # Do not call _NormalizeNames() during archive since that method tends to need
  # tweaks over time. Calling it only when loading .size files allows for more
  # flexability.
  if normalize_names:
    _NormalizeNames(raw_symbols)

  logging.info('Processed %d symbols', len(raw_symbols))
  size_info = models.SizeInfo(section_sizes, raw_symbols)

  if logging.getLogger().isEnabledFor(logging.INFO):
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


def _ArchFromElf(elf_path, tool_prefix):
  args = [tool_prefix + 'readelf', '-h', elf_path]
  stdout = subprocess.check_output(args)
  machine = re.search('Machine:\s*(.+)', stdout).group(1)
  if machine == 'Intel 80386':
    return 'x86'
  if machine == 'Advanced Micro Devices X86-64':
    return 'x64'
  elif machine == 'ARM':
    return 'arm'
  elif machine == 'AArch64':
    return 'arm64'
  return machine


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


def _ElfInfoFromApk(apk_path, apk_so_path, tool_prefix):
  """Returns a tuple of (build_id, section_sizes)."""
  with zipfile.ZipFile(apk_path) as apk, \
       tempfile.NamedTemporaryFile() as f:
    f.write(apk.read(apk_so_path))
    f.flush()
    build_id = BuildIdFromElf(f.name, tool_prefix)
    section_sizes = _SectionSizesFromElf(f.name, tool_prefix)
    return build_id, section_sizes


def AddArguments(parser):
  parser.add_argument('size_file', help='Path to output .size file.')
  parser.add_argument('--apk-file',
                      help='.apk file to measure. When set, --elf-file will be '
                            'derived (if unset). Providing the .apk allows '
                            'for the size of packed relocations to be recorded')
  parser.add_argument('--elf-file',
                      help='Path to input ELF file. Currently used for '
                           'capturing metadata.')
  parser.add_argument('--map-file',
                      help='Path to input .map(.gz) file. Defaults to '
                           '{{elf_file}}.map(.gz)?. If given without '
                           '--elf-file, no size metadata will be recorded.')
  parser.add_argument('--no-source-paths', action='store_true',
                      help='Do not use .ninja files to map '
                           'object_path -> source_path')
  parser.add_argument('--tool-prefix',
                      help='Path prefix for c++filt, nm, readelf.')
  parser.add_argument('--output-directory',
                      help='Path to the root build directory.')
  parser.add_argument('--no-string-literals', dest='track_string_literals',
                      default=True, action='store_false',
                      help='Disable breaking down "** merge strings" into more '
                           'granular symbols.')


def Run(args, parser):
  if not args.size_file.endswith('.size'):
    parser.error('size_file must end with .size')

  elf_path = args.elf_file
  map_path = args.map_file
  apk_path = args.apk_file
  any_input = apk_path or elf_path or map_path
  if not any_input:
    parser.error('Most pass at least one of --apk-file, --elf-file, --map-file')
  lazy_paths = paths.LazyPaths(tool_prefix=args.tool_prefix,
                               output_directory=args.output_directory,
                               any_path_within_output_directory=any_input)
  if apk_path:
    with zipfile.ZipFile(apk_path) as z:
      lib_infos = [f for f in z.infolist()
                   if f.filename.endswith('.so') and f.file_size > 0]
    assert lib_infos, 'APK has no .so files.'
    # TODO(agrieve): Add support for multiple .so files, and take into account
    #     secondary architectures.
    apk_so_path = max(lib_infos, key=lambda x:x.file_size).filename
    logging.debug('Sub-apk path=%s', apk_so_path)
    if not elf_path and lazy_paths.output_directory:
      elf_path = os.path.join(
          lazy_paths.output_directory, 'lib.unstripped',
          os.path.basename(apk_so_path.replace('crazy.', '')))
      logging.debug('Detected --elf-file=%s', elf_path)

  if map_path:
    if not map_path.endswith('.map') and not map_path.endswith('.map.gz'):
      parser.error('Expected --map-file to end with .map or .map.gz')
  else:
    map_path = elf_path + '.map'
    if not os.path.exists(map_path):
      map_path += '.gz'
    if not os.path.exists(map_path):
      parser.error('Could not find .map(.gz)? file. Ensure you have built with '
                   'is_official_build=true, or use --map-file to point me a '
                   'linker map file.')

  tool_prefix = lazy_paths.VerifyToolPrefix()
  output_directory = None
  if not args.no_source_paths:
    output_directory = lazy_paths.VerifyOutputDirectory()

  metadata = CreateMetadata(map_path, elf_path, apk_path, tool_prefix,
                            output_directory)
  if apk_path and elf_path:
    # Extraction takes around 1 second, so do it in parallel.
    apk_elf_result = concurrent.ForkAndCall(
        _ElfInfoFromApk, (apk_path, apk_so_path, tool_prefix))

  size_info = CreateSizeInfo(map_path, elf_path, tool_prefix, output_directory,
                             normalize_names=False,
                             track_string_literals=args.track_string_literals)

  if metadata:
    size_info.metadata = metadata

    if apk_path:
      logging.debug('Extracting section sizes from .so within .apk')
      unstripped_section_sizes = size_info.section_sizes
      apk_build_id, size_info.section_sizes = apk_elf_result.get()
      assert apk_build_id == metadata[models.METADATA_ELF_BUILD_ID], (
          'BuildID for %s within %s did not match the one at %s' %
          (apk_so_path, apk_path, elf_path))

      packed_section_name = None
      architecture = metadata[models.METADATA_ELF_ARCHITECTURE]
      # Packing occurs enabled only arm32 & arm64.
      if architecture == 'arm':
        packed_section_name = '.rel.dyn'
      elif architecture == 'arm64':
        packed_section_name = '.rela.dyn'

      if packed_section_name:
        logging.debug('Recording size of unpacked relocations')
        if packed_section_name not in size_info.section_sizes:
          logging.warning('Packed section not present: %s', packed_section_name)
        else:
          size_info.section_sizes['%s (unpacked)' % packed_section_name] = (
              unstripped_section_sizes.get(packed_section_name))

  logging.info('Recording metadata: \n  %s',
               '\n  '.join(describe.DescribeMetadata(size_info.metadata)))
  logging.info('Saving result to %s', args.size_file)
  file_format.SaveSizeInfo(size_info, args.size_file)
  size_in_mb = os.path.getsize(args.size_file) / 1024.0 / 1024.0
  logging.info('Done. File size is %.2fMiB.', size_in_mb)
