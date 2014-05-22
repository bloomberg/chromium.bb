#!/usr/bin/env python
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Generate a spatial analysis against an arbitrary library.

To use, build the 'binary_size_tool' target. Then run this tool, passing
in the location of the library to be analyzed along with any other options
you desire.
"""

import collections
import json
import logging
import multiprocessing
import optparse
import os
import re
import shutil
import subprocess
import sys
import tempfile
import time

import binary_size_utils

# This path changee is not beautiful. Temporary (I hope) measure until
# the chromium project has figured out a proper way to organize the
# library of python tools. http://crbug.com/375725
elf_symbolizer_path = os.path.abspath(os.path.join(
    os.path.dirname(__file__),
    '..',
    '..',
    'build',
    'android',
    'pylib'))
sys.path.append(elf_symbolizer_path)
import symbols.elf_symbolizer as elf_symbolizer


# TODO(andrewhayden): Only used for legacy reports. Delete.
def FormatBytes(byte_count):
  """Pretty-print a number of bytes."""
  if byte_count > 1e6:
    byte_count = byte_count / 1.0e6
    return '%.1fm' % byte_count
  if byte_count > 1e3:
    byte_count = byte_count / 1.0e3
    return '%.1fk' % byte_count
  return str(byte_count)


# TODO(andrewhayden): Only used for legacy reports. Delete.
def SymbolTypeToHuman(symbol_type):
  """Convert a symbol type as printed by nm into a human-readable name."""
  return {'b': 'bss',
          'd': 'data',
          'r': 'read-only data',
          't': 'code',
          'w': 'weak symbol',
          'v': 'weak symbol'}[symbol_type]


def _MkChild(node, name):
  child = node['children'].get(name)
  if child is None:
    child = {'n': name, 'children': {}}
    node['children'][name] = child
  return child


def MakeChildrenDictsIntoLists(node):
  largest_list_len = 0
  if 'children' in node:
    largest_list_len = len(node['children'])
    child_list = []
    for child in node['children'].itervalues():
      child_largest_list_len = MakeChildrenDictsIntoLists(child)
      if child_largest_list_len > largest_list_len:
        largest_list_len = child_largest_list_len
      child_list.append(child)
    node['children'] = child_list

  return largest_list_len


def MakeCompactTree(symbols):
  result = {'n': '/', 'children': {}, 'k': 'p', 'maxDepth': 0}
  seen_symbol_with_path = False
  for symbol_name, symbol_type, symbol_size, file_path in symbols:

    if 'vtable for ' in symbol_name:
      symbol_type = '@' # hack to categorize these separately
    # Take path like '/foo/bar/baz', convert to ['foo', 'bar', 'baz']
    if file_path:
      file_path = os.path.normpath(file_path)
      seen_symbol_with_path = True
    else:
      file_path = '(No Path)'

    if file_path.startswith('/'):
      file_path = file_path[1:]
    path_parts = file_path.split('/')

    # Find pre-existing node in tree, or update if it already exists
    node = result
    depth = 0
    while len(path_parts) > 0:
      path_part = path_parts.pop(0)
      if len(path_part) == 0:
        continue
      depth += 1
      node = _MkChild(node, path_part)
      assert not 'k' in node or node['k'] == 'p'
      node['k'] = 'p' # p for path

    # 'node' is now the file node. Find the symbol-type bucket.
    node['lastPathElement'] = True
    node = _MkChild(node, symbol_type)
    assert not 'k' in node or node['k'] == 'b'
    node['t'] = symbol_type
    node['k'] = 'b' # b for bucket
    depth += 1

    # 'node' is now the symbol-type bucket. Make the child entry.
    node = _MkChild(node, symbol_name)
    if 'children' in node:
      if node['children']:
        logging.warning('A container node used as symbol for %s.' % symbol_name)
      # This is going to be used as a leaf so no use for child list.
      del node['children']
    node['value'] = symbol_size
    node['t'] = symbol_type
    node['k'] = 's' # s for symbol
    depth += 1
    result['maxDepth'] = max(result['maxDepth'], depth)

  if not seen_symbol_with_path:
    logging.warning('Symbols lack paths. Data will not be structured.')

  largest_list_len = MakeChildrenDictsIntoLists(result)

  if largest_list_len > 1000:
    logging.warning('There are sections with %d nodes. '
                    'Results might be unusable.' % largest_list_len)
  return result


# TODO(andrewhayden): Only used for legacy reports. Delete.
def TreeifySymbols(symbols):
  """Convert symbols into a path-based tree, calculating size information
  along the way.

  The result is a dictionary that contains two kinds of nodes:
  1. Leaf nodes, representing source code locations (e.g., c++ files)
     These nodes have the following dictionary entries:
       sizes: a dictionary whose keys are categories (such as code, data,
              vtable, etceteras) and whose values are the size, in bytes, of
              those categories;
       size:  the total size, in bytes, of all the entries in the sizes dict
  2. Non-leaf nodes, representing directories
     These nodes have the following dictionary entries:
       children: a dictionary whose keys are names (path entries; either
                 directory or file names) and whose values are other nodes;
       size:     the total size, in bytes, of all the leaf nodes that are
                 contained within the children dict (recursively expanded)

  The result object is itself a dictionary that represents the common ancestor
  of all child nodes, e.g. a path to which all other nodes beneath it are
  relative. The 'size' attribute of this dict yields the sum of the size of all
  leaf nodes within the data structure.
  """
  dirs = {'children': {}, 'size': 0}
  for sym, symbol_type, size, path in symbols:
    dirs['size'] += size
    if path:
      path = os.path.normpath(path)
      if path.startswith('/'):
        path = path[1:]

    parts = None
    if path:
      parts = path.split('/')

    if parts:
      assert path
      file_key = parts.pop()
      tree = dirs
      try:
        # Traverse the tree to the parent of the file node, creating as needed
        for part in parts:
          assert part != ''
          if part not in tree['children']:
            tree['children'][part] = {'children': {}, 'size': 0}
          tree = tree['children'][part]
          tree['size'] += size

        # Get (creating if necessary) the node for the file
        # This node doesn't have a 'children' attribute
        if file_key not in tree['children']:
          tree['children'][file_key] = {'sizes': collections.defaultdict(int),
                                        'size': 0}
        tree = tree['children'][file_key]
        tree['size'] += size

        # Accumulate size into a bucket within the file
        symbol_type = symbol_type.lower()
        if 'vtable for ' in sym:
          tree['sizes']['[vtable]'] += size
        elif 'r' == symbol_type:
          tree['sizes']['[rodata]'] += size
        elif 'd' == symbol_type:
          tree['sizes']['[data]'] += size
        elif 'b' == symbol_type:
          tree['sizes']['[bss]'] += size
        elif 't' == symbol_type:
          # 'text' in binary parlance means 'code'.
          tree['sizes']['[code]'] += size
        elif 'w' == symbol_type:
          tree['sizes']['[weak]'] += size
        else:
          tree['sizes']['[other]'] += size
      except:
        print >> sys.stderr, sym, parts, file_key
        raise
    else:
      key = 'symbols without paths'
      if key not in dirs['children']:
        dirs['children'][key] = {'sizes': collections.defaultdict(int),
                                 'size': 0}
      tree = dirs['children'][key]
      subkey = 'misc'
      if (sym.endswith('::__FUNCTION__') or
        sym.endswith('::__PRETTY_FUNCTION__')):
        subkey = '__FUNCTION__'
      elif sym.startswith('CSWTCH.'):
        subkey = 'CSWTCH'
      elif '::' in sym:
        subkey = sym[0:sym.find('::') + 2]
      tree['sizes'][subkey] = tree['sizes'].get(subkey, 0) + size
      tree['size'] += size
  return dirs


# TODO(andrewhayden): Only used for legacy reports. Delete.
def JsonifyTree(tree, name):
  """Convert TreeifySymbols output to a JSON treemap.

  The format is very similar, with the notable exceptions being
  lists of children instead of maps and some different attribute names."""
  children = []
  css_class_map = {
                  '[vtable]': 'vtable',
                  '[rodata]': 'read-only_data',
                  '[data]': 'data',
                  '[bss]': 'bss',
                  '[code]': 'code',
                  '[weak]': 'weak_symbol'
  }
  if 'children' in tree:
    # Non-leaf node. Recurse.
    for child_name, child in tree['children'].iteritems():
      children.append(JsonifyTree(child, child_name))
  else:
    # Leaf node; dump per-file stats as entries in the treemap
    for kind, size in tree['sizes'].iteritems():
      child_json = {'name': kind + ' (' + FormatBytes(size) + ')',
                   'data': { '$area': size }}
      css_class = css_class_map.get(kind)
      if css_class is not None:
        child_json['data']['$symbol'] = css_class
      children.append(child_json)
  # Sort children by size, largest to smallest.
  children.sort(key=lambda child: -child['data']['$area'])

  # For leaf nodes, the 'size' attribute is the size of the leaf;
  # Non-leaf nodes don't really have a size, but their 'size' attribute is
  # the sum of the sizes of all their children.
  return {'name': name + ' (' + FormatBytes(tree['size']) + ')',
          'data': { '$area': tree['size'] },
          'children': children }

def DumpCompactTree(symbols, outfile):
  tree_root = MakeCompactTree(symbols)
  with open(outfile, 'w') as out:
    out.write('var tree_data = ')
    json.dump(tree_root, out)
  print('Writing %d bytes json' % os.path.getsize(outfile))


# TODO(andrewhayden): Only used for legacy reports. Delete.
def DumpTreemap(symbols, outfile):
  dirs = TreeifySymbols(symbols)
  out = open(outfile, 'w')
  try:
    out.write('var kTree = ' + json.dumps(JsonifyTree(dirs, '/')))
  finally:
    out.flush()
    out.close()


# TODO(andrewhayden): Only used for legacy reports. Delete.
def DumpLargestSymbols(symbols, outfile, n):
  # a list of (sym, symbol_type, size, path); sort by size.
  symbols = sorted(symbols, key=lambda x: -x[2])
  dumped = 0
  out = open(outfile, 'w')
  try:
    out.write('var largestSymbols = [\n')
    for sym, symbol_type, size, path in symbols:
      if symbol_type in ('b', 'w'):
        continue  # skip bss and weak symbols
      if path is None:
        path = ''
      entry = {'size': FormatBytes(size),
               'symbol': sym,
               'type': SymbolTypeToHuman(symbol_type),
               'location': path }
      out.write(json.dumps(entry))
      out.write(',\n')
      dumped += 1
      if dumped >= n:
        return
  finally:
    out.write('];\n')
    out.flush()
    out.close()


def MakeSourceMap(symbols):
  sources = {}
  for _sym, _symbol_type, size, path in symbols:
    key = None
    if path:
      key = os.path.normpath(path)
    else:
      key = '[no path]'
    if key not in sources:
      sources[key] = {'path': path, 'symbol_count': 0, 'size': 0}
    record = sources[key]
    record['size'] += size
    record['symbol_count'] += 1
  return sources


# TODO(andrewhayden): Only used for legacy reports. Delete.
def DumpLargestSources(symbols, outfile, n):
  source_map = MakeSourceMap(symbols)
  sources = sorted(source_map.values(), key=lambda x: -x['size'])
  dumped = 0
  out = open(outfile, 'w')
  try:
    out.write('var largestSources = [\n')
    for record in sources:
      entry = {'size': FormatBytes(record['size']),
               'symbol_count': str(record['symbol_count']),
               'location': record['path']}
      out.write(json.dumps(entry))
      out.write(',\n')
      dumped += 1
      if dumped >= n:
        return
  finally:
    out.write('];\n')
    out.flush()
    out.close()


# TODO(andrewhayden): Only used for legacy reports. Delete.
def DumpLargestVTables(symbols, outfile, n):
  vtables = []
  for symbol, _type, size, path in symbols:
    if 'vtable for ' in symbol:
      vtables.append({'symbol': symbol, 'path': path, 'size': size})
  vtables = sorted(vtables, key=lambda x: -x['size'])
  dumped = 0
  out = open(outfile, 'w')
  try:
    out.write('var largestVTables = [\n')
    for record in vtables:
      entry = {'size': FormatBytes(record['size']),
               'symbol': record['symbol'],
               'location': record['path']}
      out.write(json.dumps(entry))
      out.write(',\n')
      dumped += 1
      if dumped >= n:
        return
  finally:
    out.write('];\n')
    out.flush()
    out.close()


# Regex for parsing "nm" output. A sample line looks like this:
# 0167b39c 00000018 t ACCESS_DESCRIPTION_free /path/file.c:95
#
# The fields are: address, size, type, name, source location
# Regular expression explained ( see also: https://xkcd.com/208 ):
# ([0-9a-f]{8,}+)   The address
# [\s]+             Whitespace separator
# ([0-9a-f]{8,}+)   The size. From here on out it's all optional.
# [\s]+             Whitespace separator
# (\S?)             The symbol type, which is any non-whitespace char
# [\s*]             Whitespace separator
# ([^\t]*)          Symbol name, any non-tab character (spaces ok!)
# [\t]?             Tab separator
# (.*)              The location (filename[:linennum|?][ (discriminator n)]
sNmPattern = re.compile(
  r'([0-9a-f]{8,})[\s]+([0-9a-f]{8,})[\s]*(\S?)[\s*]([^\t]*)[\t]?(.*)')

class Progress():
  def __init__(self):
    self.count = 0
    self.skip_count = 0
    self.collisions = 0
    self.time_last_output = time.time()
    self.count_last_output = 0


def RunElfSymbolizer(outfile, library, addr2line_binary, nm_binary, jobs):
  nm_output = RunNm(library, nm_binary)
  nm_output_lines = nm_output.splitlines()
  nm_output_lines_len = len(nm_output_lines)
  address_symbol = {}
  progress = Progress()
  def map_address_symbol(symbol, addr):
    progress.count += 1
    if addr in address_symbol:
      # 'Collision between %s and %s.' % (str(symbol.name),
      #                                   str(address_symbol[addr].name))
      progress.collisions += 1
    else:
      address_symbol[addr] = symbol

    progress_chunk = 100
    if progress.count % progress_chunk == 0:
      time_now = time.time()
      time_spent = time_now - progress.time_last_output
      if time_spent > 1.0:
        # Only output at most once per second.
        progress.time_last_output = time_now
        chunk_size = progress.count - progress.count_last_output
        progress.count_last_output = progress.count
        if time_spent > 0:
          speed = chunk_size / time_spent
        else:
          speed = 0
        progress_percent = (100.0 * (progress.count + progress.skip_count) /
                            nm_output_lines_len)
        print('%.1f%%: Looked up %d symbols (%d collisions) - %.1f lookups/s.' %
              (progress_percent, progress.count, progress.collisions, speed))

  symbolizer = elf_symbolizer.ELFSymbolizer(library, addr2line_binary,
                                            map_address_symbol,
                                            max_concurrent_jobs=jobs)
  for line in nm_output_lines:
    match = sNmPattern.match(line)
    if match:
      location = match.group(5)
      if not location:
        addr = int(match.group(1), 16)
        size = int(match.group(2), 16)
        if addr in address_symbol:  # Already looked up, shortcut ELFSymbolizer.
          map_address_symbol(address_symbol[addr], addr)
          continue
        elif size == 0:
          # Save time by not looking up empty symbols (do they even exist?)
          print('Empty symbol: ' + line)
        else:
          symbolizer.SymbolizeAsync(addr, addr)
          continue

    progress.skip_count += 1

  symbolizer.Join()

  with open(outfile, 'w') as out:
    for line in nm_output_lines:
      match = sNmPattern.match(line)
      if match:
        location = match.group(5)
        if not location:
          addr = int(match.group(1), 16)
          symbol = address_symbol[addr]
          path = '??'
          if symbol.source_path is not None:
            path = symbol.source_path
          line_number = 0
          if symbol.source_line is not None:
            line_number = symbol.source_line
          out.write('%s\t%s:%d\n' % (line, path, line_number))
          continue

      out.write('%s\n' % line)

  print('%d symbols in the results.' % len(address_symbol))


def RunNm(binary, nm_binary):
  print('Starting nm')
  cmd = [nm_binary, '-C', '--print-size', binary]
  nm_process = subprocess.Popen(cmd,
                                stdout=subprocess.PIPE,
                                stderr=subprocess.PIPE)
  (process_output, err_output) = nm_process.communicate()

  if nm_process.returncode != 0:
    if err_output:
      raise Exception, err_output
    else:
      raise Exception, process_output

  print('Finished nm')
  return process_output


def GetNmSymbols(nm_infile, outfile, library, jobs, verbose,
                 addr2line_binary, nm_binary):
  if nm_infile is None:
    if outfile is None:
      outfile = tempfile.NamedTemporaryFile(delete=False).name

    if verbose:
      print 'Running parallel addr2line, dumping symbols to ' + outfile
    RunElfSymbolizer(outfile, library, addr2line_binary, nm_binary, jobs)

    nm_infile = outfile

  elif verbose:
    print 'Using nm input from ' + nm_infile
  with file(nm_infile, 'r') as infile:
    return list(binary_size_utils.ParseNm(infile))


def _find_in_system_path(binary):
  """Locate the full path to binary in the system path or return None
  if not found."""
  system_path = os.environ["PATH"].split(os.pathsep)
  for path in system_path:
    binary_path = os.path.join(path, binary)
    if os.path.isfile(binary_path):
      return binary_path
  return None


def main():
  usage = """%prog [options]

  Runs a spatial analysis on a given library, looking up the source locations
  of its symbols and calculating how much space each directory, source file,
  and so on is taking. The result is a report that can be used to pinpoint
  sources of large portions of the binary, etceteras.

  Under normal circumstances, you only need to pass two arguments, thusly:

      %prog --library /path/to/library --destdir /path/to/output

  In this mode, the program will dump the symbols from the specified library
  and map those symbols back to source locations, producing a web-based
  report in the specified output directory.

  Other options are available via '--help'.
  """
  parser = optparse.OptionParser(usage=usage)
  parser.add_option('--nm-in', metavar='PATH',
                    help='if specified, use nm input from <path> instead of '
                    'generating it. Note that source locations should be '
                    'present in the file; i.e., no addr2line symbol lookups '
                    'will be performed when this option is specified. '
                    'Mutually exclusive with --library.')
  parser.add_option('--destdir', metavar='PATH',
                    help='write output to the specified directory. An HTML '
                    'report is generated here along with supporting files; '
                    'any existing report will be overwritten.')
  parser.add_option('--library', metavar='PATH',
                    help='if specified, process symbols in the library at '
                    'the specified path. Mutually exclusive with --nm-in.')
  parser.add_option('--nm-binary',
                    help='use the specified nm binary to analyze library. '
                    'This is to be used when the nm in the path is not for '
                    'the right architecture or of the right version.')
  parser.add_option('--addr2line-binary',
                    help='use the specified addr2line binary to analyze '
                    'library. This is to be used when the addr2line in '
                    'the path is not for the right architecture or '
                    'of the right version.')
  parser.add_option('--jobs',
                    help='number of jobs to use for the parallel '
                    'addr2line processing pool; defaults to 1. More '
                    'jobs greatly improve throughput but eat RAM like '
                    'popcorn, and take several gigabytes each. Start low '
                    'and ramp this number up until your machine begins to '
                    'struggle with RAM. '
                    'This argument is only valid when using --library.')
  parser.add_option('-v', dest='verbose', action='store_true',
                    help='be verbose, printing lots of status information.')
  parser.add_option('--nm-out', metavar='PATH',
                    help='keep the nm output file, and store it at the '
                    'specified path. This is useful if you want to see the '
                    'fully processed nm output after the symbols have been '
                    'mapped to source locations. By default, a tempfile is '
                    'used and is deleted when the program terminates.'
                    'This argument is only valid when using --library.')
  parser.add_option('--legacy', action='store_true',
                    help='emit legacy binary size report instead of modern')
  opts, _args = parser.parse_args()

  if ((not opts.library) and (not opts.nm_in)) or (opts.library and opts.nm_in):
    parser.error('exactly one of --library or --nm-in is required')
  if (opts.nm_in):
    if opts.jobs:
      print >> sys.stderr, ('WARNING: --jobs has no effect '
                            'when used with --nm-in')
  if not opts.destdir:
    parser.error('--destdir is required argument')
  if not opts.jobs:
    # Use the number of processors but cap between 2 and 4 since raw
    # CPU power isn't the limiting factor. It's I/O limited, memory
    # bus limited and available-memory-limited. Too many processes and
    # the computer will run out of memory and it will be slow.
    opts.jobs = max(2, min(4, str(multiprocessing.cpu_count())))

  if opts.addr2line_binary:
    assert os.path.isfile(opts.addr2line_binary)
    addr2line_binary = opts.addr2line_binary
  else:
    addr2line_binary = _find_in_system_path('addr2line')
    assert addr2line_binary, 'Unable to find addr2line in the path. '\
        'Use --addr2line-binary to specify location.'

  if opts.nm_binary:
    assert os.path.isfile(opts.nm_binary)
    nm_binary = opts.nm_binary
  else:
    nm_binary = _find_in_system_path('nm')
    assert nm_binary, 'Unable to find nm in the path. Use --nm-binary '\
        'to specify location.'

  print('nm: %s' % nm_binary)
  print('addr2line: %s' % addr2line_binary)

  symbols = GetNmSymbols(opts.nm_in, opts.nm_out, opts.library,
                         opts.jobs, opts.verbose is True,
                         addr2line_binary, nm_binary)
  if not os.path.exists(opts.destdir):
    os.makedirs(opts.destdir, 0755)


  if opts.legacy: # legacy report
    DumpTreemap(symbols, os.path.join(opts.destdir, 'treemap-dump.js'))
    DumpLargestSymbols(symbols,
                         os.path.join(opts.destdir, 'largest-symbols.js'), 100)
    DumpLargestSources(symbols,
                         os.path.join(opts.destdir, 'largest-sources.js'), 100)
    DumpLargestVTables(symbols,
                         os.path.join(opts.destdir, 'largest-vtables.js'), 100)
    treemap_out = os.path.join(opts.destdir, 'webtreemap')
    if not os.path.exists(treemap_out):
      os.makedirs(treemap_out, 0755)
    treemap_src = os.path.join('third_party', 'webtreemap', 'src')
    shutil.copy(os.path.join(treemap_src, 'COPYING'), treemap_out)
    shutil.copy(os.path.join(treemap_src, 'webtreemap.js'), treemap_out)
    shutil.copy(os.path.join(treemap_src, 'webtreemap.css'), treemap_out)
    shutil.copy(os.path.join('tools', 'binary_size', 'legacy_template',
                             'index.html'), opts.destdir)
  else: # modern report
    DumpCompactTree(symbols, os.path.join(opts.destdir, 'data.js'))
    d3_out = os.path.join(opts.destdir, 'd3')
    if not os.path.exists(d3_out):
      os.makedirs(d3_out, 0755)
    d3_src = os.path.join(os.path.dirname(__file__),
                          '..',
                          '..',
                          'third_party', 'd3', 'src')
    template_src = os.path.join(os.path.dirname(__file__),
                                'template')
    shutil.copy(os.path.join(d3_src, 'LICENSE'), d3_out)
    shutil.copy(os.path.join(d3_src, 'd3.js'), d3_out)
    print('Copying index.html')
    shutil.copy(os.path.join(template_src, 'index.html'), opts.destdir)
    shutil.copy(os.path.join(template_src, 'D3SymbolTreeMap.js'), opts.destdir)

  if opts.verbose:
    print 'Report saved to ' + opts.destdir + '/index.html'


if __name__ == '__main__':
  sys.exit(main())
