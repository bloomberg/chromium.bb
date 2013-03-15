# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""The deep heap profiler script for Chrome."""

from datetime import datetime
import json
import logging
import optparse
import os
import re
import subprocess
import sys
import tempfile
import zipfile

BASE_PATH = os.path.dirname(os.path.abspath(__file__))
FIND_RUNTIME_SYMBOLS_PATH = os.path.join(
    BASE_PATH, os.pardir, 'find_runtime_symbols')
sys.path.append(FIND_RUNTIME_SYMBOLS_PATH)

from find_runtime_symbols import find_runtime_symbols_list
from find_runtime_symbols import find_runtime_typeinfo_symbols_list
from find_runtime_symbols import RuntimeSymbolsInProcess
from prepare_symbol_info import prepare_symbol_info

BUCKET_ID = 5
VIRTUAL = 0
COMMITTED = 1
ALLOC_COUNT = 2
FREE_COUNT = 3
NULL_REGEX = re.compile('')

LOGGER = logging.getLogger('dmprof')
POLICIES_JSON_PATH = os.path.join(BASE_PATH, 'policies.json')
FUNCTION_ADDRESS = 'function'
TYPEINFO_ADDRESS = 'typeinfo'


# Heap Profile Dump versions

# DUMP_DEEP_[1-4] are obsolete.
# DUMP_DEEP_2+ distinct mmap regions and malloc chunks.
# DUMP_DEEP_3+ don't include allocation functions in their stack dumps.
# DUMP_DEEP_4+ support comments with '#' and global stats "nonprofiled-*".
# DUMP_DEEP_[1-2] should be processed by POLICY_DEEP_1.
# DUMP_DEEP_[3-4] should be processed by POLICY_DEEP_2 or POLICY_DEEP_3.
DUMP_DEEP_1 = 'DUMP_DEEP_1'
DUMP_DEEP_2 = 'DUMP_DEEP_2'
DUMP_DEEP_3 = 'DUMP_DEEP_3'
DUMP_DEEP_4 = 'DUMP_DEEP_4'

DUMP_DEEP_OBSOLETE = (DUMP_DEEP_1, DUMP_DEEP_2, DUMP_DEEP_3, DUMP_DEEP_4)

# DUMP_DEEP_5 doesn't separate sections for malloc and mmap.
# malloc and mmap are identified in bucket files.
# DUMP_DEEP_5 should be processed by POLICY_DEEP_4.
DUMP_DEEP_5 = 'DUMP_DEEP_5'

# DUMP_DEEP_6 adds a mmap list to DUMP_DEEP_5.
DUMP_DEEP_6 = 'DUMP_DEEP_6'

# Heap Profile Policy versions

# POLICY_DEEP_1 DOES NOT include allocation_type columns.
# mmap regions are distincted w/ mmap frames in the pattern column.
POLICY_DEEP_1 = 'POLICY_DEEP_1'

# POLICY_DEEP_2 DOES include allocation_type columns.
# mmap regions are distincted w/ the allocation_type column.
POLICY_DEEP_2 = 'POLICY_DEEP_2'

# POLICY_DEEP_3 is in JSON format.
POLICY_DEEP_3 = 'POLICY_DEEP_3'

# POLICY_DEEP_3 contains typeinfo.
POLICY_DEEP_4 = 'POLICY_DEEP_4'


class EmptyDumpException(Exception):
  def __init__(self, value=''):
    super(EmptyDumpException, self).__init__()
    self.value = value
  def __str__(self):
    return repr(self.value)


class ParsingException(Exception):
  def __init__(self, value=''):
    super(ParsingException, self).__init__()
    self.value = value
  def __str__(self):
    return repr(self.value)


class InvalidDumpException(ParsingException):
  def __init__(self, value):
    super(InvalidDumpException, self).__init__()
    self.value = value
  def __str__(self):
    return "invalid heap profile dump: %s" % repr(self.value)


class ObsoleteDumpVersionException(ParsingException):
  def __init__(self, value):
    super(ObsoleteDumpVersionException, self).__init__()
    self.value = value
  def __str__(self):
    return "obsolete heap profile dump version: %s" % repr(self.value)


def skip_while(index, max_index, skipping_condition):
  """Increments |index| until |skipping_condition|(|index|) is False.

  Returns:
      A pair of an integer indicating a line number after skipped, and a
      boolean value which is True if found a line which skipping_condition
      is False for.
  """
  while skipping_condition(index):
    index += 1
    if index >= max_index:
      return index, False
  return index, True


class SymbolDataSources(object):
  """Manages symbol data sources in a process.

  The symbol data sources consist of maps (/proc/<pid>/maps), nm, readelf and
  so on.  They are collected into a directory '|prefix|.symmap' from the binary
  files by 'prepare()' with tools/find_runtime_symbols/prepare_symbol_info.py.

  Binaries are not mandatory to profile.  The prepared data sources work in
  place of the binary even if the binary has been overwritten with another
  binary.

  Note that loading the symbol data sources takes a long time.  They are often
  very big.  So, the 'dmprof' profiler is designed to use 'SymbolMappingCache'
  which caches actually used symbols.
  """
  def __init__(self, prefix):
    self._prefix = prefix
    self._prepared_symbol_data_sources_path = None
    self._loaded_symbol_data_sources = None

  def prepare(self):
    """Prepares symbol data sources by extracting mapping from a binary.

    The prepared symbol data sources are stored in a directory.  The directory
    name is stored in |self._prepared_symbol_data_sources_path|.

    Returns:
        True if succeeded.
    """
    LOGGER.info('Preparing symbol mapping...')
    self._prepared_symbol_data_sources_path, used_tempdir = prepare_symbol_info(
        self._prefix + '.maps', self._prefix + '.symmap', True)
    if self._prepared_symbol_data_sources_path:
      LOGGER.info('  Prepared symbol mapping.')
      if used_tempdir:
        LOGGER.warn('  Using a temporary directory for symbol mapping.')
        LOGGER.warn('  Delete it by yourself.')
        LOGGER.warn('  Or, move the directory by yourself to use it later.')
      return True
    else:
      LOGGER.warn('  Failed to prepare symbol mapping.')
      return False

  def get(self):
    """Returns the prepared symbol data sources.

    Returns:
        The prepared symbol data sources.  None if failed.
    """
    if not self._prepared_symbol_data_sources_path and not self.prepare():
      return None
    if not self._loaded_symbol_data_sources:
      LOGGER.info('Loading symbol mapping...')
      self._loaded_symbol_data_sources = RuntimeSymbolsInProcess.load(
          self._prepared_symbol_data_sources_path)
    return self._loaded_symbol_data_sources

  def path(self):
    """Returns the path of the prepared symbol data sources if possible."""
    if not self._prepared_symbol_data_sources_path and not self.prepare():
      return None
    return self._prepared_symbol_data_sources_path


class SymbolFinder(object):
  """Finds corresponding symbols from addresses.

  This class does only 'find()' symbols from a specified |address_list|.
  It is introduced to make a finder mockable.
  """
  _FIND_RUNTIME_SYMBOLS_FUNCTIONS = {
      FUNCTION_ADDRESS: find_runtime_symbols_list,
      TYPEINFO_ADDRESS: find_runtime_typeinfo_symbols_list,
      }

  def __init__(self, address_type, symbol_data_sources):
    self._finder_function = self._FIND_RUNTIME_SYMBOLS_FUNCTIONS[address_type]
    self._symbol_data_sources = symbol_data_sources

  def find(self, address_list):
    return self._finder_function(self._symbol_data_sources.get(), address_list)


class SymbolMappingCache(object):
  """Caches mapping from actually used addresses to symbols.

  'update()' updates the cache from the original symbol data sources via
  'SymbolFinder'.  Symbols can be looked up by the method 'lookup()'.
  """
  def __init__(self):
    self._symbol_mapping_caches = {
        FUNCTION_ADDRESS: {},
        TYPEINFO_ADDRESS: {},
        }

  def update(self, address_type, bucket_set, symbol_finder, cache_f):
    """Updates symbol mapping cache on memory and in a symbol cache file.

    It reads cached symbol mapping from a symbol cache file |cache_f| if it
    exists.  Unresolved addresses are then resolved and added to the cache
    both on memory and in the symbol cache file with using 'SymbolFinder'.

    A cache file is formatted as follows:
      <Address> <Symbol>
      <Address> <Symbol>
      <Address> <Symbol>
      ...

    Args:
        address_type: A type of addresses to update.
            It should be one of FUNCTION_ADDRESS or TYPEINFO_ADDRESS.
        bucket_set: A BucketSet object.
        symbol_finder: A SymbolFinder object to find symbols.
        cache_f: A readable and writable IO object of the symbol cache file.
    """
    cache_f.seek(0, os.SEEK_SET)
    self._load(cache_f, address_type)

    unresolved_addresses = sorted(
        address for address in bucket_set.iter_addresses(address_type)
        if address not in self._symbol_mapping_caches[address_type])

    if not unresolved_addresses:
      LOGGER.info('No need to resolve any more addresses.')
      return

    cache_f.seek(0, os.SEEK_END)
    LOGGER.info('Loading %d unresolved addresses.' %
                   len(unresolved_addresses))
    symbol_list = symbol_finder.find(unresolved_addresses)

    for address, symbol in zip(unresolved_addresses, symbol_list):
      stripped_symbol = symbol.strip() or '??'
      self._symbol_mapping_caches[address_type][address] = stripped_symbol
      cache_f.write('%x %s\n' % (address, stripped_symbol))

  def lookup(self, address_type, address):
    """Looks up a symbol for a given |address|.

    Args:
        address_type: A type of addresses to lookup.
            It should be one of FUNCTION_ADDRESS or TYPEINFO_ADDRESS.
        address: An integer that represents an address.

    Returns:
        A string that represents a symbol.
    """
    return self._symbol_mapping_caches[address_type].get(address)

  def _load(self, cache_f, address_type):
    try:
      for line in cache_f:
        items = line.rstrip().split(None, 1)
        if len(items) == 1:
          items.append('??')
        self._symbol_mapping_caches[address_type][int(items[0], 16)] = items[1]
      LOGGER.info('Loaded %d entries from symbol cache.' %
                     len(self._symbol_mapping_caches[address_type]))
    except IOError as e:
      LOGGER.info('The symbol cache file is invalid: %s' % e)


class Rule(object):
  """Represents one matching rule in a policy file."""

  def __init__(self, name, mmap, stacktrace_pattern, typeinfo_pattern=None):
    self._name = name
    self._mmap = mmap
    self._stacktrace_pattern = re.compile(stacktrace_pattern + r'\Z')
    if typeinfo_pattern:
      self._typeinfo_pattern = re.compile(typeinfo_pattern + r'\Z')
    else:
      self._typeinfo_pattern = None

  @property
  def name(self):
    return self._name

  @property
  def mmap(self):
    return self._mmap

  @property
  def stacktrace_pattern(self):
    return self._stacktrace_pattern

  @property
  def typeinfo_pattern(self):
    return self._typeinfo_pattern


class Policy(object):
  """Represents a policy, a content of a policy file."""

  def __init__(self, rules, version, components):
    self._rules = rules
    self._version = version
    self._components = components

  @property
  def rules(self):
    return self._rules

  @property
  def version(self):
    return self._version

  @property
  def components(self):
    return self._components

  def find(self, bucket):
    """Finds a matching component name which a given |bucket| belongs to.

    Args:
        bucket: A Bucket object to be searched for.

    Returns:
        A string representing a component name.
    """
    if not bucket:
      return 'no-bucket'
    if bucket.component_cache:
      return bucket.component_cache

    stacktrace = bucket.symbolized_joined_stacktrace
    typeinfo = bucket.symbolized_typeinfo
    if typeinfo.startswith('0x'):
      typeinfo = bucket.typeinfo_name

    for rule in self._rules:
      if (bucket.mmap == rule.mmap and
          rule.stacktrace_pattern.match(stacktrace) and
          (not rule.typeinfo_pattern or rule.typeinfo_pattern.match(typeinfo))):
        bucket.component_cache = rule.name
        return rule.name

    assert False

  @staticmethod
  def load(filename, filetype):
    """Loads a policy file of |filename| in a |format|.

    Args:
        filename: A filename to be loaded.
        filetype: A string to specify a type of the file.  Only 'json' is
            supported for now.

    Returns:
        A loaded Policy object.
    """
    with open(os.path.join(BASE_PATH, filename)) as policy_f:
      return Policy.parse(policy_f, filetype)

  @staticmethod
  def parse(policy_f, filetype):
    """Parses a policy file content in a |format|.

    Args:
        policy_f: An IO object to be loaded.
        filetype: A string to specify a type of the file.  Only 'json' is
            supported for now.

    Returns:
        A loaded Policy object.
    """
    if filetype == 'json':
      return Policy._parse_json(policy_f)
    else:
      return None

  @staticmethod
  def _parse_json(policy_f):
    """Parses policy file in json format.

    A policy file contains component's names and their stacktrace pattern
    written in regular expression.  Those patterns are matched against each
    symbols of each stacktraces in the order written in the policy file

    Args:
         policy_f: A File/IO object to read.

    Returns:
         A loaded policy object.
    """
    policy = json.load(policy_f)

    rules = []
    for rule in policy['rules']:
      rules.append(Rule(
          rule['name'],
          rule['allocator'] == 'mmap',
          rule['stacktrace'],
          rule['typeinfo'] if 'typeinfo' in rule else None))
    return Policy(rules, policy['version'], policy['components'])


class PolicySet(object):
  """Represents a set of policies."""

  def __init__(self, policy_directory):
    self._policy_directory = policy_directory

  @staticmethod
  def load(labels=None):
    """Loads a set of policies via the "default policy directory".

    The "default policy directory" contains pairs of policies and their labels.
    For example, a policy "policy.l0.json" is labeled "l0" in the default
    policy directory "policies.json".

    All policies in the directory are loaded by default.  Policies can be
    limited by |labels|.

    Args:
        labels: An array that contains policy labels to be loaded.

    Returns:
        A PolicySet object.
    """
    default_policy_directory = PolicySet._load_default_policy_directory()
    if labels:
      specified_policy_directory = {}
      for label in labels:
        if label in default_policy_directory:
          specified_policy_directory[label] = default_policy_directory[label]
        # TODO(dmikurube): Load an un-labeled policy file.
      return PolicySet._load_policies(specified_policy_directory)
    else:
      return PolicySet._load_policies(default_policy_directory)

  def __len__(self):
    return len(self._policy_directory)

  def __iter__(self):
    for label in self._policy_directory:
      yield label

  def __getitem__(self, label):
    return self._policy_directory[label]

  @staticmethod
  def _load_default_policy_directory():
    with open(POLICIES_JSON_PATH, mode='r') as policies_f:
      default_policy_directory = json.load(policies_f)
    return default_policy_directory

  @staticmethod
  def _load_policies(directory):
    LOGGER.info('Loading policy files.')
    policies = {}
    for label in directory:
      LOGGER.info('  %s: %s' % (label, directory[label]['file']))
      loaded = Policy.load(directory[label]['file'], directory[label]['format'])
      if loaded:
        policies[label] = loaded
    return PolicySet(policies)


class Bucket(object):
  """Represents a bucket, which is a unit of memory block classification."""

  def __init__(self, stacktrace, mmap, typeinfo, typeinfo_name):
    self._stacktrace = stacktrace
    self._mmap = mmap
    self._typeinfo = typeinfo
    self._typeinfo_name = typeinfo_name

    self._symbolized_stacktrace = stacktrace
    self._symbolized_joined_stacktrace = ''
    self._symbolized_typeinfo = typeinfo_name

    self.component_cache = ''

  def symbolize(self, symbol_mapping_cache):
    """Makes a symbolized stacktrace and typeinfo with |symbol_mapping_cache|.

    Args:
        symbol_mapping_cache: A SymbolMappingCache object.
    """
    # TODO(dmikurube): Fill explicitly with numbers if symbol not found.
    self._symbolized_stacktrace = [
        symbol_mapping_cache.lookup(FUNCTION_ADDRESS, address)
        for address in self._stacktrace]
    self._symbolized_joined_stacktrace = ' '.join(self._symbolized_stacktrace)
    if not self._typeinfo:
      self._symbolized_typeinfo = 'no typeinfo'
    else:
      self._symbolized_typeinfo = symbol_mapping_cache.lookup(
          TYPEINFO_ADDRESS, self._typeinfo)
      if not self._symbolized_typeinfo:
        self._symbolized_typeinfo = 'no typeinfo'

  def clear_component_cache(self):
    self.component_cache = ''

  @property
  def stacktrace(self):
    return self._stacktrace

  @property
  def mmap(self):
    return self._mmap

  @property
  def typeinfo(self):
    return self._typeinfo

  @property
  def typeinfo_name(self):
    return self._typeinfo_name

  @property
  def symbolized_stacktrace(self):
    return self._symbolized_stacktrace

  @property
  def symbolized_joined_stacktrace(self):
    return self._symbolized_joined_stacktrace

  @property
  def symbolized_typeinfo(self):
    return self._symbolized_typeinfo


class BucketSet(object):
  """Represents a set of bucket."""
  def __init__(self):
    self._buckets = {}
    self._addresses = {
        FUNCTION_ADDRESS: set(),
        TYPEINFO_ADDRESS: set(),
        }

  def load(self, prefix):
    """Loads all related bucket files.

    Args:
        prefix: A prefix string for bucket file names.
    """
    LOGGER.info('Loading bucket files.')

    n = 0
    while True:
      path = '%s.%04d.buckets' % (prefix, n)
      if not os.path.exists(path):
        if n > 10:
          break
        n += 1
        continue
      LOGGER.info('  %s' % path)
      with open(path, 'r') as f:
        self._load_file(f)
      n += 1

  def _load_file(self, bucket_f):
    for line in bucket_f:
      words = line.split()
      typeinfo = None
      typeinfo_name = ''
      stacktrace_begin = 2
      for index, word in enumerate(words):
        if index < 2:
          continue
        if word[0] == 't':
          typeinfo = int(word[1:], 16)
          self._addresses[TYPEINFO_ADDRESS].add(typeinfo)
        elif word[0] == 'n':
          typeinfo_name = word[1:]
        else:
          stacktrace_begin = index
          break
      stacktrace = [int(address, 16) for address in words[stacktrace_begin:]]
      for frame in stacktrace:
        self._addresses[FUNCTION_ADDRESS].add(frame)
      self._buckets[int(words[0])] = Bucket(
          stacktrace, words[1] == 'mmap', typeinfo, typeinfo_name)

  def __iter__(self):
    for bucket_id, bucket_content in self._buckets.iteritems():
      yield bucket_id, bucket_content

  def __getitem__(self, bucket_id):
    return self._buckets[bucket_id]

  def get(self, bucket_id):
    return self._buckets.get(bucket_id)

  def symbolize(self, symbol_mapping_cache):
    for bucket_content in self._buckets.itervalues():
      bucket_content.symbolize(symbol_mapping_cache)

  def clear_component_cache(self):
    for bucket_content in self._buckets.itervalues():
      bucket_content.clear_component_cache()

  def iter_addresses(self, address_type):
    for function in self._addresses[address_type]:
      yield function


class Dump(object):
  """Represents a heap profile dump."""

  def __init__(self, path, time):
    self._path = path
    self._time = time
    self._stacktrace_lines = []
    self._global_stats = {} # used only in apply_policy

    self._version = ''
    self._lines = []

  @property
  def path(self):
    return self._path

  @property
  def time(self):
    return self._time

  @property
  def iter_stacktrace(self):
    for line in self._stacktrace_lines:
      yield line

  def global_stat(self, name):
    return self._global_stats[name]

  @staticmethod
  def load(path, log_header='Loading a heap profile dump: '):
    """Loads a heap profile dump.

    Args:
        path: A file path string to load.
        log_header: A preceding string for log messages.

    Returns:
        A loaded Dump object.

    Raises:
        ParsingException for invalid heap profile dumps.
    """
    dump = Dump(path, os.stat(path).st_mtime)
    with open(path, 'r') as f:
      dump.load_file(f, log_header)
    return dump

  def load_file(self, f, log_header):
    self._lines = [line for line in f
                   if line and not line.startswith('#')]

    try:
      self._version, ln = self._parse_version()
      self._parse_global_stats()
      self._extract_stacktrace_lines(ln)
    except EmptyDumpException:
      LOGGER.info('%s%s ...ignored an empty dump.' % (log_header, self._path))
    except ParsingException, e:
      LOGGER.error('%s%s ...error %s' % (log_header, self._path, e))
      raise
    else:
      LOGGER.info('%s%s (version:%s)' % (log_header, self._path, self._version))

  def _parse_version(self):
    """Parses a version string in self._lines.

    Returns:
        A pair of (a string representing a version of the stacktrace dump,
        and an integer indicating a line number next to the version string).

    Raises:
        ParsingException for invalid dump versions.
    """
    version = ''

    # Skip until an identifiable line.
    headers = ('STACKTRACES:\n', 'MMAP_STACKTRACES:\n', 'heap profile: ')
    if not self._lines:
      raise EmptyDumpException('Empty heap dump file.')
    (ln, found) = skip_while(
        0, len(self._lines),
        lambda n: not self._lines[n].startswith(headers))
    if not found:
      raise InvalidDumpException('No version header.')

    # Identify a version.
    if self._lines[ln].startswith('heap profile: '):
      version = self._lines[ln][13:].strip()
      if version in (DUMP_DEEP_5, DUMP_DEEP_6):
        (ln, _) = skip_while(
            ln, len(self._lines),
            lambda n: self._lines[n] != 'STACKTRACES:\n')
      elif version in DUMP_DEEP_OBSOLETE:
        raise ObsoleteDumpVersionException(version)
      else:
        raise InvalidDumpException('Invalid version: %s' % version)
    elif self._lines[ln] == 'STACKTRACES:\n':
      raise ObsoleteDumpVersionException(DUMP_DEEP_1)
    elif self._lines[ln] == 'MMAP_STACKTRACES:\n':
      raise ObsoleteDumpVersionException(DUMP_DEEP_2)

    return (version, ln)

  def _parse_global_stats(self):
    """Parses lines in self._lines as global stats."""
    (ln, _) = skip_while(
        0, len(self._lines),
        lambda n: self._lines[n] != 'GLOBAL_STATS:\n')

    global_stat_names = [
        'total', 'absent', 'file-exec', 'file-nonexec', 'anonymous', 'stack',
        'other', 'nonprofiled-absent', 'nonprofiled-anonymous',
        'nonprofiled-file-exec', 'nonprofiled-file-nonexec',
        'nonprofiled-stack', 'nonprofiled-other',
        'profiled-mmap', 'profiled-malloc']

    for prefix in global_stat_names:
      (ln, _) = skip_while(
          ln, len(self._lines),
          lambda n: self._lines[n].split()[0] != prefix)
      words = self._lines[ln].split()
      self._global_stats[prefix + '_virtual'] = int(words[-2])
      self._global_stats[prefix + '_committed'] = int(words[-1])

  def _extract_stacktrace_lines(self, line_number):
    """Extracts the position of stacktrace lines.

    Valid stacktrace lines are stored into self._stacktrace_lines.

    Args:
        line_number: A line number to start parsing in lines.

    Raises:
        ParsingException for invalid dump versions.
    """
    if self._version in (DUMP_DEEP_5, DUMP_DEEP_6):
      (line_number, _) = skip_while(
          line_number, len(self._lines),
          lambda n: not self._lines[n].split()[0].isdigit())
      stacktrace_start = line_number
      (line_number, _) = skip_while(
          line_number, len(self._lines),
          lambda n: self._check_stacktrace_line(self._lines[n]))
      self._stacktrace_lines = self._lines[stacktrace_start:line_number]

    elif self._version in DUMP_DEEP_OBSOLETE:
      raise ObsoleteDumpVersionException(self._version)

    else:
      raise InvalidDumpException('Invalid version: %s' % self._version)

  @staticmethod
  def _check_stacktrace_line(stacktrace_line):
    """Checks if a given stacktrace_line is valid as stacktrace.

    Args:
        stacktrace_line: A string to be checked.

    Returns:
        True if the given stacktrace_line is valid.
    """
    words = stacktrace_line.split()
    if len(words) < BUCKET_ID + 1:
      return False
    if words[BUCKET_ID - 1] != '@':
      return False
    return True


class DumpList(object):
  """Represents a sequence of heap profile dumps."""

  def __init__(self, dump_list):
    self._dump_list = dump_list

  @staticmethod
  def load(path_list):
    LOGGER.info('Loading heap dump profiles.')
    dump_list = []
    for path in path_list:
      dump_list.append(Dump.load(path, '  '))
    return DumpList(dump_list)

  def __len__(self):
    return len(self._dump_list)

  def __iter__(self):
    for dump in self._dump_list:
      yield dump

  def __getitem__(self, index):
    return self._dump_list[index]


class Command(object):
  """Subclasses are a subcommand for this executable.

  See COMMANDS in main().
  """
  def __init__(self, usage):
    self._parser = optparse.OptionParser(usage)

  @staticmethod
  def load_basic_files(dump_path, multiple):
    prefix = Command._find_prefix(dump_path)
    symbol_data_sources = SymbolDataSources(prefix)
    symbol_data_sources.prepare()
    bucket_set = BucketSet()
    bucket_set.load(prefix)
    if multiple:
      dump_list = DumpList.load(Command._find_all_dumps(dump_path))
    else:
      dump = Dump.load(dump_path)
    symbol_mapping_cache = SymbolMappingCache()
    with open(prefix + '.funcsym', 'a+') as cache_f:
      symbol_mapping_cache.update(
          FUNCTION_ADDRESS, bucket_set,
          SymbolFinder(FUNCTION_ADDRESS, symbol_data_sources), cache_f)
    with open(prefix + '.typesym', 'a+') as cache_f:
      symbol_mapping_cache.update(
          TYPEINFO_ADDRESS, bucket_set,
          SymbolFinder(TYPEINFO_ADDRESS, symbol_data_sources), cache_f)
    bucket_set.symbolize(symbol_mapping_cache)
    if multiple:
      return (bucket_set, dump_list)
    else:
      return (bucket_set, dump)

  @staticmethod
  def _find_prefix(path):
    return re.sub('\.[0-9][0-9][0-9][0-9]\.heap', '', path)

  @staticmethod
  def _find_all_dumps(dump_path):
    prefix = Command._find_prefix(dump_path)
    dump_path_list = [dump_path]

    n = int(dump_path[len(dump_path) - 9 : len(dump_path) - 5])
    n += 1
    while True:
      p = '%s.%04d.heap' % (prefix, n)
      if os.path.exists(p):
        dump_path_list.append(p)
      else:
        break
      n += 1

    return dump_path_list

  @staticmethod
  def _find_all_buckets(dump_path):
    prefix = Command._find_prefix(dump_path)
    bucket_path_list = []

    n = 0
    while True:
      path = '%s.%04d.buckets' % (prefix, n)
      if not os.path.exists(path):
        if n > 10:
          break
        n += 1
        continue
      bucket_path_list.append(path)
      n += 1

    return bucket_path_list

  def _parse_args(self, sys_argv, required):
    options, args = self._parser.parse_args(sys_argv)
    if len(args) != required + 1:
      self._parser.error('needs %d argument(s).\n' % required)
      return None
    return (options, args)

  @staticmethod
  def _parse_policy_list(options_policy):
    if options_policy:
      return options_policy.split(',')
    else:
      return None


class StacktraceCommand(Command):
  def __init__(self):
    super(StacktraceCommand, self).__init__(
        'Usage: %prog stacktrace <dump>')

  def do(self, sys_argv):
    _, args = self._parse_args(sys_argv, 1)
    dump_path = args[1]
    (bucket_set, dump) = Command.load_basic_files(dump_path, False)

    StacktraceCommand._output(dump, bucket_set, sys.stdout)
    return 0

  @staticmethod
  def _output(dump, bucket_set, out):
    """Outputs a given stacktrace.

    Args:
        bucket_set: A BucketSet object.
        out: A file object to output.
    """
    for line in dump.iter_stacktrace:
      words = line.split()
      bucket = bucket_set.get(int(words[BUCKET_ID]))
      if not bucket:
        continue
      for i in range(0, BUCKET_ID - 1):
        out.write(words[i] + ' ')
      for frame in bucket.symbolized_stacktrace:
        out.write(frame + ' ')
      out.write('\n')


class PolicyCommands(Command):
  def __init__(self, command):
    super(PolicyCommands, self).__init__(
        'Usage: %%prog %s [-p POLICY] <first-dump>' % command)
    self._parser.add_option('-p', '--policy', type='string', dest='policy',
                            help='profile with POLICY', metavar='POLICY')

  def _set_up(self, sys_argv):
    options, args = self._parse_args(sys_argv, 1)
    dump_path = args[1]
    (bucket_set, dumps) = Command.load_basic_files(dump_path, True)

    policy_set = PolicySet.load(Command._parse_policy_list(options.policy))
    return policy_set, dumps, bucket_set

  @staticmethod
  def _apply_policy(dump, policy, bucket_set, first_dump_time):
    """Aggregates the total memory size of each component.

    Iterate through all stacktraces and attribute them to one of the components
    based on the policy.  It is important to apply policy in right order.

    Args:
        dump: A Dump object.
        policy: A Policy object.
        bucket_set: A BucketSet object.
        first_dump_time: An integer representing time when the first dump is
            dumped.

    Returns:
        A dict mapping components and their corresponding sizes.
    """
    LOGGER.info('  %s' % dump.path)
    sizes = dict((c, 0) for c in policy.components)

    PolicyCommands._accumulate(dump, policy, bucket_set, sizes)

    sizes['mmap-no-log'] = (
        dump.global_stat('profiled-mmap_committed') -
        sizes['mmap-total-log'])
    sizes['mmap-total-record'] = dump.global_stat('profiled-mmap_committed')
    sizes['mmap-total-record-vm'] = dump.global_stat('profiled-mmap_virtual')

    sizes['tc-no-log'] = (
        dump.global_stat('profiled-malloc_committed') -
        sizes['tc-total-log'])
    sizes['tc-total-record'] = dump.global_stat('profiled-malloc_committed')
    sizes['tc-unused'] = (
        sizes['mmap-tcmalloc'] -
        dump.global_stat('profiled-malloc_committed'))
    sizes['tc-total'] = sizes['mmap-tcmalloc']

    for key, value in {
        'total': 'total_committed',
        'filemapped': 'file_committed',
        'absent': 'absent_committed',
        'file-exec': 'file-exec_committed',
        'file-nonexec': 'file-nonexec_committed',
        'anonymous': 'anonymous_committed',
        'stack': 'stack_committed',
        'other': 'other_committed',
        'unhooked-absent': 'nonprofiled-absent_committed',
        'unhooked-anonymous': 'nonprofiled-anonymous_committed',
        'unhooked-file-exec': 'nonprofiled-file-exec_committed',
        'unhooked-file-nonexec': 'nonprofiled-file-nonexec_committed',
        'unhooked-stack': 'nonprofiled-stack_committed',
        'unhooked-other': 'nonprofiled-other_committed',
        'total-vm': 'total_virtual',
        'filemapped-vm': 'file_virtual',
        'anonymous-vm': 'anonymous_virtual',
        'other-vm': 'other_virtual' }.iteritems():
      if key in sizes:
        sizes[key] = dump.global_stat(value)

    if 'mustbezero' in sizes:
      removed_list = (
          'profiled-mmap_committed',
          'nonprofiled-absent_committed',
          'nonprofiled-anonymous_committed',
          'nonprofiled-file-exec_committed',
          'nonprofiled-file-nonexec_committed',
          'nonprofiled-stack_committed',
          'nonprofiled-other_committed')
      sizes['mustbezero'] = (
          dump.global_stat('total_committed') -
          sum(dump.global_stat(removed) for removed in removed_list))
    if 'total-exclude-profiler' in sizes:
      sizes['total-exclude-profiler'] = (
          dump.global_stat('total_committed') -
          (sizes['mmap-profiler'] + sizes['mmap-type-profiler']))
    if 'hour' in sizes:
      sizes['hour'] = (dump.time - first_dump_time) / 60.0 / 60.0
    if 'minute' in sizes:
      sizes['minute'] = (dump.time - first_dump_time) / 60.0
    if 'second' in sizes:
      sizes['second'] = dump.time - first_dump_time

    return sizes

  @staticmethod
  def _accumulate(dump, policy, bucket_set, sizes):
    for line in dump.iter_stacktrace:
      words = line.split()
      bucket = bucket_set.get(int(words[BUCKET_ID]))
      component_match = policy.find(bucket)
      sizes[component_match] += int(words[COMMITTED])

      if component_match.startswith('tc-'):
        sizes['tc-total-log'] += int(words[COMMITTED])
      elif component_match.startswith('mmap-'):
        sizes['mmap-total-log'] += int(words[COMMITTED])
      else:
        sizes['other-total-log'] += int(words[COMMITTED])


class CSVCommand(PolicyCommands):
  def __init__(self):
    super(CSVCommand, self).__init__('csv')

  def do(self, sys_argv):
    policy_set, dumps, bucket_set = self._set_up(sys_argv)
    return CSVCommand._output(policy_set, dumps, bucket_set, sys.stdout)

  @staticmethod
  def _output(policy_set, dumps, bucket_set, out):
    max_components = 0
    for label in policy_set:
      max_components = max(max_components, len(policy_set[label].components))

    for label in sorted(policy_set):
      components = policy_set[label].components
      if len(policy_set) > 1:
        out.write('%s%s\n' % (label, ',' * (max_components - 1)))
      out.write('%s%s\n' % (
          ','.join(components), ',' * (max_components - len(components))))

      LOGGER.info('Applying a policy %s to...' % label)
      for dump in dumps:
        component_sizes = PolicyCommands._apply_policy(
            dump, policy_set[label], bucket_set, dumps[0].time)
        s = []
        for c in components:
          if c in ('hour', 'minute', 'second'):
            s.append('%05.5f' % (component_sizes[c]))
          else:
            s.append('%05.5f' % (component_sizes[c] / 1024.0 / 1024.0))
        out.write('%s%s\n' % (
              ','.join(s), ',' * (max_components - len(components))))

      bucket_set.clear_component_cache()

    return 0


class JSONCommand(PolicyCommands):
  def __init__(self):
    super(JSONCommand, self).__init__('json')

  def do(self, sys_argv):
    policy_set, dumps, bucket_set = self._set_up(sys_argv)
    return JSONCommand._output(policy_set, dumps, bucket_set, sys.stdout)

  @staticmethod
  def _output(policy_set, dumps, bucket_set, out):
    json_base = {
      'version': 'JSON_DEEP_2',
      'policies': {},
    }

    for label in sorted(policy_set):
      json_base['policies'][label] = {
        'legends': policy_set[label].components,
        'snapshots': [],
      }

      LOGGER.info('Applying a policy %s to...' % label)
      for dump in dumps:
        component_sizes = PolicyCommands._apply_policy(
            dump, policy_set[label], bucket_set, dumps[0].time)
        component_sizes['dump_path'] = dump.path
        component_sizes['dump_time'] = datetime.fromtimestamp(
            dump.time).strftime('%Y-%m-%d %H:%M:%S')
        json_base['policies'][label]['snapshots'].append(component_sizes)

      bucket_set.clear_component_cache()

    json.dump(json_base, out, indent=2, sort_keys=True)

    return 0


class ListCommand(PolicyCommands):
  def __init__(self):
    super(ListCommand, self).__init__('list')

  def do(self, sys_argv):
    policy_set, dumps, bucket_set = self._set_up(sys_argv)
    return ListCommand._output(policy_set, dumps, bucket_set, sys.stdout)

  @staticmethod
  def _output(policy_set, dumps, bucket_set, out):
    for label in sorted(policy_set):
      LOGGER.info('Applying a policy %s to...' % label)
      for dump in dumps:
        component_sizes = PolicyCommands._apply_policy(
            dump, policy_set[label], bucket_set, dump.time)
        out.write('%s for %s:\n' % (label, dump.path))
        for c in policy_set[label].components:
          if c in ['hour', 'minute', 'second']:
            out.write('%40s %12.3f\n' % (c, component_sizes[c]))
          else:
            out.write('%40s %12d\n' % (c, component_sizes[c]))

      bucket_set.clear_component_cache()

    return 0


class ExpandCommand(Command):
  def __init__(self):
    super(ExpandCommand, self).__init__(
        'Usage: %prog expand <dump> <policy> <component> <depth>')

  def do(self, sys_argv):
    _, args = self._parse_args(sys_argv, 4)
    dump_path = args[1]
    target_policy = args[2]
    component_name = args[3]
    depth = args[4]
    (bucket_set, dump) = Command.load_basic_files(dump_path, False)
    policy_set = PolicySet.load(Command._parse_policy_list(target_policy))

    ExpandCommand._output(dump, policy_set[target_policy], bucket_set,
                          component_name, int(depth), sys.stdout)
    return 0

  @staticmethod
  def _output(dump, policy, bucket_set, component_name, depth, out):
    """Prints all stacktraces in a given component of given depth.

    Args:
        dump: A Dump object.
        policy: A Policy object.
        bucket_set: A BucketSet object.
        component_name: A name of component for filtering.
        depth: An integer representing depth to be printed.
        out: An IO object to output.
    """
    sizes = {}

    ExpandCommand._accumulate(
        dump, policy, bucket_set, component_name, depth, sizes)

    sorted_sizes_list = sorted(
        sizes.iteritems(), key=(lambda x: x[1]), reverse=True)
    total = 0
    for size_pair in sorted_sizes_list:
      out.write('%10d %s\n' % (size_pair[1], size_pair[0]))
      total += size_pair[1]
    LOGGER.info('total: %d\n' % total)

  @staticmethod
  def _accumulate(dump, policy, bucket_set, component_name, depth, sizes):
    for line in dump.iter_stacktrace:
      words = line.split()
      bucket = bucket_set.get(int(words[BUCKET_ID]))
      component_match = policy.find(bucket)
      if component_match == component_name:
        stacktrace_sequence = ''
        if bucket.typeinfo:
          stacktrace_sequence += '(type=%s)' % bucket.symbolized_typeinfo
          stacktrace_sequence += ' (type.name=%s) ' % bucket.typeinfo_name
        for stack in bucket.symbolized_stacktrace[
            0 : min(len(bucket.symbolized_stacktrace), 1 + depth)]:
          stacktrace_sequence += stack + ' '
        if not stacktrace_sequence in sizes:
          sizes[stacktrace_sequence] = 0
        sizes[stacktrace_sequence] += int(words[COMMITTED])


class PProfCommand(Command):
  def __init__(self):
    super(PProfCommand, self).__init__(
        'Usage: %prog pprof [-c COMPONENT] <dump> <policy>')
    self._parser.add_option('-c', '--component', type='string',
                            dest='component',
                            help='restrict to COMPONENT', metavar='COMPONENT')

  def do(self, sys_argv):
    options, args = self._parse_args(sys_argv, 2)

    dump_path = args[1]
    target_policy = args[2]
    component = options.component

    (bucket_set, dump) = Command.load_basic_files(dump_path, False)
    policy_set = PolicySet.load(Command._parse_policy_list(target_policy))

    with open(Command._find_prefix(dump_path) + '.maps', 'r') as maps_f:
      maps_lines = maps_f.readlines()
    PProfCommand._output(
        dump, policy_set[target_policy], bucket_set, maps_lines, component,
        sys.stdout)

    return 0

  @staticmethod
  def _output(dump, policy, bucket_set, maps_lines, component_name, out):
    """Converts the heap profile dump so it can be processed by pprof.

    Args:
        dump: A Dump object.
        policy: A Policy object.
        bucket_set: A BucketSet object.
        maps_lines: A list of strings containing /proc/.../maps.
        component_name: A name of component for filtering.
        out: An IO object to output.
    """
    out.write('heap profile: ')
    com_committed, com_allocs = PProfCommand._accumulate(
        dump, policy, bucket_set, component_name)

    out.write('%6d: %8s [%6d: %8s] @ heapprofile\n' % (
        com_allocs, com_committed, com_allocs, com_committed))

    PProfCommand._output_stacktrace_lines(
        dump, policy, bucket_set, component_name, out)

    out.write('MAPPED_LIBRARIES:\n')
    for line in maps_lines:
      out.write(line)

  @staticmethod
  def _accumulate(dump, policy, bucket_set, component_name):
    """Accumulates size of committed chunks and the number of allocated chunks.

    Args:
        dump: A Dump object.
        policy: A Policy object.
        bucket_set: A BucketSet object.
        component_name: A name of component for filtering.

    Returns:
        Two integers which are the accumulated size of committed regions and the
        number of allocated chunks, respectively.
    """
    com_committed = 0
    com_allocs = 0
    for line in dump.iter_stacktrace:
      words = line.split()
      bucket = bucket_set.get(int(words[BUCKET_ID]))
      if (not bucket or
          (component_name and component_name != policy.find(bucket))):
        continue

      com_committed += int(words[COMMITTED])
      com_allocs += int(words[ALLOC_COUNT]) - int(words[FREE_COUNT])

    return com_committed, com_allocs

  @staticmethod
  def _output_stacktrace_lines(dump, policy, bucket_set, component_name, out):
    """Prints information of stacktrace lines for pprof.

    Args:
        dump: A Dump object.
        policy: A Policy object.
        bucket_set: A BucketSet object.
        component_name: A name of component for filtering.
        out: An IO object to output.
    """
    for line in dump.iter_stacktrace:
      words = line.split()
      bucket = bucket_set.get(int(words[BUCKET_ID]))
      if (not bucket or
          (component_name and component_name != policy.find(bucket))):
        continue

      out.write('%6d: %8s [%6d: %8s] @' % (
          int(words[ALLOC_COUNT]) - int(words[FREE_COUNT]),
          words[COMMITTED],
          int(words[ALLOC_COUNT]) - int(words[FREE_COUNT]),
          words[COMMITTED]))
      for address in bucket.stacktrace:
        out.write(' 0x%016x' % address)
      out.write('\n')


class UploadCommand(Command):
  def __init__(self):
    super(UploadCommand, self).__init__(
        'Usage: %prog upload [--gsutil path/to/gsutil] '
        '<first-dump> <destination-gs-path>')
    self._parser.add_option('--gsutil', default='gsutil',
                            help='path to GSUTIL', metavar='GSUTIL')

  def do(self, sys_argv):
    options, args = self._parse_args(sys_argv, 2)
    dump_path = args[1]
    gs_path = args[2]

    dump_files = Command._find_all_dumps(dump_path)
    bucket_files = Command._find_all_buckets(dump_path)
    prefix = Command._find_prefix(dump_path)
    symbol_data_sources = SymbolDataSources(prefix)
    symbol_data_sources.prepare()
    symbol_path = symbol_data_sources.path()

    handle_zip, filename_zip = tempfile.mkstemp('.zip', 'dmprof')
    os.close(handle_zip)

    try:
      file_zip = zipfile.ZipFile(filename_zip, 'w', zipfile.ZIP_DEFLATED)
      for filename in dump_files:
        file_zip.write(filename, os.path.basename(os.path.abspath(filename)))
      for filename in bucket_files:
        file_zip.write(filename, os.path.basename(os.path.abspath(filename)))

      symbol_basename = os.path.basename(os.path.abspath(symbol_path))
      for filename in os.listdir(symbol_path):
        if not filename.startswith('.'):
          file_zip.write(os.path.join(symbol_path, filename),
                         os.path.join(symbol_basename, os.path.basename(
                             os.path.abspath(filename))))
      file_zip.close()

      returncode = UploadCommand._run_gsutil(
          options.gsutil, 'cp', '-a', 'public-read', filename_zip, gs_path)
    finally:
      os.remove(filename_zip)

    return returncode

  @staticmethod
  def _run_gsutil(gsutil, *args):
    """Run gsutil as a subprocess.

    Args:
        *args: Arguments to pass to gsutil. The first argument should be an
            operation such as ls, cp or cat.
    Returns:
        The return code from the process.
    """
    command = [gsutil] + list(args)
    LOGGER.info("Running: %s", command)

    try:
      return subprocess.call(command)
    except OSError, e:
      LOGGER.error('Error to run gsutil: %s', e)


def main():
  COMMANDS = {
    'csv': CSVCommand,
    'expand': ExpandCommand,
    'json': JSONCommand,
    'list': ListCommand,
    'pprof': PProfCommand,
    'stacktrace': StacktraceCommand,
    'upload': UploadCommand,
  }

  if len(sys.argv) < 2 or (not sys.argv[1] in COMMANDS):
    sys.stderr.write("""Usage: dmprof <command> [options] [<args>]

Commands:
   csv          Classify memory usage in CSV
   expand       Show all stacktraces contained in the specified component
   json         Classify memory usage in JSON
   list         Classify memory usage in simple listing format
   pprof        Format the profile dump so that it can be processed by pprof
   stacktrace   Convert runtime addresses to symbol names
   upload       Upload dumped files

Quick Reference:
   dmprof csv [-p POLICY] <first-dump>
   dmprof expand <dump> <policy> <component> <depth>
   dmprof json [-p POLICY] <first-dump>
   dmprof list [-p POLICY] <first-dump>
   dmprof pprof [-c COMPONENT] <dump> <policy>
   dmprof stacktrace <dump>
   dmprof upload [--gsutil path/to/gsutil] <first-dump> <destination-gs-path>
""")
    sys.exit(1)
  action = sys.argv.pop(1)

  LOGGER.setLevel(logging.DEBUG)
  handler = logging.StreamHandler()
  handler.setLevel(logging.INFO)
  formatter = logging.Formatter('%(message)s')
  handler.setFormatter(formatter)
  LOGGER.addHandler(handler)

  try:
    errorcode = COMMANDS[action]().do(sys.argv)
  except ParsingException, e:
    errorcode = 1
    sys.stderr.write('Exit by parsing error: %s\n' % e)

  return errorcode


if __name__ == '__main__':
  sys.exit(main())
