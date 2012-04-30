#!/usr/bin/python
# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# IMPORTANT NOTE: If you make local mods to this file, you must run:
#   %  pnacl/build.sh driver
# in order for them to take effect in the scons build.  This command
# updates the copy in the toolchain/ tree.
#

# Generate a draft manifest file given bitcode pexe.
# URLs and unknown runtime dependencies may be manually changed afterwards.

from driver_tools import *
from driver_env import env
from driver_log import Log, DriverOpen, TempFiles
from collections import deque
import hashlib

try:
  import json
except Exception:
  import simplejson as json

EXTRA_ENV = {
  'INPUTS'   : '',
  'OUTPUT'   : '',

  'BASELINE_NMF' : '',

  # FLAT_URL_SCHEME is '1' if we are just trying to test via scons and
  # do not want to put libraries under any kind of directory structure
  # to simplify file staging. This means that really only one of the files
  # is valid, since all arches will have the same URL. This is only for tests.
  'FLAT_URL_SCHEME' : '0',

  # Search path for finding pso's to grab transitive dependencies.
  'SEARCH_DIRS'        : '${SEARCH_DIRS_USER} ${SEARCH_DIRS_BUILTIN}',
  'SEARCH_DIRS_USER'   : '',
  # This doesn't include native lib directories, so we aren't
  # tracing transitive dependencies for those.
  'SEARCH_DIRS_BUILTIN': '  ${BASE_USR}/lib/ ' +
                         '  ${BASE_SDK}/lib/ ' +
                         '  ${BASE_LIB}/ ',
}

NMF_PATTERNS = [
  ( ('-L', '(.+)'),
    "env.append('SEARCH_DIRS_USER', pathtools.normalize($0))\n"),
  ( '-L(.+)',
    "env.append('SEARCH_DIRS_USER', pathtools.normalize($0))\n"),
  ( '--library-path=(.+)',
    "env.append('SEARCH_DIRS_USER', pathtools.normalize($0))\n"),
  ( ('--base-nmf', '(.+)'),
    "env.set('BASELINE_NMF', pathtools.normalize($0))\n"),

  ( ('-o', '(.+)'), "env.set('OUTPUT', pathtools.normalize($0))\n"),
  ( '--flat-url-scheme', "env.set('FLAT_URL_SCHEME', '1')"),

  ( '(.*)',      "env.append('INPUTS', pathtools.normalize($0))"),
]

def get_help(argv):
  return """
NaCl Manifest Generator for PNaCl.

Usage: %s [options] <pexe_file>

BASIC OPTIONS:
  -o <file>               Output to <file>
  --base-nmf <file>       Use a baseline NMF file as a starting point.
  -L <dir>                Add library search path
  --library-path=<dir>    Ditto.
  --flat-url-scheme       For testing: Do not nest architecture-specific
                          files in an architecture-specific directory
                          when generating URLs.
""" % env.getone('SCRIPT_NAME')

def AddUrlForAllArches(json_dict, shortname):
  KNOWN_NMF_ARCHES = ['x86-32', 'x86-64', 'arm']
  for arch in KNOWN_NMF_ARCHES:
    json_dict[arch] = {}
    if env.getbool('FLAT_URL_SCHEME'):
      json_dict[arch]['url'] = '%s' % shortname
    else:
      json_dict[arch]['url'] = 'lib-%s/%s' % (arch, shortname)

def SetPortableUrl(json_dict, shortname, filepath):
  json_dict['portable'] = {}
  json_dict['portable']['pnacl-translate'] = {}
  json_dict['portable']['pnacl-translate']['url'] = shortname
  fd = DriverOpen(filepath, 'r')
  sha = hashlib.sha256()
  sha.update(fd.read())
  fd.close()
  json_dict['portable']['pnacl-translate']['sha256'] = sha.hexdigest()

def GetActualFilePathAndType(shortname):
  actual_lib_path = ldtools.FindFile([shortname],
                                     env.get('SEARCH_DIRS'),
                                     ldtools.LibraryTypes.ANY)
  if actual_lib_path == None:
    Log.Warning('Could not find path of lib: %s, assuming it is native',
                shortname)
    file_type = 'so'
  else:
    file_type = FileType(actual_lib_path)
  return actual_lib_path, file_type

def GetTransitiveClosureOfNeeded(base_needed):
  visited_portable = set()
  visited_native = set()
  all_needed = set(base_needed)
  worklist = deque(base_needed)
  while len(worklist) != 0:
    lib = worklist.popleft()
    if lib in visited_portable or lib in visited_native:
      continue
    actual_lib_path, file_type = GetActualFilePathAndType(lib)
    if file_type == 'so':
      # We could trace if we used objdump / readelf...
      Log.Warning('Not tracing dependencies of native lib %s', lib)
      visited_native.add(lib)
      continue
    elif file_type == 'pso':
      visited_portable.add(lib)
      more_needed = GetBitcodeMetadata(actual_lib_path).get('NeedsLibrary', [])
      all_needed |= set(more_needed)
      worklist.extend(more_needed)
    else:
      Log.Fatal('Lib: %s is neither bitcode nor native', lib)
  return all_needed

def ReadBaselineNMF(baseline_file):
  f = DriverOpen(baseline_file, 'r')
  json_dict = json.load(f)
  f.close()
  return json_dict

def EchoBaselineNMF(baseline_nmf, output_file):
  json.dump(baseline_nmf, output_file, sort_keys=True, indent=2)

def GenerateStaticNMF(pexe_file, baseline_nmf, output_file):
  nmf_json = baseline_nmf
  nmf_json['program'] = {}
  SetPortableUrl(nmf_json['program'],
                 pathtools.basename(pexe_file),
                 pexe_file)
  json.dump(nmf_json, output_file, sort_keys=True, indent=2)


def GenerateDynamicNMF(pexe_file, baseline_nmf, needed, output_file):
  nmf_json = baseline_nmf
  # Set runnable-ld.so as the program interpreter.
  nmf_json['program'] = {}
  AddUrlForAllArches(nmf_json['program'], 'runnable-ld.so')
  # Set the pexe as the main program.
  nmf_json['files'] = baseline_nmf.get('files', {})
  nmf_json['files']['main.nexe'] = {}
  SetPortableUrl(nmf_json['files']['main.nexe'],
                 pathtools.basename(pexe_file),
                 pexe_file)

  # Get transitive closure of needed libraries.
  transitive_needed = GetTransitiveClosureOfNeeded(needed)

  # Make urls for libraries.
  for lib in transitive_needed:
    nmf_json['files'][lib] = {}
    actual_lib_path, file_type = GetActualFilePathAndType(lib)
    if file_type == 'so':
      # Assume a native version exists for every known arch.
      AddUrlForAllArches(nmf_json['files'][lib], lib)
    elif file_type == 'pso':
      SetPortableUrl(nmf_json['files'][lib],
                     lib,
                     actual_lib_path)
    else:
      Log.Fatal('Needed library is not a .so nor a .pso')
  json.dump(nmf_json, output_file, sort_keys=True, indent=2)


def main(argv):
  env.update(EXTRA_ENV)
  ParseArgs(argv, NMF_PATTERNS)
  exe_file = env.getone('INPUTS')

  if not exe_file:
    Log.Fatal('Expecting input exe_file')
    DriverExit(1)

  baseline_file = env.getone('BASELINE_NMF')
  if not baseline_file:
    baseline_nmf = {}
  else:
    baseline_nmf = ReadBaselineNMF(baseline_file)

  output = env.getone('OUTPUT')
  if output == '':
    output_file = sys.stdout
  else:
    output_file = DriverOpen(output, 'w')

  if not FileType(exe_file) == 'pexe':
    EchoBaselineNMF(baseline_nmf, output_file)
    return 0
  needed = GetBitcodeMetadata(exe_file).get('NeedsLibrary', [])
  if len(needed) == 0:
    GenerateStaticNMF(exe_file, baseline_nmf, output_file)
  else:
    # runnable_ld.so is going to ask for libgcc_s.so.1 even though it does
    # not show up in the pexe's NEEDED metadata, so it won't show up
    # in the NMF and on the App's webserver.
    #
    # For now, hack in libgcc_s.so.1.
    # http://code.google.com/p/nativeclient/issues/detail?id=2582
    needed.append('libgcc_s.so.1')
    GenerateDynamicNMF(exe_file, baseline_nmf, needed, output_file)
  DriverClose(output_file)
  return 0
