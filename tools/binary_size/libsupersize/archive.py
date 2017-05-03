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
    name = symbol.name
    if name.startswith('startup.'):
      symbol.flags |= models.FLAG_STARTUP
      symbol.name = name[8:]
    elif name.startswith('unlikely.'):
      symbol.flags |= models.FLAG_UNLIKELY
      symbol.name = name[9:]
    elif name.startswith('rel.local.'):
      symbol.flags |= models.FLAG_REL_LOCAL
      symbol.name = name[10:]
    elif name.startswith('rel.'):
      symbol.flags |= models.FLAG_REL
      symbol.name = name[4:]


def _UnmangleRemainingSymbols(raw_symbols, tool_prefix):
  """Uses c++filt to unmangle any symbols that need it."""
  to_process = [s for s in raw_symbols if s.name.startswith('_Z')]
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
      symbol.flags |= models.FLAG_ANONYMOUS
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
  if path.startswith('gen/'):
    # Convert gen/third_party/... -> third_party/...
    return path[4:]
  if path.startswith('../../'):
    # Convert ../../third_party/... -> third_party/...
    return path[6:]
  return path


def _SourcePathForObjectPath(object_path, source_mapper):
  # We don't have source info for prebuilt .a files.
  if not os.path.isabs(object_path) and not object_path.startswith('..'):
    source_path = source_mapper.FindSourceForPath(object_path)
    if source_path:
      return _NormalizeSourcePath(source_path)
  return ''


def _ExtractSourcePaths(raw_symbols, source_mapper):
  """Fills in the |source_path| attribute."""
  logging.debug('Parsed %d .ninja files.', source_mapper.parsed_file_count)
  for symbol in raw_symbols:
    object_path = symbol.object_path
    if object_path and not symbol.source_path:
      symbol.source_path = _SourcePathForObjectPath(object_path, source_mapper)


def _ComputeAnscestorPath(path_list):
  """Returns the common anscestor of the given paths."""
  # Ignore missing paths.
  path_list = [p for p in path_list if p]
  prefix = os.path.commonprefix(path_list)
  # Put the path count as a subdirectory to allow for better grouping when
  # path-based breakdowns.
  if not prefix:
    if len(path_list) < 2:
      return ''
    return os.path.join('{shared}', str(len(path_list)))
  if prefix == path_list[0]:
    return prefix
  assert len(path_list) > 1, 'path_list: ' + repr(path_list)
  return os.path.join(os.path.dirname(prefix), '{shared}', str(len(path_list)))


# This must normalize object paths at the same time because normalization
# needs to occur before finding common ancestor.
def _ComputeAnscestorPathsAndNormalizeObjectPaths(
    raw_symbols, object_paths_by_name, source_mapper):
  num_found_paths = 0
  num_unknown_names = 0
  num_path_mismatches = 0
  num_unmatched_aliases = 0
  for symbol in raw_symbols:
    name = symbol.name
    if (symbol.IsBss() or
        not name or
        name[0] in '*.' or  # e.g. ** merge symbols, .Lswitch.table
        name == 'startup'):
      symbol.object_path = _NormalizeObjectPath(symbol.object_path)
      continue

    object_paths = object_paths_by_name.get(name)
    if object_paths:
      num_found_paths += 1
    else:
      if not symbol.object_path and symbol.aliases:
        # Happens when aliases are from object files where all symbols were
        # pruned or de-duped as aliases. Since we are only scanning .o files
        # referenced by included symbols, such files are missed.
        # TODO(agrieve): This could be fixed by retrieving linker inputs from
        #     build.ninja, or by looking for paths within the .map file's
        #     discarded sections.
        num_unmatched_aliases += 1
        continue
      if num_unknown_names < 10:
        logging.warning('Symbol not found in any .o files: %r', symbol)
      num_unknown_names += 1
      symbol.object_path = _NormalizeObjectPath(symbol.object_path)
      continue

    if symbol.object_path and symbol.object_path not in object_paths:
      if num_path_mismatches < 10:
        logging.warning('Symbol path reported by .map not found by nm.')
        logging.warning('sym=%r', symbol)
        logging.warning('paths=%r', object_paths)
      num_path_mismatches += 1

    if source_mapper:
      source_paths = [
          _SourcePathForObjectPath(p, source_mapper) for p in object_paths]
      symbol.source_path = _ComputeAnscestorPath(source_paths)

    object_paths = [_NormalizeObjectPath(p) for p in object_paths]
    symbol.object_path = _ComputeAnscestorPath(object_paths)

  logging.debug('Cross-referenced %d symbols with nm output. '
                'num_unknown_names=%d num_path_mismatches=%d '
                'num_unused_aliases=%d', num_found_paths, num_unknown_names,
                num_path_mismatches, num_unmatched_aliases)


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
    if not symbol.name.startswith('*') and (
        symbol.section in 'rd' and padding >= 256 or
        symbol.section in 't' and padding >= 64):
      # Should not happen.
      logging.warning('Large padding of %d between:\n  A) %r\n  B) %r' % (
                      padding, prev_symbol, symbol))
    symbol.padding = padding
    symbol.size += padding
    assert symbol.size >= 0, (
        'Symbol has negative size (likely not sorted propertly): '
        '%r\nprev symbol: %r' % (symbol, prev_symbol))


def _AddSymbolAliases(raw_symbols, aliases_by_address):
  # Step 1: Create list of (index_of_symbol, name_list).
  logging.debug('Creating alias list')
  replacements = []
  num_new_symbols = 0
  for i, s in enumerate(raw_symbols):
    # Don't alias padding-only symbols (e.g. ** symbol gap)
    if s.size_without_padding == 0:
      continue
    name_list = aliases_by_address.get(s.address)
    if name_list:
      if s.name not in name_list:
        logging.warning('Name missing from aliases: %s %s', s.name, name_list)
        continue
      replacements.append((i, name_list))
      num_new_symbols += len(name_list) - 1

  # Step 2: Create new symbols as siblings to each existing one.
  logging.debug('Creating %d aliases', num_new_symbols)
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

    # Create aliases (does not bother reusing the existing symbol).
    aliases = [None] * len(name_list)
    for i, name in enumerate(name_list):
      aliases[i] = models.Symbol(
          sym.section_name, sym.size, address=sym.address, name=name,
          aliases=aliases)

    dst_cursor_end -= len(aliases)
    raw_symbols[dst_cursor_end:dst_cursor_end + len(aliases)] = aliases

  assert dst_cursor_end == src_cursor_end


def LoadAndPostProcessSizeInfo(path):
  """Returns a SizeInfo for the given |path|."""
  logging.debug('Loading results from: %s', path)
  size_info = file_format.LoadSizeInfo(path)
  _PostProcessSizeInfo(size_info)
  return size_info


def _PostProcessSizeInfo(size_info):
  logging.info('Normalizing symbol names')
  _NormalizeNames(size_info.symbols)
  logging.info('Calculating padding')
  _CalculatePadding(size_info.symbols)
  logging.info('Processed %d symbols', len(size_info.symbols))


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

    metadata = {
        models.METADATA_GIT_REVISION: git_rev,
        models.METADATA_ELF_ARCHITECTURE: architecture,
        models.METADATA_ELF_MTIME: timestamp,
        models.METADATA_ELF_BUILD_ID: build_id,
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
                   raw_only=False):
  """Creates a SizeInfo.

  Args:
    map_path: Path to the linker .map(.gz) file to parse.
    elf_path: Path to the corresponding unstripped ELF file. Used to find symbol
        aliases and inlined functions. Can be None.
    tool_prefix: Prefix for c++filt & nm (required).
    output_directory: Build output directory. If None, source_paths and symbol
        alias information will not be recorded.
    raw_only: Fill in just the information required for creating a .size file.
  """
  source_mapper = None
  if output_directory:
    # Start by finding the elf_object_paths, so that nm can run on them while
    # the linker .map is being parsed.
    logging.info('Parsing ninja files.')
    source_mapper, elf_object_paths = ninja_parser.Parse(
        output_directory, elf_path)
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
    bulk_analyzer.Close()

  if source_mapper:
    logging.info('Looking up source paths from ninja files')
    _ExtractSourcePaths(raw_symbols, source_mapper)
    assert source_mapper.unmatched_paths_count == 0, (
        'One or more source file paths could not be found. Likely caused by '
        '.ninja files being generated at a different time than the .map file.')

  logging.info('Stripping linker prefixes from symbol names')
  _StripLinkerAddedSymbolPrefixes(raw_symbols)
  # Map file for some reason doesn't unmangle all names.
  # Unmangle prints its own log statement.
  _UnmangleRemainingSymbols(raw_symbols, tool_prefix)

  if elf_path:
    logging.info('Adding aliased symbols, as reported by nm')
    # This normally does not block (it's finished by this time).
    aliases_by_address = elf_nm_result.get()
    _AddSymbolAliases(raw_symbols, aliases_by_address)

    if output_directory:
      # For aliases, this provides path information where there wasn't any.
      logging.info('Computing ancestor paths for inline functions and '
                   'normalizing object paths')

      object_paths_by_name = bulk_analyzer.Get()
      logging.debug('Fetched path information for %d symbols from %d files',
                    len(object_paths_by_name),
                    len(elf_object_paths) + len(missed_object_paths))
      _ComputeAnscestorPathsAndNormalizeObjectPaths(
          raw_symbols, object_paths_by_name, source_mapper)

  if not elf_path or not output_directory:
    logging.info('Normalizing object paths.')
    for symbol in raw_symbols:
      symbol.object_path = _NormalizeObjectPath(symbol.object_path)

  size_info = models.SizeInfo(section_sizes, models.SymbolGroup(raw_symbols))

  # Name normalization not strictly required, but makes for smaller files.
  if raw_only:
    logging.info('Normalizing symbol names')
    _NormalizeNames(size_info.symbols)
  else:
    _PostProcessSizeInfo(size_info)

  if logging.getLogger().isEnabledFor(logging.DEBUG):
    # Padding is reported in size coverage logs.
    if raw_only:
      _CalculatePadding(size_info.symbols)
    for line in describe.DescribeSizeInfoCoverage(size_info):
      logging.info(line)
  logging.info('Recorded info for %d symbols', len(size_info.symbols))
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
  return re.search('Machine:\s*(\S+)', stdout).group(1)


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
  parser.add_argument('--tool-prefix', default='',
                      help='Path prefix for c++filt.')
  parser.add_argument('--output-directory',
                      help='Path to the root build directory.')


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
      parser.error('Could not find .map(.gz)? file. Use --map-file.')

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

  size_info = CreateSizeInfo(
      map_path, elf_path, tool_prefix, output_directory, raw_only=True)

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
      if architecture == 'ARM':
        packed_section_name = '.rel.dyn'
      elif architecture == 'AArch64':
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
  logging.info('Done')
