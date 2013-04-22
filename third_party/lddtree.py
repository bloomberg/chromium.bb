#!/usr/bin/python
# Copyright 2012-2013 Gentoo Foundation
# Copyright 2012-2013 Mike Frysinger <vapier@gentoo.org>
# Use of this source code is governed by a BSD-style license (BSD-3)
# pylint: disable=C0301
# $Header: /var/cvsroot/gentoo-projects/pax-utils/lddtree.py,v 1.41 2013/04/22 22:02:43 vapier Exp $

# TODO: Handle symlinks.

"""Read the ELF dependency tree and show it

This does not work like `ldd` in that we do not execute/load code (only read
files on disk), and we should the ELFs as a tree rather than a flat list.
"""

from __future__ import print_function

import glob
import errno
import optparse
import os
import shutil
import sys

from elftools.elf.elffile import ELFFile
from elftools.common import exceptions


def warn(msg, prefix='warning'):
  """Write |msg| to stderr with a |prefix| before it"""
  print('%s: %s: %s' % (sys.argv[0], prefix, msg), file=sys.stderr)


def err(msg, status=1):
  """Write |msg| to stderr and exit with |status|"""
  warn(msg, prefix='error')
  sys.exit(status)


def bstr(buf):
  """Decode the byte string into a string"""
  return buf.decode('utf-8')


def normpath(path):
  """Normalize a path

  Python's os.path.normpath() doesn't handle some cases:
    // -> //
    //..// -> //
    //..//..// -> ///
  """
  return os.path.normpath(path).replace('//', '/')


def makedirs(path):
  """Like os.makedirs(), but ignore EEXIST errors"""
  try:
    os.makedirs(path)
  except OSError as e:
    if e.errno != os.errno.EEXIST:
      raise


def dedupe(items):
  """Remove all duplicates from |items| (keeping order)"""
  seen = {}
  return [seen.setdefault(x, x) for x in items if x not in seen]


def GenerateLdsoWrapper(root, path, interp, libpaths=()):
  """Generate a shell script wrapper which uses local ldso to run the ELF

  Since we cannot rely on the host glibc (or other libraries), we need to
  execute the local packaged ldso directly and tell it where to find our
  copies of libraries.

  Args:
    root: The root tree to generate scripts inside of
    path: The full path (inside |root|) to the program to wrap
    interp: The ldso interpreter that we need to execute
    libpaths: Extra lib paths to search for libraries
  """
  basedir = os.path.dirname(path)
  interp_dir, interp_name = os.path.split(interp)
  libpaths = dedupe([interp_dir] + list(libpaths))
  replacements = {
    'interp': os.path.join(os.path.relpath(interp_dir, basedir),
                           interp_name),
    'libpaths': ':'.join(['${basedir}/' + os.path.relpath(p, basedir)
                          for p in libpaths]),
  }
  wrapper = """#!/bin/sh
if ! base=$(realpath "$0" 2>/dev/null); then
  case $0 in
  /*) base=$0;;
  *)  base=${PWD:-`pwd`}/$0;;
  esac
fi
basedir=${base%%/*}
exec \
  "${basedir}/%(interp)s" \
  --library-path "%(libpaths)s" \
  --inhibit-rpath '' \
  "${base}.elf" \
  "$@"
"""
  wrappath = root + path
  os.rename(wrappath, wrappath + '.elf')
  with open(wrappath, 'w') as f:
    f.write(wrapper % replacements)
  os.chmod(wrappath, 0o0755)


def ParseLdPaths(str_ldpaths, root='', path=None):
  """Parse the colon-delimited list of paths and apply ldso rules to each

  Note the special handling as dictated by the ldso:
   - Empty paths are equivalent to $PWD
   - $ORIGIN is expanded to the path of the given file
   - (TODO) $LIB and friends

  Args:
    str_ldpath: A colon-delimited string of paths
    root: The path to prepend to all paths found
  Returns:
    list of processed paths
  """
  ldpaths = []
  for ldpath in str_ldpaths.split(':'):
    if ldpath == '':
      # The ldso treats "" paths as $PWD.
      ldpath = os.getcwd()
    elif '$ORIGIN' in ldpath:
      ldpath = ldpath.replace('$ORIGIN', os.path.dirname(path))
    else:
      ldpath = root + ldpath
    ldpaths.append(normpath(ldpath))
  return dedupe(ldpaths)


def ParseLdSoConf(ldso_conf, root='/', _first=True):
  """Load all the paths from a given ldso config file

  This should handle comments, whitespace, and "include" statements.

  Args:
    ldso_conf: The file to scan
    root: The path to prepend to all paths found
    _first: Recursive use only; is this the first ELF ?
  Returns:
    list of paths found
  """
  paths = []

  try:
    with open(ldso_conf) as f:
      for line in f.readlines():
        line = line.split('#', 1)[0].strip()
        if not line:
          continue
        if line.startswith('include '):
          line = line[8:]
          if line[0] == '/':
            line = root + line.lstrip('/')
          else:
            line = os.path.dirname(ldso_conf) + '/' + line
          for path in glob.glob(line):
            paths += ParseLdSoConf(path, root=root, _first=False)
        else:
          paths += [normpath(root + line)]
  except IOError as e:
    if e.errno != errno.ENOENT:
      warn(e)

  if _first:
    # XXX: Load paths from ldso itself.
    # Remove duplicate entries to speed things up.
    paths = dedupe(paths)

  return paths


def LoadLdpaths(root='/'):
  """Load linker paths from common locations

  This parses the ld.so.conf and LD_LIBRARY_PATH env var.

  Args:
    root: The root tree to prepend to paths
  Returns:
    dict containing library paths to search
  """
  ldpaths = {
    'conf': [],
    'env': [],
    'interp': [],
  }

  # Load up $LD_LIBRARY_PATH.
  ldpaths['env'] = []
  env_ldpath = os.environ.get('LD_LIBRARY_PATH')
  if not env_ldpath is None:
    if root != '/':
      warn('ignoring LD_LIBRARY_PATH due to ROOT usage')
    else:
      # XXX: If this contains $ORIGIN, we probably have to parse this
      # on a per-ELF basis so it can get turned into the right thing.
      ldpaths['env'] = ParseLdPaths(env_ldpath, path='')

  # Load up /etc/ld.so.conf.
  ldpaths['conf'] = ParseLdSoConf(root + 'etc/ld.so.conf', root=root)

  return ldpaths


def CompatibleELFs(elf1, elf2):
  """See if two ELFs are compatible

  This compares the aspects of the ELF to see if they're compatible:
  bit size, endianness, machine type, and operating system.

  Args:
    elf1: an ELFFile object
    elf2: an ELFFile object
  Returns:
    True if compatible, False otherwise
  """
  osabis = frozenset([e.header['e_ident']['EI_OSABI'] for e in (elf1, elf2)])
  compat_sets = (
    frozenset('ELFOSABI_%s' % x for x in ('NONE', 'SYSV', 'GNU', 'LINUX',)),
  )
  return ((len(osabis) == 1 or any(osabis.issubset(x) for x in compat_sets)) and
    elf1.elfclass == elf2.elfclass and
    elf1.little_endian == elf2.little_endian and
    elf1.header['e_machine'] == elf2.header['e_machine'])


def FindLib(elf, lib, ldpaths):
  """Try to locate a |lib| that is compatible to |elf| in the given |ldpaths|

  Args:
    elf: the elf which the library should be compatible with (ELF wise)
    lib: the library (basename) to search for
    ldpaths: a list of paths to search
  Returns:
    the full path to the desired library
  """
  for ldpath in ldpaths:
    path = os.path.join(ldpath, lib)
    if os.path.exists(path):
      with open(path, 'rb') as f:
        libelf = ELFFile(f)
        if CompatibleELFs(elf, libelf):
          return path
  return None


def ParseELF(path, root='/', ldpaths={'conf':[], 'env':[], 'interp':[]},
             _first=True, _all_libs={}):
  """Parse the ELF dependency tree of the specified file

  Args:
    path: The ELF to scan
    root: The root tree to prepend to paths; this applies to interp and rpaths
          only as |path| and |ldpaths| are expected to be prefixed already
    ldpaths: dict containing library paths to search; should have the keys:
             conf, env, interp
    _first: Recursive use only; is this the first ELF ?
    _all_libs: Recursive use only; dict of all libs we've seen
  Returns:
    a dict containing information about all the ELFs; e.g.
    {
      'interp': '/lib64/ld-linux.so.2',
      'needed': ['libc.so.6', 'libcurl.so.4',],
      'libs': {
        'libc.so.6': {
          'path': '/lib64/libc.so.6',
          'needed': [],
        },
        'libcurl.so.4': {
          'path': '/usr/lib64/libcurl.so.4',
          'needed': ['libc.so.6', 'librt.so.1',],
        },
      },
    }
  """
  if _first:
    _all_libs = {}
    ldpaths = ldpaths.copy()
  ret = {
    'interp': None,
    'path': path,
    'needed': [],
    'rpath': [],
    'runpath': [],
    'libs': _all_libs,
  }

  with open(path, 'rb') as f:
    elf = ELFFile(f)

    # If this is the first ELF, extract the interpreter.
    if _first:
      for segment in elf.iter_segments():
        if segment.header.p_type != 'PT_INTERP':
          continue

        interp = bstr(segment.get_interp_name())
        ret['interp'] = normpath(root + interp)
        ret['libs'][os.path.basename(interp)] = {
          'path': ret['interp'],
          'needed': [],
        }
        # XXX: Should read it and scan for /lib paths.
        ldpaths['interp'] = [
          normpath(root + os.path.dirname(interp)),
          normpath(root + '/usr' + os.path.dirname(interp)),
        ]
        break

    # Parse the ELF's dynamic tags.
    libs = []
    rpaths = []
    runpaths = []
    for segment in elf.iter_segments():
      if segment.header.p_type != 'PT_DYNAMIC':
        continue

      for t in segment.iter_tags():
        if t.entry.d_tag == 'DT_RPATH':
          rpaths = ParseLdPaths(bstr(t.rpath), root=root, path=path)
        elif t.entry.d_tag == 'DT_RUNPATH':
          runpaths = ParseLdPaths(bstr(t.runpath), root=root, path=path)
        elif t.entry.d_tag == 'DT_NEEDED':
          libs.append(bstr(t.needed))
      if runpaths:
        # If both RPATH and RUNPATH are set, only the latter is used.
        rpaths = []

      # XXX: We assume there is only one PT_DYNAMIC.  This is
      # probably fine since the runtime ldso does the same.
      break
    if _first:
      # Propagate the rpaths used by the main ELF since those will be
      # used at runtime to locate things.
      ldpaths['rpath'] = rpaths
      ldpaths['runpath'] = runpaths
    ret['rpath'] = rpaths
    ret['runpath'] = runpaths
    ret['needed'] = libs

    # Search for the libs this ELF uses.
    all_ldpaths = None
    for lib in libs:
      if lib in _all_libs:
        continue
      if all_ldpaths is None:
        all_ldpaths = rpaths + ldpaths['rpath'] + ldpaths['env'] + runpaths + ldpaths['runpath'] + ldpaths['conf'] + ldpaths['interp']
      fullpath = FindLib(elf, lib, all_ldpaths)
      _all_libs[lib] = {
        'path': fullpath,
        'needed': [],
      }
      if fullpath:
        lret = ParseELF(fullpath, root, ldpaths, False, _all_libs)
        _all_libs[lib]['needed'] = lret['needed']

    del elf

  return ret


def _NormalizePath(option, _opt, value, parser):
  setattr(parser.values, option.dest, normpath(value))


def _ShowVersion(_option, _opt, _value, _parser):
  d = '$Id: lddtree.py,v 1.41 2013/04/22 22:02:43 vapier Exp $'.split()
  print('%s-%s %s %s' % (d[1].split('.')[0], d[2], d[3], d[4]))
  sys.exit(0)


def _ActionShow(options, elf):
  """Show the dependency tree for this ELF"""
  def _show(lib, depth):
    chain_libs.append(lib)
    fullpath = elf['libs'][lib]['path']
    if options.list:
      print(fullpath or lib)
    else:
      print('%s%s => %s' % ('    ' * depth, lib, fullpath))

    new_libs = []
    for lib in elf['libs'][lib]['needed']:
      if lib in chain_libs:
        if not options.list:
          print('%s%s => !!! circular loop !!!' % ('    ' * depth, lib))
        continue
      if options.all or not lib in shown_libs:
        shown_libs.add(lib)
        new_libs.append(lib)

    for lib in new_libs:
      _show(lib, depth + 1)
    chain_libs.pop()

  shown_libs = set(elf['needed'])
  chain_libs = []
  interp = elf['interp']
  if interp:
    shown_libs.add(os.path.basename(interp))
  if options.list:
    print(elf['path'])
    if not interp is None:
      print(interp)
  else:
    print('%s (interpreter => %s)' % (elf['path'], interp))
  for lib in elf['needed']:
    _show(lib, 1)


def _ActionCopy(options, elf):
  """Copy the ELF and its dependencies to a destination tree"""
  def _StripRoot(path):
    return path[len(options.root) - 1:]

  def _copy(src, striproot=True, wrapit=False, libpaths=(), outdir=None):
    if src is None:
      return

    if wrapit:
      # Static ELFs don't need to be wrapped.
      if not elf['interp']:
        wrapit = False

    striproot = _StripRoot if striproot else lambda x: x

    if outdir:
      subdst = os.path.join(outdir, os.path.basename(src))
    else:
      subdst = striproot(src)
    dst = options.dest + subdst

    try:
      # See if they're the same file.
      nstat = os.stat(dst + ('.elf' if wrapit else ''))
      ostat = os.stat(src)
      for field in ('mode', 'mtime', 'size'):
        if getattr(ostat, 'st_' + field) != \
           getattr(nstat, 'st_' + field):
          break
      else:
        return
    except OSError as e:
      if e.errno != errno.ENOENT:
        raise

    if options.verbose:
      print('%s -> %s' % (src, dst))

    makedirs(os.path.dirname(dst))
    try:
      shutil.copy2(src, dst)
    except IOError:
      os.unlink(dst)
      shutil.copy2(src, dst)

    if wrapit:
      if options.verbose:
        print('generate wrapper %s' % (dst,))

      if options.libdir:
        interp = os.path.join(options.libdir, os.path.basename(elf['interp']))
      else:
        interp = _StripRoot(elf['interp'])
      GenerateLdsoWrapper(options.dest, subdst, interp, libpaths)

  # XXX: We should automatically import libgcc_s.so whenever libpthread.so
  # is copied over (since we know it can be dlopen-ed by NPTL at runtime).
  # Similarly, we should provide an option for automatically copying over
  # the libnsl.so and libnss_*.so libraries, as well as an open ended list
  # for known libs that get loaded (e.g. curl will dlopen(libresolv)).
  libpaths = set()
  for lib in elf['libs']:
    path = elf['libs'][lib]['path']
    if not options.libdir:
      libpaths.add(_StripRoot(os.path.dirname(path)))
    _copy(path, outdir=options.libdir)

  if not options.libdir:
    libpaths = list(libpaths)
    if elf['runpath']:
      libpaths = elf['runpath'] + libpaths
    else:
      libpaths = elf['rpath'] + libpaths
  else:
    libpaths.add(options.libdir)

  _copy(elf['interp'], outdir=options.libdir)
  _copy(elf['path'], striproot=options.auto_root,
        wrapit=options.generate_wrappers, libpaths=libpaths,
        outdir=options.bindir)


def main(argv):
  parser = optparse.OptionParser("""%prog [options] <ELFs>

Display ELF dependencies as a tree

<ELFs> can be globs that lddtree will take care of expanding.
Useful when you want to glob a path under the ROOT path.

When using the --root option, all paths are implicitly prefixed by that.
  e.g. lddtree -R /my/magic/root /bin/bash
This will load up the ELF found at /my/magic/root/bin/bash and then resolve
all libraries via that path.  If you wish to actually read /bin/bash (and
so use the ROOT path as an alternative library tree), you can specify the
--no-auto-root option.

When pairing --root with --copy-to-tree, the ROOT path will be stripped.
  e.g. lddtree -R /my/magic/root --copy-to-tree /foo /bin/bash
You will see /foo/bin/bash and /foo/lib/libc.so.6 and not paths like
/foo/my/magic/root/bin/bash.  If you want that, you'll have to manually
add the ROOT path to the output path.

The --bindir and --libdir flags are used to normalize the output subdirs
when used with --copy-to-tree.
  e.g. lddtree --copy-to-tree /foo /bin/bash /usr/sbin/lspci /usr/bin/lsof
This will mirror the input paths in the output.  So you will end up with
/foo/bin/bash and /foo/usr/sbin/lspci and /foo/usr/bin/lsof.  Similarly,
the libraries needed will be scattered among /foo/lib/ and /foo/usr/lib/
and perhaps other paths (like /foo/lib64/ and /usr/lib/gcc/...).  You can
collapse all that down into nice directory structure.
  e.g. lddtree --copy-to-tree /foo /bin/bash /usr/sbin/lspci /usr/bin/lsof \\
               --bindir /bin --libdir /lib
This will place bash, lspci, and lsof into /foo/bin/.  All the libraries
they need will be placed into /foo/lib/ only.""")
  parser.add_option('-a', '--all',
    action='store_true', default=False,
    help='Show all duplicated dependencies')
  parser.add_option('-R', '--root',
    default=os.environ.get('ROOT', ''), type='string',
    action='callback', callback=_NormalizePath,
    help='Search for all files/dependencies in ROOT')
  parser.add_option('--no-auto-root',
    dest='auto_root', action='store_false', default=True,
    help='Do not automatically prefix input ELFs with ROOT')
  parser.add_option('-l', '--list',
    action='store_true', default=False,
    help='Display output in a simple list (easy for copying)')
  parser.add_option('-x', '--debug',
    action='store_true', default=False,
    help='Run with debugging')
  parser.add_option('-v', '--verbose',
    action='store_true', default=False,
    help='Be verbose')
  parser.add_option('--skip-non-elfs',
    action='store_true', default=False,
    help='Skip plain (non-ELF) files instead of warning')
  parser.add_option('-V', '--version',
    action='callback', callback=_ShowVersion,
    help='Show version information')

  group = optparse.OptionGroup(parser, 'Copying options')
  group.add_option('--copy-to-tree',
    dest='dest', default=None, type='string',
    action='callback', callback=_NormalizePath,
    help='Copy all files to the specified tree')
  group.add_option('--bindir',
    default=None, type='string',
    action='callback', callback=_NormalizePath,
    help='Dir to store all ELFs specified on the command line')
  group.add_option('--libdir',
    default=None, type='string',
    action='callback', callback=_NormalizePath,
    help='Dir to store all ELF libs')
  group.add_option('--generate-wrappers',
    action='store_true', default=False,
    help='Wrap executable ELFs with scripts for local ldso')
  group.add_option('--copy-non-elfs',
    action='store_true', default=False,
    help='Copy over plain (non-ELF) files instead of warn+ignore')
  parser.add_option_group(group)

  (options, paths) = parser.parse_args(argv)

  if options.root != '/':
    options.root += '/'

  if options.bindir and options.bindir[0] != '/':
    parser.error('--bindir accepts absolute paths only')
  if options.libdir and options.libdir[0] != '/':
    parser.error('--libdir accepts absolute paths only')

  if options.skip_non_elfs and options.copy_non_elfs:
    parser.error('pick one handler for non-ELFs: skip or copy')

  if options.debug:
    print('root =', options.root)
    if options.dest:
      print('dest =', options.dest)
  if not paths:
    err('missing ELF files to scan')

  ldpaths = LoadLdpaths(options.root)
  if options.debug:
    print('ldpaths[conf] =', ldpaths['conf'])
    print('ldpaths[env]  =', ldpaths['env'])

  # Process all the files specified.
  ret = 0
  for path in paths:
    # Only auto-prefix the path if the ELF is absolute.
    # If it's a relative path, the user most likely wants
    # the local path.
    if options.auto_root and path.startswith('/'):
      path = options.root + path.lstrip('/')

    matched = False
    for p in glob.iglob(path):
      matched = True
      try:
        elf = ParseELF(p, options.root, ldpaths)
      except exceptions.ELFError as e:
        if options.skip_non_elfs:
          continue
        # XXX: Ugly.  Should unify with _Action* somehow.
        if options.dest is not None and options.copy_non_elfs:
          if os.path.exists(p):
            elf = {
              'interp': None,
              'libs': [],
              'runpath': [],
              'rpath': [],
              'path': p,
            }
            _ActionCopy(options, elf)
            continue
        ret = 1
        warn('%s: %s' % (p, e))
        continue
      except IOError as e:
        ret = 1
        warn('%s: %s' % (p, e))
        continue

      if options.dest is None:
        _ActionShow(options, elf)
      else:
        _ActionCopy(options, elf)

    if not matched:
      ret = 1
      warn('%s: did not match any paths' % (path,))

  return ret


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
