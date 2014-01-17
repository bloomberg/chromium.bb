#!/usr/bin/python
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Generate a spatial analysis against an arbitrary library.

To use, build the 'binary_size_tool' target. Then run this tool, passing
in the location of the library to be analyzed along with any other options
you desire.
"""

import collections
import fileinput
import json
import optparse
import os
import pprint
import re
import shutil
import subprocess
import sys
import tempfile


def FormatBytes(bytes):
  """Pretty-print a number of bytes."""
  if bytes > 1e6:
    bytes = bytes / 1.0e6
    return '%.1fm' % bytes
  if bytes > 1e3:
    bytes = bytes / 1.0e3
    return '%.1fk' % bytes
  return str(bytes)


def SymbolTypeToHuman(type):
  """Convert a symbol type as printed by nm into a human-readable name."""
  return {'b': 'bss',
          'd': 'data',
          'r': 'read-only data',
          't': 'code',
          'w': 'weak symbol',
          'v': 'weak symbol'}[type]


def ParseNm(input):
  """Parse nm output.

  Argument: an iterable over lines of nm output.

  Yields: (symbol name, symbol type, symbol size, source file path).
  Path may be None if nm couldn't figure out the source file.
  """

  # Match lines with size, symbol, optional location, optional discriminator
  sym_re = re.compile(r'^[0-9a-f]{8} ' # address (8 hex digits)
                      '([0-9a-f]{8}) ' # size (8 hex digits)
                      '(.) ' # symbol type, one character
                      '([^\t]+)' # symbol name, separated from next by tab
                      '(?:\t(.*):[\d\?]+)?.*$') # location
  # Match lines with addr but no size.
  addr_re = re.compile(r'^[0-9a-f]{8} (.) ([^\t]+)(?:\t.*)?$')
  # Match lines that don't have an address at all -- typically external symbols.
  noaddr_re = re.compile(r'^ {8} (.) (.*)$')

  for line in input:
    line = line.rstrip()
    match = sym_re.match(line)
    if match:
      size, type, sym = match.groups()[0:3]
      size = int(size, 16)
      type = type.lower()
      if type == 'v':
        type = 'w'  # just call them all weak
      if type == 'b':
        continue  # skip all BSS for now
      path = match.group(4)
      yield sym, type, size, path
      continue
    match = addr_re.match(line)
    if match:
      type, sym = match.groups()[0:2]
      # No size == we don't care.
      continue
    match = noaddr_re.match(line)
    if match:
      type, sym = match.groups()
      if type in ('U', 'w'):
        # external or weak symbol
        continue

    print >>sys.stderr, 'unparsed:', repr(line)


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
  for sym, type, size, path in symbols:
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
        type = type.lower()
        if 'vtable for ' in sym:
          tree['sizes']['[vtable]'] += size
        elif 'r' == type:
          tree['sizes']['[rodata]'] += size
        elif 'd' == type:
          tree['sizes']['[data]'] += size
        elif 'b' == type:
          tree['sizes']['[bss]'] += size
        elif 't' == type:
          # 'text' in binary parlance means 'code'.
          tree['sizes']['[code]'] += size
        elif 'w' == type:
          tree['sizes']['[weak]'] += size
        else:
          tree['sizes']['[other]'] += size
      except:
        print >>sys.stderr, sym, parts, key
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
      if css_class is not None: child_json['data']['$symbol'] = css_class
      children.append(child_json)
  # Sort children by size, largest to smallest.
  children.sort(key=lambda child: -child['data']['$area'])

  # For leaf nodes, the 'size' attribute is the size of the leaf;
  # Non-leaf nodes don't really have a size, but their 'size' attribute is
  # the sum of the sizes of all their children.
  return {'name': name + ' (' + FormatBytes(tree['size']) + ')',
          'data': { '$area': tree['size'] },
          'children': children }


def DumpTreemap(symbols, outfile):
  dirs = TreeifySymbols(symbols)
  out = open(outfile, 'w')
  try:
    out.write('var kTree = ' + json.dumps(JsonifyTree(dirs, '/')))
  finally:
    out.flush()
    out.close()


def DumpLargestSymbols(symbols, outfile, n):
  # a list of (sym, type, size, path); sort by size.
  symbols = sorted(symbols, key=lambda x: -x[2])
  dumped = 0
  out = open(outfile, 'w')
  try:
    out.write('var largestSymbols = [\n')
    for sym, type, size, path in symbols:
      if type in ('b', 'w'):
        continue  # skip bss and weak symbols
      if path is None:
        path = ''
      entry = {'size': FormatBytes(size),
               'symbol': sym,
               'type': SymbolTypeToHuman(type),
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
  for sym, type, size, path in symbols:
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


def DumpLargestSources(symbols, outfile, n):
  map = MakeSourceMap(symbols)
  sources = sorted(map.values(), key=lambda x: -x['size'])
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


def DumpLargestVTables(symbols, outfile, n):
  vtables = []
  for symbol, type, size, path in symbols:
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


def RunParallelAddress2Line(outfile, library, arch, jobs, verbose):
  """Run a parallel addr2line processing engine to dump and resolve symbols."""
  out_dir = os.getenv('CHROMIUM_OUT_DIR', 'out')
  build_type = os.getenv('BUILDTYPE', 'Release')
  classpath = os.path.join(out_dir, build_type, 'lib.java',
                           'binary_size_java.jar')
  cmd = ['java',
         '-classpath', classpath,
         'org.chromium.tools.binary_size.ParallelAddress2Line',
         '--disambiguate',
         '--outfile', outfile,
         '--library', library,
         '--threads', jobs]
  if verbose is True:
    cmd.append('--verbose')
  prefix = os.path.join('third_party', 'android_tools', 'ndk', 'toolchains')
  if arch == 'android-arm':
    prefix = os.path.join(prefix, 'arm-linux-androideabi-4.7', 'prebuilt',
                          'linux-x86_64', 'bin', 'arm-linux-androideabi-')
    cmd.extend(['--nm', prefix + 'nm', '--addr2line', prefix + 'addr2line'])
  elif arch == 'android-mips':
    prefix = os.path.join(prefix, 'mipsel-linux-android-4.7', 'prebuilt',
                          'linux-x86_64', 'bin', 'mipsel-linux-android-')
    cmd.extend(['--nm', prefix + 'nm', '--addr2line', prefix + 'addr2line'])
  elif arch == 'android-x86':
    prefix = os.path.join(prefix, 'x86-4.7', 'prebuilt',
                          'linux-x86_64', 'bin', 'i686-linux-android-')
    cmd.extend(['--nm', prefix + 'nm', '--addr2line', prefix + 'addr2line'])
  # else, use whatever is in PATH (don't pass --nm or --addr2line)

  if verbose:
    print cmd

  return_code = subprocess.call(cmd)
  if return_code:
    raise RuntimeError('Failed to run ParallelAddress2Line: returned ' +
                       str(return_code))


def GetNmSymbols(infile, outfile, library, arch, jobs, verbose):
  if infile is None:
    if outfile is None:
      infile = tempfile.NamedTemporaryFile(delete=False).name
    else:
      infile = outfile

    if verbose:
      print 'Running parallel addr2line, dumping symbols to ' + infile;
    RunParallelAddress2Line(outfile=infile, library=library, arch=arch,
             jobs=jobs, verbose=verbose)
  elif verbose:
    print 'Using nm input from ' + infile
  with file(infile, 'r') as infile:
    return list(ParseNm(infile))


def main():
  usage="""%prog [options]

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
  parser.add_option('--arch',
                    help='the architecture that the library is targeted to. '
                    'Determines which nm/addr2line binaries are used. When '
                    '\'host-native\' is chosen, the program will use whichever '
                    'nm/addr2line binaries are on the PATH. This is '
                    'appropriate when you are analyzing a binary by and for '
                    'your computer. '
                    'This argument is only valid when using --library. '
                    'Default is \'host-native\'.',
                    choices=['host-native', 'android-arm',
                             'android-mips', 'android-x86'],)
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
  opts, args = parser.parse_args()

  if ((not opts.library) and (not opts.nm_in)) or (opts.library and opts.nm_in):
    parser.error('exactly one of --library or --nm-in is required')
  if (opts.nm_in):
    if opts.jobs:
      print >> sys.stderr, ('WARNING: --jobs has no effect '
                            'when used with --nm-in')
    if opts.arch:
      print >> sys.stderr, ('WARNING: --arch has no effect '
                            'when used with --nm-in')
  if not opts.destdir:
    parser.error('--destdir is required argument')
  if not opts.jobs:
    opts.jobs = '1'
  if not opts.arch:
    opts.arch = 'host-native'

  symbols = GetNmSymbols(opts.nm_in, opts.nm_out, opts.library, opts.arch,
                           opts.jobs, opts.verbose is True)
  if not os.path.exists(opts.destdir):
    os.makedirs(opts.destdir, 0755)

  DumpTreemap(symbols, os.path.join(opts.destdir, 'treemap-dump.js'))
  DumpLargestSymbols(symbols,
                       os.path.join(opts.destdir, 'largest-symbols.js'), 100)
  DumpLargestSources(symbols,
                       os.path.join(opts.destdir, 'largest-sources.js'), 100)
  DumpLargestVTables(symbols,
                       os.path.join(opts.destdir, 'largest-vtables.js'), 100)

  # TODO(andrewhayden): Switch to D3 for greater flexibility
  treemap_out = os.path.join(opts.destdir, 'webtreemap')
  if not os.path.exists(treemap_out):
    os.makedirs(treemap_out, 0755)
  treemap_src = os.path.join('third_party', 'webtreemap', 'src')
  shutil.copy(os.path.join(treemap_src, 'COPYING'), treemap_out)
  shutil.copy(os.path.join(treemap_src, 'webtreemap.js'), treemap_out)
  shutil.copy(os.path.join(treemap_src, 'webtreemap.css'), treemap_out)
  shutil.copy(os.path.join('tools', 'binary_size', 'template', 'index.html'),
              opts.destdir)
  if opts.verbose:
    print 'Report saved to ' + opts.destdir + '/index.html'


if __name__ == '__main__':
  sys.exit(main())