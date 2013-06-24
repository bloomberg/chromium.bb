# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""The deep heap profiler script for Chrome."""

import copy
import datetime
import json
import logging
import optparse
import os
import re
import struct
import subprocess
import sys
import tempfile
import time
import zipfile

from range_dict import ExclusiveRangeDict

BASE_PATH = os.path.dirname(os.path.abspath(__file__))
FIND_RUNTIME_SYMBOLS_PATH = os.path.join(
    BASE_PATH, os.pardir, 'find_runtime_symbols')
sys.path.append(FIND_RUNTIME_SYMBOLS_PATH)

import find_runtime_symbols
import prepare_symbol_info
import proc_maps

from find_runtime_symbols import FUNCTION_SYMBOLS
from find_runtime_symbols import SOURCEFILE_SYMBOLS
from find_runtime_symbols import TYPEINFO_SYMBOLS

BUCKET_ID = 5
VIRTUAL = 0
COMMITTED = 1
ALLOC_COUNT = 2
FREE_COUNT = 3
NULL_REGEX = re.compile('')

LOGGER = logging.getLogger('dmprof')
POLICIES_JSON_PATH = os.path.join(BASE_PATH, 'policies.json')
CHROME_SRC_PATH = os.path.join(BASE_PATH, os.pardir, os.pardir)


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


class ListAttribute(ExclusiveRangeDict.RangeAttribute):
  """Represents a list for an attribute in range_dict.ExclusiveRangeDict."""
  def __init__(self):
    super(ListAttribute, self).__init__()
    self._list = []

  def __str__(self):
    return str(self._list)

  def __repr__(self):
    return 'ListAttribute' + str(self._list)

  def __len__(self):
    return len(self._list)

  def __iter__(self):
    for x in self._list:
      yield x

  def __getitem__(self, index):
    return self._list[index]

  def __setitem__(self, index, value):
    if index >= len(self._list):
      self._list.extend([None] * (index + 1 - len(self._list)))
    self._list[index] = value

  def copy(self):
    new_list = ListAttribute()
    for index, item in enumerate(self._list):
      new_list[index] = copy.deepcopy(item)
    return new_list


class ProcMapsEntryAttribute(ExclusiveRangeDict.RangeAttribute):
  """Represents an entry of /proc/maps in range_dict.ExclusiveRangeDict."""
  _DUMMY_ENTRY = proc_maps.ProcMapsEntry(
      0,     # begin
      0,     # end
      '-',   # readable
      '-',   # writable
      '-',   # executable
      '-',   # private
      0,     # offset
      '00',  # major
      '00',  # minor
      0,     # inode
      ''     # name
      )

  def __init__(self):
    super(ProcMapsEntryAttribute, self).__init__()
    self._entry = self._DUMMY_ENTRY.as_dict()

  def __str__(self):
    return str(self._entry)

  def __repr__(self):
    return 'ProcMapsEntryAttribute' + str(self._entry)

  def __getitem__(self, key):
    return self._entry[key]

  def __setitem__(self, key, value):
    if key not in self._entry:
      raise KeyError(key)
    self._entry[key] = value

  def copy(self):
    new_entry = ProcMapsEntryAttribute()
    for key, value in self._entry.iteritems():
      new_entry[key] = copy.deepcopy(value)
    return new_entry


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
  def __init__(self, prefix, alternative_dirs=None):
    self._prefix = prefix
    self._prepared_symbol_data_sources_path = None
    self._loaded_symbol_data_sources = None
    self._alternative_dirs = alternative_dirs or {}

  def prepare(self):
    """Prepares symbol data sources by extracting mapping from a binary.

    The prepared symbol data sources are stored in a directory.  The directory
    name is stored in |self._prepared_symbol_data_sources_path|.

    Returns:
        True if succeeded.
    """
    LOGGER.info('Preparing symbol mapping...')
    self._prepared_symbol_data_sources_path, used_tempdir = (
        prepare_symbol_info.prepare_symbol_info(
            self._prefix + '.maps',
            output_dir_path=self._prefix + '.symmap',
            alternative_dirs=self._alternative_dirs,
            use_tempdir=True,
            use_source_file_name=True))
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
      self._loaded_symbol_data_sources = (
          find_runtime_symbols.RuntimeSymbolsInProcess.load(
              self._prepared_symbol_data_sources_path))
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
  def __init__(self, symbol_type, symbol_data_sources):
    self._symbol_type = symbol_type
    self._symbol_data_sources = symbol_data_sources

  def find(self, address_list):
    return find_runtime_symbols.find_runtime_symbols(
        self._symbol_type, self._symbol_data_sources.get(), address_list)


class SymbolMappingCache(object):
  """Caches mapping from actually used addresses to symbols.

  'update()' updates the cache from the original symbol data sources via
  'SymbolFinder'.  Symbols can be looked up by the method 'lookup()'.
  """
  def __init__(self):
    self._symbol_mapping_caches = {
        FUNCTION_SYMBOLS: {},
        SOURCEFILE_SYMBOLS: {},
        TYPEINFO_SYMBOLS: {},
        }

  def update(self, symbol_type, bucket_set, symbol_finder, cache_f):
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
        symbol_type: A type of symbols to update.  It should be one of
            FUNCTION_SYMBOLS, SOURCEFILE_SYMBOLS and TYPEINFO_SYMBOLS.
        bucket_set: A BucketSet object.
        symbol_finder: A SymbolFinder object to find symbols.
        cache_f: A readable and writable IO object of the symbol cache file.
    """
    cache_f.seek(0, os.SEEK_SET)
    self._load(cache_f, symbol_type)

    unresolved_addresses = sorted(
        address for address in bucket_set.iter_addresses(symbol_type)
        if address not in self._symbol_mapping_caches[symbol_type])

    if not unresolved_addresses:
      LOGGER.info('No need to resolve any more addresses.')
      return

    cache_f.seek(0, os.SEEK_END)
    LOGGER.info('Loading %d unresolved addresses.' %
                len(unresolved_addresses))
    symbol_dict = symbol_finder.find(unresolved_addresses)

    for address, symbol in symbol_dict.iteritems():
      stripped_symbol = symbol.strip() or '?'
      self._symbol_mapping_caches[symbol_type][address] = stripped_symbol
      cache_f.write('%x %s\n' % (address, stripped_symbol))

  def lookup(self, symbol_type, address):
    """Looks up a symbol for a given |address|.

    Args:
        symbol_type: A type of symbols to update.  It should be one of
            FUNCTION_SYMBOLS, SOURCEFILE_SYMBOLS and TYPEINFO_SYMBOLS.
        address: An integer that represents an address.

    Returns:
        A string that represents a symbol.
    """
    return self._symbol_mapping_caches[symbol_type].get(address)

  def _load(self, cache_f, symbol_type):
    try:
      for line in cache_f:
        items = line.rstrip().split(None, 1)
        if len(items) == 1:
          items.append('??')
        self._symbol_mapping_caches[symbol_type][int(items[0], 16)] = items[1]
      LOGGER.info('Loaded %d entries from symbol cache.' %
                     len(self._symbol_mapping_caches[symbol_type]))
    except IOError as e:
      LOGGER.info('The symbol cache file is invalid: %s' % e)


class Rule(object):
  """Represents one matching rule in a policy file."""

  def __init__(self,
               name,
               allocator_type,
               stackfunction_pattern=None,
               stacksourcefile_pattern=None,
               typeinfo_pattern=None,
               mappedpathname_pattern=None,
               mappedpermission_pattern=None,
               sharedwith=None):
    self._name = name
    self._allocator_type = allocator_type

    self._stackfunction_pattern = None
    if stackfunction_pattern:
      self._stackfunction_pattern = re.compile(
          stackfunction_pattern + r'\Z')

    self._stacksourcefile_pattern = None
    if stacksourcefile_pattern:
      self._stacksourcefile_pattern = re.compile(
          stacksourcefile_pattern + r'\Z')

    self._typeinfo_pattern = None
    if typeinfo_pattern:
      self._typeinfo_pattern = re.compile(typeinfo_pattern + r'\Z')

    self._mappedpathname_pattern = None
    if mappedpathname_pattern:
      self._mappedpathname_pattern = re.compile(mappedpathname_pattern + r'\Z')

    self._mappedpermission_pattern = None
    if mappedpermission_pattern:
      self._mappedpermission_pattern = re.compile(
          mappedpermission_pattern + r'\Z')

    self._sharedwith = list()
    if sharedwith:
      self._sharedwith = sharedwith

  @property
  def name(self):
    return self._name

  @property
  def allocator_type(self):
    return self._allocator_type

  @property
  def stackfunction_pattern(self):
    return self._stackfunction_pattern

  @property
  def stacksourcefile_pattern(self):
    return self._stacksourcefile_pattern

  @property
  def typeinfo_pattern(self):
    return self._typeinfo_pattern

  @property
  def mappedpathname_pattern(self):
    return self._mappedpathname_pattern

  @property
  def mappedpermission_pattern(self):
    return self._mappedpermission_pattern

  @property
  def sharedwith(self):
    return self._sharedwith


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

  def find_rule(self, component_name):
    """Finds a rule whose name is |component_name|. """
    for rule in self._rules:
      if rule.name == component_name:
        return rule
    return None

  def find_malloc(self, bucket):
    """Finds a matching component name which a given |bucket| belongs to.

    Args:
        bucket: A Bucket object to be searched for.

    Returns:
        A string representing a component name.
    """
    assert not bucket or bucket.allocator_type == 'malloc'

    if not bucket:
      return 'no-bucket'
    if bucket.component_cache:
      return bucket.component_cache

    stackfunction = bucket.symbolized_joined_stackfunction
    stacksourcefile = bucket.symbolized_joined_stacksourcefile
    typeinfo = bucket.symbolized_typeinfo
    if typeinfo.startswith('0x'):
      typeinfo = bucket.typeinfo_name

    for rule in self._rules:
      if (rule.allocator_type == 'malloc' and
          (not rule.stackfunction_pattern or
           rule.stackfunction_pattern.match(stackfunction)) and
          (not rule.stacksourcefile_pattern or
           rule.stacksourcefile_pattern.match(stacksourcefile)) and
          (not rule.typeinfo_pattern or rule.typeinfo_pattern.match(typeinfo))):
        bucket.component_cache = rule.name
        return rule.name

    assert False

  def find_mmap(self, region, bucket_set,
                pageframe=None, group_pfn_counts=None):
    """Finds a matching component which a given mmap |region| belongs to.

    It uses |bucket_set| to match with backtraces.  If |pageframe| is given,
    it considers memory sharing among processes.

    NOTE: Don't use Bucket's |component_cache| for mmap regions because they're
    classified not only with bucket information (mappedpathname for example).

    Args:
        region: A tuple representing a memory region.
        bucket_set: A BucketSet object to look up backtraces.
        pageframe: A PageFrame object representing a pageframe maybe including
            a pagecount.
        group_pfn_counts: A dict mapping a PFN to the number of times the
            the pageframe is mapped by the known "group (Chrome)" processes.

    Returns:
        A string representing a component name.
    """
    assert region[0] == 'hooked'
    bucket = bucket_set.get(region[1]['bucket_id'])
    assert not bucket or bucket.allocator_type == 'mmap'

    if not bucket:
      return 'no-bucket', None

    stackfunction = bucket.symbolized_joined_stackfunction
    stacksourcefile = bucket.symbolized_joined_stacksourcefile
    sharedwith = self._categorize_pageframe(pageframe, group_pfn_counts)

    for rule in self._rules:
      if (rule.allocator_type == 'mmap' and
          (not rule.stackfunction_pattern or
           rule.stackfunction_pattern.match(stackfunction)) and
          (not rule.stacksourcefile_pattern or
           rule.stacksourcefile_pattern.match(stacksourcefile)) and
          (not rule.mappedpathname_pattern or
           rule.mappedpathname_pattern.match(region[1]['vma']['name'])) and
          (not rule.mappedpermission_pattern or
           rule.mappedpermission_pattern.match(
               region[1]['vma']['readable'] +
               region[1]['vma']['writable'] +
               region[1]['vma']['executable'] +
               region[1]['vma']['private'])) and
          (not rule.sharedwith or
           not pageframe or sharedwith in rule.sharedwith)):
        return rule.name, bucket

    assert False

  def find_unhooked(self, region, pageframe=None, group_pfn_counts=None):
    """Finds a matching component which a given unhooked |region| belongs to.

    If |pageframe| is given, it considers memory sharing among processes.

    Args:
        region: A tuple representing a memory region.
        pageframe: A PageFrame object representing a pageframe maybe including
            a pagecount.
        group_pfn_counts: A dict mapping a PFN to the number of times the
            the pageframe is mapped by the known "group (Chrome)" processes.

    Returns:
        A string representing a component name.
    """
    assert region[0] == 'unhooked'
    sharedwith = self._categorize_pageframe(pageframe, group_pfn_counts)

    for rule in self._rules:
      if (rule.allocator_type == 'unhooked' and
          (not rule.mappedpathname_pattern or
           rule.mappedpathname_pattern.match(region[1]['vma']['name'])) and
          (not rule.mappedpermission_pattern or
           rule.mappedpermission_pattern.match(
               region[1]['vma']['readable'] +
               region[1]['vma']['writable'] +
               region[1]['vma']['executable'] +
               region[1]['vma']['private'])) and
          (not rule.sharedwith or
           not pageframe or sharedwith in rule.sharedwith)):
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
      stackfunction = rule.get('stackfunction') or rule.get('stacktrace')
      stacksourcefile = rule.get('stacksourcefile')
      rules.append(Rule(
          rule['name'],
          rule['allocator'],  # allocator_type
          stackfunction,
          stacksourcefile,
          rule['typeinfo'] if 'typeinfo' in rule else None,
          rule.get('mappedpathname'),
          rule.get('mappedpermission'),
          rule.get('sharedwith')))

    return Policy(rules, policy['version'], policy['components'])

  @staticmethod
  def _categorize_pageframe(pageframe, group_pfn_counts):
    """Categorizes a pageframe based on its sharing status.

    Returns:
        'private' if |pageframe| is not shared with other processes.  'group'
        if |pageframe| is shared only with group (Chrome-related) processes.
        'others' if |pageframe| is shared with non-group processes.
    """
    if not pageframe:
      return 'private'

    if pageframe.pagecount:
      if pageframe.pagecount == 1:
        return 'private'
      elif pageframe.pagecount <= group_pfn_counts.get(pageframe.pfn, 0) + 1:
        return 'group'
      else:
        return 'others'
    else:
      if pageframe.pfn in group_pfn_counts:
        return 'group'
      else:
        return 'private'


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

  def __init__(self, stacktrace, allocator_type, typeinfo, typeinfo_name):
    self._stacktrace = stacktrace
    self._allocator_type = allocator_type
    self._typeinfo = typeinfo
    self._typeinfo_name = typeinfo_name

    self._symbolized_stackfunction = stacktrace
    self._symbolized_joined_stackfunction = ''
    self._symbolized_stacksourcefile = stacktrace
    self._symbolized_joined_stacksourcefile = ''
    self._symbolized_typeinfo = typeinfo_name

    self.component_cache = ''

  def __str__(self):
    result = []
    result.append(self._allocator_type)
    if self._symbolized_typeinfo == 'no typeinfo':
      result.append('tno_typeinfo')
    else:
      result.append('t' + self._symbolized_typeinfo)
    result.append('n' + self._typeinfo_name)
    result.extend(['%s(@%s)' % (function, sourcefile)
                   for function, sourcefile
                   in zip(self._symbolized_stackfunction,
                          self._symbolized_stacksourcefile)])
    return ' '.join(result)

  def symbolize(self, symbol_mapping_cache):
    """Makes a symbolized stacktrace and typeinfo with |symbol_mapping_cache|.

    Args:
        symbol_mapping_cache: A SymbolMappingCache object.
    """
    # TODO(dmikurube): Fill explicitly with numbers if symbol not found.
    self._symbolized_stackfunction = [
        symbol_mapping_cache.lookup(FUNCTION_SYMBOLS, address)
        for address in self._stacktrace]
    self._symbolized_joined_stackfunction = ' '.join(
        self._symbolized_stackfunction)
    self._symbolized_stacksourcefile = [
        symbol_mapping_cache.lookup(SOURCEFILE_SYMBOLS, address)
        for address in self._stacktrace]
    self._symbolized_joined_stacksourcefile = ' '.join(
        self._symbolized_stacksourcefile)
    if not self._typeinfo:
      self._symbolized_typeinfo = 'no typeinfo'
    else:
      self._symbolized_typeinfo = symbol_mapping_cache.lookup(
          TYPEINFO_SYMBOLS, self._typeinfo)
      if not self._symbolized_typeinfo:
        self._symbolized_typeinfo = 'no typeinfo'

  def clear_component_cache(self):
    self.component_cache = ''

  @property
  def stacktrace(self):
    return self._stacktrace

  @property
  def allocator_type(self):
    return self._allocator_type

  @property
  def typeinfo(self):
    return self._typeinfo

  @property
  def typeinfo_name(self):
    return self._typeinfo_name

  @property
  def symbolized_stackfunction(self):
    return self._symbolized_stackfunction

  @property
  def symbolized_joined_stackfunction(self):
    return self._symbolized_joined_stackfunction

  @property
  def symbolized_stacksourcefile(self):
    return self._symbolized_stacksourcefile

  @property
  def symbolized_joined_stacksourcefile(self):
    return self._symbolized_joined_stacksourcefile

  @property
  def symbolized_typeinfo(self):
    return self._symbolized_typeinfo


class BucketSet(object):
  """Represents a set of bucket."""
  def __init__(self):
    self._buckets = {}
    self._code_addresses = set()
    self._typeinfo_addresses = set()

  def load(self, prefix):
    """Loads all related bucket files.

    Args:
        prefix: A prefix string for bucket file names.
    """
    LOGGER.info('Loading bucket files.')

    n = 0
    skipped = 0
    while True:
      path = '%s.%04d.buckets' % (prefix, n)
      if not os.path.exists(path) or not os.stat(path).st_size:
        if skipped > 10:
          break
        n += 1
        skipped += 1
        continue
      LOGGER.info('  %s' % path)
      with open(path, 'r') as f:
        self._load_file(f)
      n += 1
      skipped = 0

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
          self._typeinfo_addresses.add(typeinfo)
        elif word[0] == 'n':
          typeinfo_name = word[1:]
        else:
          stacktrace_begin = index
          break
      stacktrace = [int(address, 16) for address in words[stacktrace_begin:]]
      for frame in stacktrace:
        self._code_addresses.add(frame)
      self._buckets[int(words[0])] = Bucket(
          stacktrace, words[1], typeinfo, typeinfo_name)

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

  def iter_addresses(self, symbol_type):
    if symbol_type in [FUNCTION_SYMBOLS, SOURCEFILE_SYMBOLS]:
      for function in self._code_addresses:
        yield function
    else:
      for function in self._typeinfo_addresses:
        yield function


class PageFrame(object):
  """Represents a pageframe and maybe its shared count."""
  def __init__(self, pfn, size, pagecount, start_truncated, end_truncated):
    self._pfn = pfn
    self._size = size
    self._pagecount = pagecount
    self._start_truncated = start_truncated
    self._end_truncated = end_truncated

  def __str__(self):
    result = str()
    if self._start_truncated:
      result += '<'
    result += '%06x#%d' % (self._pfn, self._pagecount)
    if self._end_truncated:
      result += '>'
    return result

  def __repr__(self):
    return str(self)

  @staticmethod
  def parse(encoded_pfn, size):
    start = 0
    end = len(encoded_pfn)
    end_truncated = False
    if encoded_pfn.endswith('>'):
      end = len(encoded_pfn) - 1
      end_truncated = True
    pagecount_found = encoded_pfn.find('#')
    pagecount = None
    if pagecount_found >= 0:
      encoded_pagecount = 'AAA' + encoded_pfn[pagecount_found+1 : end]
      pagecount = struct.unpack(
          '>I', '\x00' + encoded_pagecount.decode('base64'))[0]
      end = pagecount_found
    start_truncated = False
    if encoded_pfn.startswith('<'):
      start = 1
      start_truncated = True

    pfn = struct.unpack(
        '>I', '\x00' + (encoded_pfn[start:end]).decode('base64'))[0]

    return PageFrame(pfn, size, pagecount, start_truncated, end_truncated)

  @property
  def pfn(self):
    return self._pfn

  @property
  def size(self):
    return self._size

  def set_size(self, size):
    self._size = size

  @property
  def pagecount(self):
    return self._pagecount

  @property
  def start_truncated(self):
    return self._start_truncated

  @property
  def end_truncated(self):
    return self._end_truncated


class PFNCounts(object):
  """Represents counts of PFNs in a process."""

  _PATH_PATTERN = re.compile(r'^(.*)\.([0-9]+)\.([0-9]+)\.heap$')

  def __init__(self, path, modified_time):
    matched = self._PATH_PATTERN.match(path)
    if matched:
      self._pid = int(matched.group(2))
    else:
      self._pid = 0
    self._command_line = ''
    self._pagesize = 4096
    self._path = path
    self._pfn_meta = ''
    self._pfnset = dict()
    self._reason = ''
    self._time = modified_time

  @staticmethod
  def load(path, log_header='Loading PFNs from a heap profile dump: '):
    pfnset = PFNCounts(path, float(os.stat(path).st_mtime))
    LOGGER.info('%s%s' % (log_header, path))

    with open(path, 'r') as pfnset_f:
      pfnset.load_file(pfnset_f)

    return pfnset

  @property
  def path(self):
    return self._path

  @property
  def pid(self):
    return self._pid

  @property
  def time(self):
    return self._time

  @property
  def reason(self):
    return self._reason

  @property
  def iter_pfn(self):
    for pfn, count in self._pfnset.iteritems():
      yield pfn, count

  def load_file(self, pfnset_f):
    prev_pfn_end_truncated = None
    for line in pfnset_f:
      line = line.strip()
      if line.startswith('GLOBAL_STATS:') or line.startswith('STACKTRACES:'):
        break
      elif line.startswith('PF: '):
        for encoded_pfn in line[3:].split():
          page_frame = PageFrame.parse(encoded_pfn, self._pagesize)
          if page_frame.start_truncated and (
              not prev_pfn_end_truncated or
              prev_pfn_end_truncated != page_frame.pfn):
            LOGGER.error('Broken page frame number: %s.' % encoded_pfn)
          self._pfnset[page_frame.pfn] = self._pfnset.get(page_frame.pfn, 0) + 1
          if page_frame.end_truncated:
            prev_pfn_end_truncated = page_frame.pfn
          else:
            prev_pfn_end_truncated = None
      elif line.startswith('PageSize: '):
        self._pagesize = int(line[10:])
      elif line.startswith('PFN: '):
        self._pfn_meta = line[5:]
      elif line.startswith('PageFrame: '):
        self._pfn_meta = line[11:]
      elif line.startswith('Time: '):
        self._time = float(line[6:])
      elif line.startswith('CommandLine: '):
        self._command_line = line[13:]
      elif line.startswith('Reason: '):
        self._reason = line[8:]


class Dump(object):
  """Represents a heap profile dump."""

  _PATH_PATTERN = re.compile(r'^(.*)\.([0-9]+)\.([0-9]+)\.heap$')

  _HOOK_PATTERN = re.compile(
      r'^ ([ \(])([a-f0-9]+)([ \)])-([ \(])([a-f0-9]+)([ \)])\s+'
      r'(hooked|unhooked)\s+(.+)$', re.IGNORECASE)

  _HOOKED_PATTERN = re.compile(r'(?P<TYPE>.+ )?(?P<COMMITTED>[0-9]+) / '
                               '(?P<RESERVED>[0-9]+) @ (?P<BUCKETID>[0-9]+)')
  _UNHOOKED_PATTERN = re.compile(r'(?P<TYPE>.+ )?(?P<COMMITTED>[0-9]+) / '
                                 '(?P<RESERVED>[0-9]+)')

  _OLD_HOOKED_PATTERN = re.compile(r'(?P<TYPE>.+) @ (?P<BUCKETID>[0-9]+)')
  _OLD_UNHOOKED_PATTERN = re.compile(r'(?P<TYPE>.+) (?P<COMMITTED>[0-9]+)')

  _TIME_PATTERN_FORMAT = re.compile(
      r'^Time: ([0-9]+/[0-9]+/[0-9]+ [0-9]+:[0-9]+:[0-9]+)(\.[0-9]+)?')
  _TIME_PATTERN_SECONDS = re.compile(r'^Time: ([0-9]+)$')

  def __init__(self, path, modified_time):
    self._path = path
    matched = self._PATH_PATTERN.match(path)
    self._pid = int(matched.group(2))
    self._count = int(matched.group(3))
    self._time = modified_time
    self._map = {}
    self._procmaps = ExclusiveRangeDict(ProcMapsEntryAttribute)
    self._stacktrace_lines = []
    self._global_stats = {} # used only in apply_policy

    self._pagesize = 4096
    self._pageframe_length = 0
    self._pageframe_encoding = ''
    self._has_pagecount = False

    self._version = ''
    self._lines = []

  @property
  def path(self):
    return self._path

  @property
  def count(self):
    return self._count

  @property
  def time(self):
    return self._time

  @property
  def iter_map(self):
    for region in sorted(self._map.iteritems()):
      yield region[0], region[1]

  def iter_procmaps(self):
    for begin, end, attr in self._map.iter_range():
      yield begin, end, attr

  @property
  def iter_stacktrace(self):
    for line in self._stacktrace_lines:
      yield line

  def global_stat(self, name):
    return self._global_stats[name]

  @property
  def pagesize(self):
    return self._pagesize

  @property
  def pageframe_length(self):
    return self._pageframe_length

  @property
  def pageframe_encoding(self):
    return self._pageframe_encoding

  @property
  def has_pagecount(self):
    return self._has_pagecount

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
      self._parse_meta_information()
      if self._version == DUMP_DEEP_6:
        self._parse_mmap_list()
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

  def _parse_meta_information(self):
    """Parses lines in self._lines for meta information."""
    (ln, found) = skip_while(
        0, len(self._lines),
        lambda n: self._lines[n] != 'META:\n')
    if not found:
      return
    ln += 1

    while True:
      if self._lines[ln].startswith('Time:'):
        matched_seconds = self._TIME_PATTERN_SECONDS.match(self._lines[ln])
        matched_format = self._TIME_PATTERN_FORMAT.match(self._lines[ln])
        if matched_format:
          self._time = time.mktime(datetime.datetime.strptime(
              matched_format.group(1), '%Y/%m/%d %H:%M:%S').timetuple())
          if matched_format.group(2):
            self._time += float(matched_format.group(2)[1:]) / 1000.0
        elif matched_seconds:
          self._time = float(matched_seconds.group(1))
      elif self._lines[ln].startswith('Reason:'):
        pass  # Nothing to do for 'Reason:'
      elif self._lines[ln].startswith('PageSize: '):
        self._pagesize = int(self._lines[ln][10:])
      elif self._lines[ln].startswith('CommandLine:'):
        pass
      elif (self._lines[ln].startswith('PageFrame: ') or
            self._lines[ln].startswith('PFN: ')):
        if self._lines[ln].startswith('PageFrame: '):
          words = self._lines[ln][11:].split(',')
        else:
          words = self._lines[ln][5:].split(',')
        for word in words:
          if word == '24':
            self._pageframe_length = 24
          elif word == 'Base64':
            self._pageframe_encoding = 'base64'
          elif word == 'PageCount':
            self._has_pagecount = True
      else:
        break
      ln += 1

  def _parse_mmap_list(self):
    """Parses lines in self._lines as a mmap list."""
    (ln, found) = skip_while(
        0, len(self._lines),
        lambda n: self._lines[n] != 'MMAP_LIST:\n')
    if not found:
      return {}

    ln += 1
    self._map = dict()
    current_vma = dict()
    pageframe_list = list()
    while True:
      entry = proc_maps.ProcMaps.parse_line(self._lines[ln])
      if entry:
        current_vma = dict()
        for _, _, attr in self._procmaps.iter_range(entry.begin, entry.end):
          for key, value in entry.as_dict().iteritems():
            attr[key] = value
            current_vma[key] = value
        ln += 1
        continue

      if self._lines[ln].startswith('  PF: '):
        for pageframe in self._lines[ln][5:].split():
          pageframe_list.append(PageFrame.parse(pageframe, self._pagesize))
        ln += 1
        continue

      matched = self._HOOK_PATTERN.match(self._lines[ln])
      if not matched:
        break
      # 2: starting address
      # 5: end address
      # 7: hooked or unhooked
      # 8: additional information
      if matched.group(7) == 'hooked':
        submatched = self._HOOKED_PATTERN.match(matched.group(8))
        if not submatched:
          submatched = self._OLD_HOOKED_PATTERN.match(matched.group(8))
      elif matched.group(7) == 'unhooked':
        submatched = self._UNHOOKED_PATTERN.match(matched.group(8))
        if not submatched:
          submatched = self._OLD_UNHOOKED_PATTERN.match(matched.group(8))
      else:
        assert matched.group(7) in ['hooked', 'unhooked']

      submatched_dict = submatched.groupdict()
      region_info = { 'vma': current_vma }
      if submatched_dict.get('TYPE'):
        region_info['type'] = submatched_dict['TYPE'].strip()
      if submatched_dict.get('COMMITTED'):
        region_info['committed'] = int(submatched_dict['COMMITTED'])
      if submatched_dict.get('RESERVED'):
        region_info['reserved'] = int(submatched_dict['RESERVED'])
      if submatched_dict.get('BUCKETID'):
        region_info['bucket_id'] = int(submatched_dict['BUCKETID'])

      if matched.group(1) == '(':
        start = current_vma['begin']
      else:
        start = int(matched.group(2), 16)
      if matched.group(4) == '(':
        end = current_vma['end']
      else:
        end = int(matched.group(5), 16)

      if pageframe_list and pageframe_list[0].start_truncated:
        pageframe_list[0].set_size(
            pageframe_list[0].size - start % self._pagesize)
      if pageframe_list and pageframe_list[-1].end_truncated:
        pageframe_list[-1].set_size(
            pageframe_list[-1].size - (self._pagesize - end % self._pagesize))
      region_info['pageframe'] = pageframe_list
      pageframe_list = list()

      self._map[(start, end)] = (matched.group(7), region_info)
      ln += 1

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
  _DEVICE_LIB_BASEDIRS = ['/data/data/', '/data/app-lib/', '/data/local/tmp']

  def __init__(self, usage):
    self._parser = optparse.OptionParser(usage)

  @staticmethod
  def load_basic_files(
      dump_path, multiple, no_dump=False, alternative_dirs=None):
    prefix = Command._find_prefix(dump_path)
    # If the target process is estimated to be working on Android, converts
    # a path in the Android device to a path estimated to be corresponding in
    # the host.  Use --alternative-dirs to specify the conversion manually.
    if not alternative_dirs:
      alternative_dirs = Command._estimate_alternative_dirs(prefix)
    if alternative_dirs:
      for device, host in alternative_dirs.iteritems():
        LOGGER.info('Assuming %s on device as %s on host' % (device, host))
    symbol_data_sources = SymbolDataSources(prefix, alternative_dirs)
    symbol_data_sources.prepare()
    bucket_set = BucketSet()
    bucket_set.load(prefix)
    if not no_dump:
      if multiple:
        dump_list = DumpList.load(Command._find_all_dumps(dump_path))
      else:
        dump = Dump.load(dump_path)
    symbol_mapping_cache = SymbolMappingCache()
    with open(prefix + '.cache.function', 'a+') as cache_f:
      symbol_mapping_cache.update(
          FUNCTION_SYMBOLS, bucket_set,
          SymbolFinder(FUNCTION_SYMBOLS, symbol_data_sources), cache_f)
    with open(prefix + '.cache.typeinfo', 'a+') as cache_f:
      symbol_mapping_cache.update(
          TYPEINFO_SYMBOLS, bucket_set,
          SymbolFinder(TYPEINFO_SYMBOLS, symbol_data_sources), cache_f)
    with open(prefix + '.cache.sourcefile', 'a+') as cache_f:
      symbol_mapping_cache.update(
          SOURCEFILE_SYMBOLS, bucket_set,
          SymbolFinder(SOURCEFILE_SYMBOLS, symbol_data_sources), cache_f)
    bucket_set.symbolize(symbol_mapping_cache)
    if no_dump:
      return bucket_set
    elif multiple:
      return (bucket_set, dump_list)
    else:
      return (bucket_set, dump)

  @staticmethod
  def _find_prefix(path):
    return re.sub('\.[0-9][0-9][0-9][0-9]\.heap', '', path)

  @staticmethod
  def _estimate_alternative_dirs(prefix):
    """Estimates a path in host from a corresponding path in target device.

    For Android, dmprof.py should find symbol information from binaries in
    the host instead of the Android device because dmprof.py doesn't run on
    the Android device.  This method estimates a path in the host
    corresponding to a path in the Android device.

    Returns:
        A dict that maps a path in the Android device to a path in the host.
        If a file in Command._DEVICE_LIB_BASEDIRS is found in /proc/maps, it
        assumes the process was running on Android and maps the path to
        "out/Debug/lib" in the Chromium directory.  An empty dict is returned
        unless Android.
    """
    device_lib_path_candidates = set()

    with open(prefix + '.maps') as maps_f:
      maps = proc_maps.ProcMaps.load(maps_f)
      for entry in maps:
        name = entry.as_dict()['name']
        if any([base_dir in name for base_dir in Command._DEVICE_LIB_BASEDIRS]):
          device_lib_path_candidates.add(os.path.dirname(name))

    if len(device_lib_path_candidates) == 1:
      return {device_lib_path_candidates.pop(): os.path.join(
                  CHROME_SRC_PATH, 'out', 'Debug', 'lib')}
    else:
      return {}

  @staticmethod
  def _find_all_dumps(dump_path):
    prefix = Command._find_prefix(dump_path)
    dump_path_list = [dump_path]

    n = int(dump_path[len(dump_path) - 9 : len(dump_path) - 5])
    n += 1
    skipped = 0
    while True:
      p = '%s.%04d.heap' % (prefix, n)
      if os.path.exists(p) and os.stat(p).st_size:
        dump_path_list.append(p)
      else:
        if skipped > 10:
          break
        skipped += 1
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
    if len(args) < required + 1:
      self._parser.error('needs %d argument(s).\n' % required)
      return None
    return (options, args)

  @staticmethod
  def _parse_policy_list(options_policy):
    if options_policy:
      return options_policy.split(',')
    else:
      return None


class BucketsCommand(Command):
  def __init__(self):
    super(BucketsCommand, self).__init__('Usage: %prog buckets <first-dump>')

  def do(self, sys_argv, out=sys.stdout):
    _, args = self._parse_args(sys_argv, 1)
    dump_path = args[1]
    bucket_set = Command.load_basic_files(dump_path, True, True)

    BucketsCommand._output(bucket_set, out)
    return 0

  @staticmethod
  def _output(bucket_set, out):
    """Prints all buckets with resolving symbols.

    Args:
        bucket_set: A BucketSet object.
        out: An IO object to output.
    """
    for bucket_id, bucket in sorted(bucket_set):
      out.write('%d: %s\n' % (bucket_id, bucket))


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
      for frame in bucket.symbolized_stackfunction:
        out.write(frame + ' ')
      out.write('\n')


class PolicyCommands(Command):
  def __init__(self, command):
    super(PolicyCommands, self).__init__(
        'Usage: %%prog %s [-p POLICY] <first-dump> [shared-first-dumps...]' %
        command)
    self._parser.add_option('-p', '--policy', type='string', dest='policy',
                            help='profile with POLICY', metavar='POLICY')
    self._parser.add_option('--alternative-dirs', dest='alternative_dirs',
                            metavar='/path/on/target@/path/on/host[:...]',
                            help='Read files in /path/on/host/ instead of '
                                 'files in /path/on/target/.')

  def _set_up(self, sys_argv):
    options, args = self._parse_args(sys_argv, 1)
    dump_path = args[1]
    shared_first_dump_paths = args[2:]
    alternative_dirs_dict = {}
    if options.alternative_dirs:
      for alternative_dir_pair in options.alternative_dirs.split(':'):
        target_path, host_path = alternative_dir_pair.split('@', 1)
        alternative_dirs_dict[target_path] = host_path
    (bucket_set, dumps) = Command.load_basic_files(
        dump_path, True, alternative_dirs=alternative_dirs_dict)

    pfn_counts_dict = dict()
    for shared_first_dump_path in shared_first_dump_paths:
      shared_dumps = Command._find_all_dumps(shared_first_dump_path)
      for shared_dump in shared_dumps:
        pfn_counts = PFNCounts.load(shared_dump)
        if pfn_counts.pid not in pfn_counts_dict:
          pfn_counts_dict[pfn_counts.pid] = list()
        pfn_counts_dict[pfn_counts.pid].append(pfn_counts)

    policy_set = PolicySet.load(Command._parse_policy_list(options.policy))
    return policy_set, dumps, pfn_counts_dict, bucket_set

  @staticmethod
  def _apply_policy(dump, pfn_counts_dict, policy, bucket_set, first_dump_time):
    """Aggregates the total memory size of each component.

    Iterate through all stacktraces and attribute them to one of the components
    based on the policy.  It is important to apply policy in right order.

    Args:
        dump: A Dump object.
        pfn_counts_dict: A dict mapping a pid to a list of PFNCounts.
        policy: A Policy object.
        bucket_set: A BucketSet object.
        first_dump_time: An integer representing time when the first dump is
            dumped.

    Returns:
        A dict mapping components and their corresponding sizes.
    """
    LOGGER.info('  %s' % dump.path)
    all_pfn_dict = dict()
    if pfn_counts_dict:
      LOGGER.info('    shared with...')
      for pid, pfnset_list in pfn_counts_dict.iteritems():
        closest_pfnset_index = None
        closest_pfnset_difference = 1024.0
        for index, pfnset in enumerate(pfnset_list):
          time_difference = pfnset.time - dump.time
          if time_difference >= 3.0:
            break
          elif ((time_difference < 0.0 and pfnset.reason != 'Exiting') or
                (0.0 <= time_difference and time_difference < 3.0)):
            closest_pfnset_index = index
            closest_pfnset_difference = time_difference
          elif time_difference < 0.0 and pfnset.reason == 'Exiting':
            closest_pfnset_index = None
            break
        if closest_pfnset_index:
          for pfn, count in pfnset_list[closest_pfnset_index].iter_pfn:
            all_pfn_dict[pfn] = all_pfn_dict.get(pfn, 0) + count
          LOGGER.info('      %s (time difference = %f)' %
                      (pfnset_list[closest_pfnset_index].path,
                       closest_pfnset_difference))
        else:
          LOGGER.info('      (no match with pid:%d)' % pid)

    sizes = dict((c, 0) for c in policy.components)

    PolicyCommands._accumulate_malloc(dump, policy, bucket_set, sizes)
    verify_global_stats = PolicyCommands._accumulate_maps(
        dump, all_pfn_dict, policy, bucket_set, sizes)

    # TODO(dmikurube): Remove the verifying code when GLOBAL_STATS is removed.
    # http://crbug.com/245603.
    for verify_key, verify_value in verify_global_stats.iteritems():
      dump_value = dump.global_stat('%s_committed' % verify_key)
      if dump_value != verify_value:
        LOGGER.warn('%25s: %12d != %d (%d)' % (
            verify_key, dump_value, verify_value, dump_value - verify_value))

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
    if sizes['tc-unused'] < 0:
      LOGGER.warn('    Assuming tc-unused=0 as it is negative: %d (bytes)' %
                  sizes['tc-unused'])
      sizes['tc-unused'] = 0
    sizes['tc-total'] = sizes['mmap-tcmalloc']

    # TODO(dmikurube): global_stat will be deprecated.
    # See http://crbug.com/245603.
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
  def _accumulate_malloc(dump, policy, bucket_set, sizes):
    for line in dump.iter_stacktrace:
      words = line.split()
      bucket = bucket_set.get(int(words[BUCKET_ID]))
      if not bucket or bucket.allocator_type == 'malloc':
        component_match = policy.find_malloc(bucket)
      elif bucket.allocator_type == 'mmap':
        continue
      else:
        assert False
      sizes[component_match] += int(words[COMMITTED])

      assert not component_match.startswith('mmap-')
      if component_match.startswith('tc-'):
        sizes['tc-total-log'] += int(words[COMMITTED])
      else:
        sizes['other-total-log'] += int(words[COMMITTED])

  @staticmethod
  def _accumulate_maps(dump, pfn_dict, policy, bucket_set, sizes):
    # TODO(dmikurube): Remove the dict when GLOBAL_STATS is removed.
    # http://crbug.com/245603.
    global_stats = {
        'total': 0,
        'file-exec': 0,
        'file-nonexec': 0,
        'anonymous': 0,
        'stack': 0,
        'other': 0,
        'nonprofiled-file-exec': 0,
        'nonprofiled-file-nonexec': 0,
        'nonprofiled-anonymous': 0,
        'nonprofiled-stack': 0,
        'nonprofiled-other': 0,
        'profiled-mmap': 0,
        }

    for key, value in dump.iter_map:
      # TODO(dmikurube): Remove the subtotal code when GLOBAL_STATS is removed.
      # It's temporary verification code for transition described in
      # http://crbug.com/245603.
      committed = 0
      if 'committed' in value[1]:
        committed = value[1]['committed']
      global_stats['total'] += committed
      key = 'other'
      name = value[1]['vma']['name']
      if name.startswith('/'):
        if value[1]['vma']['executable'] == 'x':
          key = 'file-exec'
        else:
          key = 'file-nonexec'
      elif name == '[stack]':
        key = 'stack'
      elif name == '':
        key = 'anonymous'
      global_stats[key] += committed
      if value[0] == 'unhooked':
        global_stats['nonprofiled-' + key] += committed
      if value[0] == 'hooked':
        global_stats['profiled-mmap'] += committed

      if value[0] == 'unhooked':
        if pfn_dict and dump.pageframe_length:
          for pageframe in value[1]['pageframe']:
            component_match = policy.find_unhooked(value, pageframe, pfn_dict)
            sizes[component_match] += pageframe.size
        else:
          component_match = policy.find_unhooked(value)
          sizes[component_match] += int(value[1]['committed'])
      elif value[0] == 'hooked':
        if pfn_dict and dump.pageframe_length:
          for pageframe in value[1]['pageframe']:
            component_match, _ = policy.find_mmap(
                value, bucket_set, pageframe, pfn_dict)
            sizes[component_match] += pageframe.size
            assert not component_match.startswith('tc-')
            if component_match.startswith('mmap-'):
              sizes['mmap-total-log'] += pageframe.size
            else:
              sizes['other-total-log'] += pageframe.size
        else:
          component_match, _ = policy.find_mmap(value, bucket_set)
          sizes[component_match] += int(value[1]['committed'])
          if component_match.startswith('mmap-'):
            sizes['mmap-total-log'] += int(value[1]['committed'])
          else:
            sizes['other-total-log'] += int(value[1]['committed'])
      else:
        LOGGER.error('Unrecognized mapping status: %s' % value[0])

    return global_stats


class CSVCommand(PolicyCommands):
  def __init__(self):
    super(CSVCommand, self).__init__('csv')

  def do(self, sys_argv):
    policy_set, dumps, pfn_counts_dict, bucket_set = self._set_up(sys_argv)
    return CSVCommand._output(
        policy_set, dumps, pfn_counts_dict, bucket_set, sys.stdout)

  @staticmethod
  def _output(policy_set, dumps, pfn_counts_dict, bucket_set, out):
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
            dump, pfn_counts_dict, policy_set[label], bucket_set, dumps[0].time)
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
    policy_set, dumps, pfn_counts_dict, bucket_set = self._set_up(sys_argv)
    return JSONCommand._output(
        policy_set, dumps, pfn_counts_dict, bucket_set, sys.stdout)

  @staticmethod
  def _output(policy_set, dumps, pfn_counts_dict, bucket_set, out):
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
            dump, pfn_counts_dict, policy_set[label], bucket_set, dumps[0].time)
        component_sizes['dump_path'] = dump.path
        component_sizes['dump_time'] = datetime.datetime.fromtimestamp(
            dump.time).strftime('%Y-%m-%d %H:%M:%S')
        json_base['policies'][label]['snapshots'].append(component_sizes)

      bucket_set.clear_component_cache()

    json.dump(json_base, out, indent=2, sort_keys=True)

    return 0


class ListCommand(PolicyCommands):
  def __init__(self):
    super(ListCommand, self).__init__('list')

  def do(self, sys_argv):
    policy_set, dumps, pfn_counts_dict, bucket_set = self._set_up(sys_argv)
    return ListCommand._output(
        policy_set, dumps, pfn_counts_dict, bucket_set, sys.stdout)

  @staticmethod
  def _output(policy_set, dumps, pfn_counts_dict, bucket_set, out):
    for label in sorted(policy_set):
      LOGGER.info('Applying a policy %s to...' % label)
      for dump in dumps:
        component_sizes = PolicyCommands._apply_policy(
            dump, pfn_counts_dict, policy_set[label], bucket_set, dump.time)
        out.write('%s for %s:\n' % (label, dump.path))
        for c in policy_set[label].components:
          if c in ['hour', 'minute', 'second']:
            out.write('%40s %12.3f\n' % (c, component_sizes[c]))
          else:
            out.write('%40s %12d\n' % (c, component_sizes[c]))

      bucket_set.clear_component_cache()

    return 0


class MapCommand(Command):
  def __init__(self):
    super(MapCommand, self).__init__('Usage: %prog map <first-dump> <policy>')

  def do(self, sys_argv, out=sys.stdout):
    _, args = self._parse_args(sys_argv, 2)
    dump_path = args[1]
    target_policy = args[2]
    (bucket_set, dumps) = Command.load_basic_files(dump_path, True)
    policy_set = PolicySet.load(Command._parse_policy_list(target_policy))

    MapCommand._output(dumps, bucket_set, policy_set[target_policy], out)
    return 0

  @staticmethod
  def _output(dumps, bucket_set, policy, out):
    """Prints all stacktraces in a given component of given depth.

    Args:
        dumps: A list of Dump objects.
        bucket_set: A BucketSet object.
        policy: A Policy object.
        out: An IO object to output.
    """
    max_dump_count = 0
    range_dict = ExclusiveRangeDict(ListAttribute)
    for dump in dumps:
      max_dump_count = max(max_dump_count, dump.count)
      for key, value in dump.iter_map:
        for begin, end, attr in range_dict.iter_range(key[0], key[1]):
          attr[dump.count] = value

    max_dump_count_digit = len(str(max_dump_count))
    for begin, end, attr in range_dict.iter_range():
      out.write('%x-%x\n' % (begin, end))
      if len(attr) < max_dump_count:
        attr[max_dump_count] = None
      for index, value in enumerate(attr[1:]):
        out.write('  #%0*d: ' % (max_dump_count_digit, index + 1))
        if not value:
          out.write('None\n')
        elif value[0] == 'hooked':
          component_match, _ = policy.find_mmap(value, bucket_set)
          out.write('hooked %s: %s @ %d\n' % (
              value[1]['type'] if 'type' in value[1] else 'None',
              component_match, value[1]['bucket_id']))
        else:
          region_info = value[1]
          size = region_info['committed']
          out.write('unhooked %s: %d bytes committed\n' % (
              region_info['type'] if 'type' in region_info else 'None', size))


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
    # TODO(dmikurube): Better formatting.
    for size_pair in sorted_sizes_list:
      out.write('%10d %s\n' % (size_pair[1], size_pair[0]))
      total += size_pair[1]
    LOGGER.info('total: %d\n' % total)

  @staticmethod
  def _add_size(precedence, bucket, depth, committed, sizes):
    stacktrace_sequence = precedence
    for function, sourcefile in zip(
        bucket.symbolized_stackfunction[
            0 : min(len(bucket.symbolized_stackfunction), 1 + depth)],
        bucket.symbolized_stacksourcefile[
            0 : min(len(bucket.symbolized_stacksourcefile), 1 + depth)]):
      stacktrace_sequence += '%s(@%s) ' % (function, sourcefile)
    if not stacktrace_sequence in sizes:
      sizes[stacktrace_sequence] = 0
    sizes[stacktrace_sequence] += committed

  @staticmethod
  def _accumulate(dump, policy, bucket_set, component_name, depth, sizes):
    rule = policy.find_rule(component_name)
    if not rule:
      pass
    elif rule.allocator_type == 'malloc':
      for line in dump.iter_stacktrace:
        words = line.split()
        bucket = bucket_set.get(int(words[BUCKET_ID]))
        if not bucket or bucket.allocator_type == 'malloc':
          component_match = policy.find_malloc(bucket)
        elif bucket.allocator_type == 'mmap':
          continue
        else:
          assert False
        if component_match == component_name:
          precedence = ''
          precedence += '(alloc=%d) ' % int(words[ALLOC_COUNT])
          precedence += '(free=%d) ' % int(words[FREE_COUNT])
          if bucket.typeinfo:
            precedence += '(type=%s) ' % bucket.symbolized_typeinfo
            precedence += '(type.name=%s) ' % bucket.typeinfo_name
          ExpandCommand._add_size(precedence, bucket, depth,
                                  int(words[COMMITTED]), sizes)
    elif rule.allocator_type == 'mmap':
      for _, region in dump.iter_map:
        if region[0] != 'hooked':
          continue
        component_match, bucket = policy.find_mmap(region, bucket_set)
        if component_match == component_name:
          ExpandCommand._add_size('', bucket, depth,
                                  region[1]['committed'], sizes)


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

    for _, region in dump.iter_map:
      if region[0] != 'hooked':
        continue
      component_match, bucket = policy.find_mmap(region, bucket_set)

      if (component_name and component_name != component_match) or (
          region[1]['committed'] == 0):
        continue

      com_committed += region[1]['committed']
      com_allocs += 1

    for line in dump.iter_stacktrace:
      words = line.split()
      bucket = bucket_set.get(int(words[BUCKET_ID]))
      if not bucket or bucket.allocator_type == 'malloc':
        component_match = policy.find_malloc(bucket)
      elif bucket.allocator_type == 'mmap':
        continue
      else:
        assert False
      if (not bucket or
          (component_name and component_name != component_match)):
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
    for _, region in dump.iter_map:
      if region[0] != 'hooked':
        continue
      component_match, bucket = policy.find_mmap(region, bucket_set)

      if (component_name and component_name != component_match) or (
          region[1]['committed'] == 0):
        continue

      out.write('     1: %8s [     1: %8s] @' % (
          region[1]['committed'], region[1]['committed']))
      for address in bucket.stacktrace:
        out.write(' 0x%016x' % address)
      out.write('\n')

    for line in dump.iter_stacktrace:
      words = line.split()
      bucket = bucket_set.get(int(words[BUCKET_ID]))
      if not bucket or bucket.allocator_type == 'malloc':
        component_match = policy.find_malloc(bucket)
      elif bucket.allocator_type == 'mmap':
        continue
      else:
        assert False
      if (not bucket or
          (component_name and component_name != component_match)):
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
    'buckets': BucketsCommand,
    'csv': CSVCommand,
    'expand': ExpandCommand,
    'json': JSONCommand,
    'list': ListCommand,
    'map': MapCommand,
    'pprof': PProfCommand,
    'stacktrace': StacktraceCommand,
    'upload': UploadCommand,
  }

  if len(sys.argv) < 2 or (not sys.argv[1] in COMMANDS):
    sys.stderr.write("""Usage: dmprof <command> [options] [<args>]

Commands:
   buckets      Dump a bucket list with resolving symbols
   csv          Classify memory usage in CSV
   expand       Show all stacktraces contained in the specified component
   json         Classify memory usage in JSON
   list         Classify memory usage in simple listing format
   map          Show history of mapped regions
   pprof        Format the profile dump so that it can be processed by pprof
   stacktrace   Convert runtime addresses to symbol names
   upload       Upload dumped files

Quick Reference:
   dmprof buckets <first-dump>
   dmprof csv [-p POLICY] <first-dump>
   dmprof expand <dump> <policy> <component> <depth>
   dmprof json [-p POLICY] <first-dump>
   dmprof list [-p POLICY] <first-dump>
   dmprof map <first-dump> <policy>
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
