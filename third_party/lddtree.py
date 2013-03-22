#!/usr/bin/python
# Copyright 2012 Gentoo Foundation
# Copyright 2012 Mike Frysinger <vapier@gentoo.org>
# Use of this source code is governed by a BSD-style license (BSD-3)
# $Header: /var/cvsroot/gentoo-projects/pax-utils/lddtree.py,v 1.18 2013/01/05 20:39:56 vapier Exp $

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


def normpath(path):
	"""Normalize a path

	Python's os.path.normpath() doesn't handle some cases:
		// -> //
		//..// -> //
		//..//..// -> ///
	"""
	return os.path.normpath(path).replace('//', '/')


def dedupe(input):
	"""Remove all duplicates from input (keeping order)"""
	seen = {}
	return [seen.setdefault(x, x) for x in input if x not in seen]


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
		else:
			ldpath = ldpath.replace('$ORIGIN', os.path.dirname(path))
		ldpaths.append(normpath(root + ldpath))
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
		frozenset(['ELFOSABI_NONE', 'ELFOSABI_SYSV', 'ELFOSABI_LINUX']),
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
			with open(path) as f:
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

	with open(path) as f:
		elf = ELFFile(f)

		# If this is the first ELF, extract the interpreter.
		if _first:
			for segment in elf.iter_segments():
				if segment.header.p_type != 'PT_INTERP':
					continue

				interp = segment.get_interp_name()
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
					rpaths = ParseLdPaths(t.rpath, root=root, path=path)
				elif t.entry.d_tag == 'DT_RUNPATH':
					runpaths = ParseLdPaths(t.runpath, root=root, path=path)
				elif t.entry.d_tag == 'DT_NEEDED':
					libs.append(t.needed)
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
	id = '$Id: lddtree.py,v 1.18 2013/01/05 20:39:56 vapier Exp $'.split()
	print('%s-%s %s %s' % (id[1].split('.')[0], id[2], id[3], id[4]))
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
	def _copy(src):
		if src is None:
			return

		dst = options.dest + src
		if os.path.exists(dst):
			# See if they're the same file.
			ostat = os.stat(src)
			nstat = os.stat(dst)
			for field in ('mode', 'mtime', 'size'):
				if getattr(ostat, 'st_' + field) != \
				   getattr(nstat, 'st_' + field):
					break
			else:
				return

		if options.verbose:
			print('%s -> %s' % (src, dst))

		try:
			os.makedirs(os.path.dirname(dst))
		except OSError as e:
			if e.errno != os.errno.EEXIST:
				raise
		try:
			shutil.copy2(src, dst)
			return
		except IOError:
			os.unlink(dst)
		shutil.copy2(src, dst)

	_copy(elf['path'])
	_copy(elf['interp'])
	for lib in elf['libs']:
		_copy(elf['libs'][lib]['path'])


def main(argv):
	parser = optparse.OptionParser("""%prog [options] <ELFs>

Display ELF dependencies as a tree""")
	parser.add_option('-a', '--all',
		action='store_true', default=False,
		help=('Show all duplicated dependencies'))
	parser.add_option('-R', '--root',
		dest='root', default=os.environ.get('ROOT', ''), type='string',
		action='callback', callback=_NormalizePath,
		help=('Show all duplicated dependencies'))
	parser.add_option('--copy-to-tree',
		dest='dest', default=None, type='string',
		action='callback', callback=_NormalizePath,
		help=('Copy all files to the specified tree'))
	parser.add_option('-l', '--list',
		action='store_true', default=False,
		help=('Display output in a simple list (easy for copying)'))
	parser.add_option('-x', '--debug',
		action='store_true', default=False,
		help=('Run with debugging'))
	parser.add_option('-v', '--verbose',
		action='store_true', default=False,
		help=('Be verbose'))
	parser.add_option('-V', '--version',
		action='callback', callback=_ShowVersion,
		help=('Show version information'))
	(options, paths) = parser.parse_args(argv)

	# Throw away argv[0].
	paths.pop(0)
	if options.root != '/':
		options.root += '/'

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
		try:
			elf = ParseELF(path, options.root, ldpaths)
		except (exceptions.ELFError, IOError) as e:
			ret = 1
			warn('%s: %s' % (path, e))
			continue
		if options.dest is None:
			_ActionShow(options, elf)
		else:
			_ActionCopy(options, elf)
	return ret


if __name__ == '__main__':
	sys.exit(main(sys.argv))
