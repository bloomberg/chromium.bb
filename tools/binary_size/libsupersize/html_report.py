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
import path_util


# Node dictionary keys. These are output in json read by the webapp so
# keep them short to save file size.
# Note: If these change, the webapp must also change.
_METHOD_COUNT_MODE_KEY = 'methodCountMode'
_NODE_TYPE_KEY = 'k'
_NODE_TYPE_BUCKET = 'b'
_NODE_TYPE_PATH = 'p'
_NODE_TYPE_SYMBOL = 's'
_NODE_NAME_KEY = 'n'
_NODE_CHILDREN_KEY = 'children'
_NODE_SYMBOL_TYPE_KEY = 't'
_NODE_SYMBOL_TYPE_VTABLE = 'v'
_NODE_SYMBOL_TYPE_GENERATED = '*'
_NODE_SYMBOL_SIZE_KEY = 'value'
_NODE_MAX_DEPTH_KEY = 'maxDepth'
_NODE_LAST_PATH_ELEMENT_KEY = 'lastPathElement'

_COMPACT_FILE_PATH_KEY = 'p'
_COMPACT_FILE_COMPONENT_INDEX_KEY = 'c'
_COMPACT_FILE_SYMBOLS_KEY = 's'
_COMPACT_SYMBOL_NAME_KEY = 'n'
_COMPACT_SYMBOL_BYTE_SIZE_KEY = 'b'
_COMPACT_SYMBOL_TYPE_KEY = 't'

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

# The display name of the bucket where we put symbols without path.
_NAME_SMALL_SYMBOL_BUCKET = '(Other)'
_NAME_NO_PATH_BUCKET = '(No Path)'

# Try to keep data buckets smaller than this to avoid killing the
# graphing lib.
_BIG_BUCKET_LIMIT = 3000

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


def _GetOrMakeChildNode(node, node_type, name):
  child = node[_NODE_CHILDREN_KEY].get(name)
  if child is None:
    child = {
        _NODE_TYPE_KEY: node_type,
        _NODE_NAME_KEY: name,
    }
    if node_type != _NODE_TYPE_SYMBOL:
      child[_NODE_CHILDREN_KEY] = {}
    node[_NODE_CHILDREN_KEY][name] = child
  else:
    assert child[_NODE_TYPE_KEY] == node_type
  return child


def _SplitLargeBucket(bucket):
  """Split the given node into sub-buckets when it's too big."""
  old_children = bucket[_NODE_CHILDREN_KEY]
  count = 0
  for symbol_type, symbol_bucket in old_children.iteritems():
    count += len(symbol_bucket[_NODE_CHILDREN_KEY])
  if count > _BIG_BUCKET_LIMIT:
    new_children = {}
    bucket[_NODE_CHILDREN_KEY] = new_children
    current_bucket = None
    index = 0
    for symbol_type, symbol_bucket in old_children.iteritems():
      for symbol_name, value in symbol_bucket[_NODE_CHILDREN_KEY].iteritems():
        if index % _BIG_BUCKET_LIMIT == 0:
          group_no = (index / _BIG_BUCKET_LIMIT) + 1
          node_name = '%s subgroup %d' % (_NAME_NO_PATH_BUCKET, group_no)
          current_bucket = _GetOrMakeChildNode(
              bucket, _NODE_TYPE_PATH, node_name)
        index += 1
        symbol_size = value[_NODE_SYMBOL_SIZE_KEY]
        _AddSymbolIntoFileNode(current_bucket, symbol_type, symbol_name,
                               symbol_size, True)


def _MakeChildrenDictsIntoLists(node):
  """Recursively converts all children from dicts -> lists."""
  children = node.get(_NODE_CHILDREN_KEY)
  if children:
    children = children.values()  # Convert dict -> list.
    node[_NODE_CHILDREN_KEY] = children
    for child in children:
      _MakeChildrenDictsIntoLists(child)
    if len(children) > _BIG_BUCKET_LIMIT:
      logging.warning('Bucket found with %d entries. Might be unusable.',
                      len(children))


def _CombineSingleChildNodes(node):
  """Collapse "java"->"com"->"google" into ."java/com/google"."""
  children = node.get(_NODE_CHILDREN_KEY)
  if children:
    child = children[0]
    if len(children) == 1 and node[_NODE_TYPE_KEY] == child[_NODE_TYPE_KEY]:
      node[_NODE_NAME_KEY] = '{}/{}'.format(
          node[_NODE_NAME_KEY], child[_NODE_NAME_KEY])
      node[_NODE_CHILDREN_KEY] = child[_NODE_CHILDREN_KEY]
      _CombineSingleChildNodes(node)
    else:
      for child in children:
        _CombineSingleChildNodes(child)


def _AddSymbolIntoFileNode(node, symbol_type, symbol_name, symbol_size,
                           min_symbol_size):
  """Puts symbol into the file path node |node|."""
  node[_NODE_LAST_PATH_ELEMENT_KEY] = True
  # Don't bother with buckets when not including symbols.
  if min_symbol_size == 0:
    node = _GetOrMakeChildNode(node, _NODE_TYPE_BUCKET, symbol_type)
    node[_NODE_SYMBOL_TYPE_KEY] = symbol_type

  # 'node' is now the symbol-type bucket. Make the child entry.
  if not symbol_name or symbol_size >= min_symbol_size:
    node_name = symbol_name or '[Anonymous]'
  elif symbol_name.startswith('*'):
    node_name = symbol_name
  else:
    node_name = symbol_type
  node = _GetOrMakeChildNode(node, _NODE_TYPE_SYMBOL, node_name)
  node[_NODE_SYMBOL_SIZE_KEY] = node.get(_NODE_SYMBOL_SIZE_KEY, 0) + symbol_size
  node[_NODE_SYMBOL_TYPE_KEY] = symbol_type


def _MakeCompactTree(symbols, min_symbol_size, method_count_mode):
  if method_count_mode:
    # Include all symbols and avoid bucket nodes.
    min_symbol_size = -1
  result = {
      _NODE_NAME_KEY: '/',
      _NODE_CHILDREN_KEY: {},
      _NODE_TYPE_KEY: 'p',
      _NODE_MAX_DEPTH_KEY: 0,
      _METHOD_COUNT_MODE_KEY: bool(method_count_mode),
  }
  for symbol in symbols:
    file_path = symbol.source_path or symbol.object_path or _NAME_NO_PATH_BUCKET
    node = result
    depth = 0
    for path_part in file_path.split(os.path.sep):
      if not path_part:
        continue
      depth += 1
      node = _GetOrMakeChildNode(node, _NODE_TYPE_PATH, path_part)

    symbol_type = _GetSymbolType(symbol)
    symbol_size = 1 if method_count_mode else symbol.pss
    _AddSymbolIntoFileNode(node, symbol_type, symbol.template_name, symbol_size,
                           min_symbol_size)
    depth += 2
    result[_NODE_MAX_DEPTH_KEY] = max(result[_NODE_MAX_DEPTH_KEY], depth)

  # The (no path) bucket can be extremely large if we failed to get
  # path information. Split it into subgroups if needed.
  no_path_bucket = result[_NODE_CHILDREN_KEY].get(_NAME_NO_PATH_BUCKET)
  if no_path_bucket and min_symbol_size == 0:
    _SplitLargeBucket(no_path_bucket)

  _MakeChildrenDictsIntoLists(result)
  _CombineSingleChildNodes(result)

  return result


def _GetSymbolType(symbol):
  symbol_type = symbol.section
  if symbol.name.endswith('[vtable]'):
    symbol_type = _NODE_SYMBOL_TYPE_VTABLE
  elif symbol.name.endswith(']'):
    symbol_type = _NODE_SYMBOL_TYPE_GENERATED
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
      symbol_type = 'o'

    total_size += symbol.pss
    symbol_size = round(symbol.pss, 2)
    if symbol_size.is_integer():
      symbol_size = int(symbol_size)

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
    if symbol_type == 'm' or abs(symbol_size) >= min_symbol_size:
      file_node[_COMPACT_FILE_SYMBOLS_KEY].append({
        _COMPACT_SYMBOL_NAME_KEY: symbol.template_name,
        _COMPACT_SYMBOL_TYPE_KEY: symbol_type,
        _COMPACT_SYMBOL_BYTE_SIZE_KEY: symbol_size,
      })
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


def _CopyTemplateFiles(template_src, dest_dir):
  d3_out = os.path.join(dest_dir, 'd3')
  if not os.path.exists(d3_out):
    os.makedirs(d3_out, 0755)
  d3_src = os.path.join(path_util.SRC_ROOT, 'third_party', 'd3', 'src')

  shutil.copy(os.path.join(template_src, 'index.html'), dest_dir)
  shutil.copy(os.path.join(d3_src, 'LICENSE'), d3_out)
  shutil.copy(os.path.join(d3_src, 'd3.js'), d3_out)
  shutil.copy(os.path.join(template_src, 'D3SymbolTreeMap.js'), dest_dir)


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
  parser.add_argument('--include-bss', action='store_true',
                      help='Include symbols from .bss (which consume no real '
                           'space)')
  parser.add_argument('--min-symbol-size', type=float, default=1024,
                      help='Minimum size (PSS) for a symbol to be included as '
                           'an independent node.')
  parser.add_argument('--method-count', action='store_true',
                      help='Show dex method count rather than size')
  parser.add_argument('--tree-view-ui', action='store_true',
                      help='Use the new tree view UI')
  parser.add_argument('--diff-with',
                      help='Diffs the input_file against an older .size file')


def Run(args, parser):
  if not args.input_file.endswith('.size'):
    parser.error('Input must end with ".size"')
  if args.diff_with and not args.diff_with.endswith('.size'):
    parser.error('Diff input must end with ".size"')
  elif args.diff_with and not args.tree_view_ui:
    parser.error('Diffs only supported in --tree-view-ui mode')
  if args.tree_view_ui and args.method_count:
    parser.error('--method-count is no longer supported as a command line '
                 'flag, use the client-side options instead.')

  logging.info('Reading .size file')
  size_info = archive.LoadAndPostProcessSizeInfo(args.input_file)
  if args.diff_with:
    before_size_info = archive.LoadAndPostProcessSizeInfo(args.diff_with)
    after_size_info = size_info
    size_info = diff.Diff(before_size_info, after_size_info)
  symbols = size_info.raw_symbols
  if args.method_count:
    symbols = symbols.WhereInSection('m')
  elif not args.include_bss:
    symbols = symbols.WhereInSection('b').Inverted()

  if args.tree_view_ui:
    template_src = os.path.join(os.path.dirname(__file__), 'template_tree_view')
    _CopyTreeViewTemplateFiles(template_src, args.report_dir)
    logging.info('Creating JSON objects')
    meta, tree_nodes = _MakeTreeViewList(symbols, args.min_symbol_size)
    meta['diff_mode'] = args.diff_with

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
  else:
    # Copy report boilerplate into output directory. This also proves that the
    # output directory is safe for writing, so there should be no problems
    # writing the nm.out file later.
    template_src = os.path.join(os.path.dirname(__file__), 'template')
    _CopyTemplateFiles(template_src, args.report_dir)
    logging.info('Creating JSON objects')
    tree_root = _MakeCompactTree(symbols, args.min_symbol_size,
                                 args.method_count)

    logging.info('Serializing JSON')
    with open(os.path.join(args.report_dir, 'data.js'), 'w') as out_file:
      out_file.write('var tree_data=')
      # Use separators without whitespace to get a smaller file.
      json.dump(tree_root, out_file, ensure_ascii=False, check_circular=False,
                separators=(',', ':'))

  logging.warning('Report saved to %s/index.html', args.report_dir)
