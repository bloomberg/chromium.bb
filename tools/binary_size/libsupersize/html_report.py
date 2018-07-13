# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Creates an html report that allows you to view binary size by component."""

import codecs
import collections
import json
import logging
import os
import shutil

import archive
import diff
import models


_SYMBOL_TYPE_VTABLE = 'v'
_SYMBOL_TYPE_GENERATED = '*'
_SYMBOL_TYPE_DEX_METHOD = 'm'
_SYMBOL_TYPE_OTHER = 'o'

_COMPACT_FILE_PATH_KEY = 'p'
_COMPACT_FILE_COMPONENT_INDEX_KEY = 'c'
_COMPACT_FILE_SYMBOLS_KEY = 's'
_COMPACT_SYMBOL_NAME_KEY = 'n'
_COMPACT_SYMBOL_BYTE_SIZE_KEY = 'b'
_COMPACT_SYMBOL_TYPE_KEY = 't'
_COMPACT_SYMBOL_COUNT_KEY = 'u'

_SMALL_SYMBOL_DESCRIPTIONS = {
  'b': 'Other small uninitialized data',
  'd': 'Other small initialized data',
  'r': 'Other small readonly data',
  't': 'Other small code',
  'v': 'Other small vtable entries',
  '*': 'Other small generated symbols',
  'x': 'Other small dex non-method entries',
  'm': 'Other small dex methods',
  'p': 'Other small locale pak entries',
  'P': 'Other small non-locale pak entries',
  'o': 'Other small entries',
}

_TEMPLATE_FILES = [
  'index.html',
  'favicon.ico',
  'options.css',
  'infocard.css',
  'start-worker.js',
  'shared.js',
  'state.js',
  'infocard-ui.js',
  'tree-ui.js',
  'tree-worker.js',
]


def _GetSymbolType(symbol):
  symbol_type = symbol.section
  if symbol.name.endswith('[vtable]'):
    symbol_type = _SYMBOL_TYPE_VTABLE
  elif symbol.name.endswith(']'):
    symbol_type = _SYMBOL_TYPE_GENERATED
  return symbol_type


class IndexedSet(object):
  """Set-like object where values are unique and indexed.

  Values must be immutable.
  """

  def __init__(self):
    self._index_dict = {}  # Value -> Index dict
    self.value_list = []  # List containing all the set items

  def GetOrAdd(self, value):
    """Get the index of the value in the list. Append it if not yet present."""
    index = self._index_dict.get(value)
    if index is None:
      self.value_list.append(value)
      index = len(self.value_list) - 1
      self._index_dict[value] = index
    return index


def _MakeTreeViewList(symbols, min_symbol_size):
  """Builds JSON data of the symbols for the tree view HTML report.

  As the tree is built on the client-side, this function creates a flat list
  of files, where each file object contains symbols that have the same path.

  Args:
    symbols: A SymbolGroup containing all symbols.
    min_symbol_size: The minimum byte size needed for a symbol to be included.
  """
  file_nodes = {}
  components = IndexedSet()
  total_size = 0

  # Build a container for symbols smaller than min_symbol_size
  small_symbols = collections.defaultdict(dict)

  # Bundle symbols by the file they belong to,
  # and add all the file buckets into file_nodes
  for symbol in symbols:
    symbol_type = _GetSymbolType(symbol)
    if symbol_type not in _SMALL_SYMBOL_DESCRIPTIONS:
      symbol_type = _SYMBOL_TYPE_OTHER

    total_size += symbol.pss
    symbol_size = round(symbol.pss, 2)
    if symbol_size.is_integer():
      symbol_size = int(symbol_size)
    symbol_count = 1
    if symbol.IsDelta() and symbol.diff_status == models.DIFF_STATUS_REMOVED:
      symbol_count = -1

    path = symbol.source_path or symbol.object_path
    file_node = file_nodes.get(path)
    if file_node is None:
      component_index = components.GetOrAdd(symbol.component)
      file_node = {
        _COMPACT_FILE_PATH_KEY: path,
        _COMPACT_FILE_COMPONENT_INDEX_KEY: component_index,
        _COMPACT_FILE_SYMBOLS_KEY: [],
      }
      file_nodes[path] = file_node

    # Dex methods (type "m") are whitelisted for the method_count mode on the
    # UI. It's important to see details on all the methods, and most fall below
    # the default byte size.
    is_dex_method = symbol_type == _SYMBOL_TYPE_DEX_METHOD
    if is_dex_method or abs(symbol_size) >= min_symbol_size:
      symbol_entry = {
        _COMPACT_SYMBOL_NAME_KEY: symbol.template_name,
        _COMPACT_SYMBOL_TYPE_KEY: symbol_type,
        _COMPACT_SYMBOL_BYTE_SIZE_KEY: symbol_size,
      }
      # We use symbol count for the method count mode in the diff mode report.
      # Negative values are used to indicate a symbol was removed, so it should
      # count as -1 rather than the default, 1.
      # We don't care about accurate counts for other symbol types currently,
      # so this data is only included for methods.
      if is_dex_method and symbol_count != 1:
        symbol_entry[_COMPACT_SYMBOL_COUNT_KEY] = symbol_count
      file_node[_COMPACT_FILE_SYMBOLS_KEY].append(symbol_entry)
    else:
      small_type_symbol = small_symbols[path].get(symbol_type)
      if small_type_symbol is None:
        small_type_symbol = {
          _COMPACT_SYMBOL_NAME_KEY: _SMALL_SYMBOL_DESCRIPTIONS[symbol_type],
          _COMPACT_SYMBOL_TYPE_KEY: symbol_type,
          _COMPACT_SYMBOL_BYTE_SIZE_KEY: 0,
        }
        small_symbols[path][symbol_type] = small_type_symbol
        file_node[_COMPACT_FILE_SYMBOLS_KEY].append(small_type_symbol)

      small_type_symbol[_COMPACT_SYMBOL_BYTE_SIZE_KEY] += symbol_size

  meta = {
    'components': components.value_list,
    'total': total_size,
  }
  return meta, file_nodes.values()


def _MakeDirIfDoesNotExist(rel_path):
  """Ensures a directory exists."""
  abs_path = os.path.abspath(rel_path)
  try:
    os.makedirs(abs_path)
  except OSError:
    if not os.path.isdir(abs_path):
      raise


def _CopyTreeViewTemplateFiles(template_src, dest_dir):
  """Copy and format template files for the tree view UI.

  The index.html file uses basic mustache syntax to denote where strings
  should be replaced. Only variable tags are supported.

  Args:
    template_src: Path to the directory containing the template files.
    dest_dir: Path to the directory where the outputted files will be saved.
    kwags: Dict of key-value pairs which will be used to replace {{<key>}}
      strings in the index.html template.

  Throws:
    KeyError: thrown if a variable tag does not have a corresponding kwarg.
  """
  _MakeDirIfDoesNotExist(dest_dir)
  for path in _TEMPLATE_FILES:
    shutil.copy(os.path.join(template_src, path), dest_dir)


def AddArguments(parser):
  parser.add_argument('input_file',
                      help='Path to input .size file.')
  parser.add_argument('--report-dir', metavar='PATH', required=True,
                      help='Write output to the specified directory. An HTML '
                            'report is generated here.')
  parser.add_argument('--min-symbol-size', type=float, default=1024,
                      help='Minimum size (PSS) for a symbol to be included as '
                           'an independent node.')
  parser.add_argument('--diff-with',
                      help='Diffs the input_file against an older .size file')


def Run(args, parser):
  if not args.input_file.endswith('.size'):
    parser.error('Input must end with ".size"')
  if args.diff_with and not args.diff_with.endswith('.size'):
    parser.error('Diff input must end with ".size"')

  logging.info('Reading .size file')
  size_info = archive.LoadAndPostProcessSizeInfo(args.input_file)
  if args.diff_with:
    before_size_info = archive.LoadAndPostProcessSizeInfo(args.diff_with)
    after_size_info = size_info
    size_info = diff.Diff(before_size_info, after_size_info)
  symbols = size_info.raw_symbols

  template_src = os.path.join(os.path.dirname(__file__), 'template_tree_view')
  _CopyTreeViewTemplateFiles(template_src, args.report_dir)
  logging.info('Creating JSON objects')
  meta, tree_nodes = _MakeTreeViewList(symbols, args.min_symbol_size)
  meta.update({
    'diff_mode': bool(args.diff_with),
    'section_sizes': size_info.section_sizes,
  })
  if args.diff_with:
    meta.update({
      'before_metadata': size_info.before.metadata,
      'after_metadata': size_info.after.metadata,
    })
  else:
    meta['metadata'] = size_info.metadata

  logging.info('Serializing JSON')
  # Write newline-delimited JSON file
  data_file_path = os.path.join(args.report_dir, 'data.ndjson')
  with codecs.open(data_file_path, 'w', encoding='ascii') as out_file:
    # Use separators without whitespace to get a smaller file.
    json.dump(meta, out_file, ensure_ascii=False, check_circular=False,
              separators=(',', ':'))
    out_file.write('\n')

    for tree_node in tree_nodes:
      json.dump(tree_node, out_file, ensure_ascii=False, check_circular=False,
                separators=(',', ':'))
      out_file.write('\n')

  logging.warning('Report saved to %s/index.html', args.report_dir)
