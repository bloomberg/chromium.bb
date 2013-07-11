#!/usr/bin/python
# Copyright (c) 2013 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Utilities for determining (and overriding) the types of files
"""

# driver_tools.Run and driver_env.env are used by GetBitcodeMetadata to run
# llvm-dis. The metadata is still used to distinguish bitcode object files
# from non-final pexes.
# TODO(dschuff): Fix this last circular dependence, possibly by moving Run
# into driver_base
import subprocess

import artools
import driver_env
import driver_log
import driver_tools
import elftools
import ldtools

LLVM_BITCODE_MAGIC = 'BC\xc0\xde'
LLVM_WRAPPER_MAGIC = '\xde\xc0\x17\x0b'
PNACL_BITCODE_MAGIC = 'PEXE'

class SimpleCache(object):
  """ Cache results of a function using a dictionary. """

  __all_caches = dict()

  @classmethod
  def ClearAllCaches(cls):
    """ Clear cached results from all functions. """
    for d in cls.__all_caches.itervalues():
      d.clear()

  def __init__(self, f):
    SimpleCache.__all_caches[self] = dict()
    self.cache = SimpleCache.__all_caches[self]
    self.func = f

  def __call__(self, *args):
    if args in self.cache:
      return self.cache[args]
    else:
      result = self.func(*args)
      self.cache[args] = result
      return result

  def __repr__(self):
    return self.func.__doc__

  def OverrideValue(self, value, *args):
    """ Force a function call with |args| to return |value|. """
    self.cache[args] = value

  def ClearCache(self):
    """ Clear cached results for one instance (function). """
    self.cache.clear()


@SimpleCache
def IsNative(filename):
  return (IsNativeObject(filename) or
          IsNativeDSO(filename) or
          IsNativeArchive(filename))

@SimpleCache
def IsNativeObject(filename):
  return FileType(filename) == 'o'

@SimpleCache
def IsNativeDSO(filename):
  return FileType(filename) == 'so'

@SimpleCache
def IsBitcodeDSO(filename):
  return FileType(filename) == 'pso'

@SimpleCache
def IsBitcodeObject(filename):
  return FileType(filename) == 'po'

@SimpleCache
def GetBitcodeMagic(filename):
  fp = driver_log.DriverOpen(filename, 'rb')
  header = fp.read(4)
  driver_log.DriverClose(fp)
  return header

def IsLLVMBitcodeWrapperHeader(data):
  return data[:4] == LLVM_WRAPPER_MAGIC

@SimpleCache
def IsLLVMWrappedBitcode(filename):
  return IsLLVMBitcodeWrapperHeader(GetBitcodeMagic(filename))

def IsPNaClBitcodeHeader(data):
  return data[:4] == PNACL_BITCODE_MAGIC

@SimpleCache
def IsPNaClBitcode(filename):
  return IsPNaClBitcodeHeader(GetBitcodeMagic(filename))

def IsLLVMRawBitcodeHeader(data):
  return data[:4] == LLVM_BITCODE_MAGIC

@SimpleCache
def IsLLVMBitcode(filename):
  header = GetBitcodeMagic(filename)
  return IsLLVMRawBitcodeHeader(header) or IsLLVMBitcodeWrapperHeader(header)

@SimpleCache
def IsArchive(filename):
  return artools.IsArchive(filename)

@SimpleCache
def IsBitcodeArchive(filename):
  filetype = FileType(filename)
  return filetype == 'archive-bc'

@SimpleCache
def IsNativeArchive(filename):
  return IsArchive(filename) and not IsBitcodeArchive(filename)


# If FORCED_FILE_TYPE is set, FileType() will return FORCED_FILE_TYPE for all
# future input files. This is useful for the "as" incarnation, which
# needs to accept files of any extension and treat them as ".s" (or ".ll")
# files. Also useful for gcc's "-x", which causes all files between the
# current -x and the next -x to be treated in a certain way.
FORCED_FILE_TYPE = None
def SetForcedFileType(t):
  global FORCED_FILE_TYPE
  FORCED_FILE_TYPE = t

def GetForcedFileType():
  return FORCED_FILE_TYPE

def ForceFileType(filename, newtype = None):
  if newtype is None:
    if FORCED_FILE_TYPE is None:
      return
    newtype = FORCED_FILE_TYPE
  FileType.OverrideValue(newtype, filename)

def ClearFileTypeCaches():
  """ Clear caches for all filetype functions (externally they must all be
      cleared together because they can call each other)
  """
  SimpleCache.ClearAllCaches()

# File Extension -> Type string
# TODO(pdox): Add types for sources which should not be preprocessed.
ExtensionMap = {
  'c'   : 'c',
  'i'   : 'c',    # C, but should not be preprocessed.

  'cc'  : 'c++',
  'cp'  : 'c++',
  'cxx' : 'c++',
  'cpp' : 'c++',
  'CPP' : 'c++',
  'c++' : 'c++',
  'C'   : 'c++',
  'ii'  : 'c++',  # C++, but should not be preprocessed.

  'm'   : 'objc',  # .m = "Objective-C source file"

  'll'  : 'll',
  'bc'  : 'po',
  'po'  : 'po',   # .po = "Portable object file"
  'pexe': 'pexe', # .pexe = "Portable executable"
  'pso' : 'pso',  # .pso = "Portable Shared Object"
  'asm' : 'S',
  'S'   : 'S',
  'sx'  : 'S',
  's'   : 's',
  'o'   : 'o',
  'os'  : 'o',
  'so'  : 'so',
  'nexe': 'nexe',
}

def IsSourceType(filetype):
  return filetype in ('c','c++','objc')

# The SimpleCache decorator is required for correctness, due to the
# ForceFileType mechanism.
@SimpleCache
def FileType(filename):
  # Auto-detect bitcode files, since we can't rely on extensions
  ext = filename.split('.')[-1]

  # TODO(pdox): We open and read the the first few bytes of each file
  #             up to 4 times, when we only need to do it once. The
  #             OS cache prevents us from hitting the disk, but this
  #             is still slower than it needs to be.
  if IsArchive(filename):
    return artools.GetArchiveType(filename)

  if elftools.IsELF(filename):
    return GetELFType(filename)

  if IsLLVMBitcode(filename) or IsPNaClBitcode(filename):
    return GetBitcodeType(filename, False)

  if ldtools.IsLinkerScript(filename):
    return 'ldscript'

  # Use the file extension if it is recognized
  if ext in ExtensionMap:
    return ExtensionMap[ext]

  driver_log.Log.Fatal('%s: Unrecognized file type', filename)

@SimpleCache
def IsELF(filename):
  return elftools.IsELF(filename)

@SimpleCache
def GetELFType(filename):
  """ ELF type as determined by ELF metadata """
  assert(elftools.IsELF(filename))
  elfheader = elftools.GetELFHeader(filename)
  elf_type_map = {
    'EXEC': 'nexe',
    'REL' : 'o',
    'DYN' : 'so'
  }
  return elf_type_map[elfheader.type]

# Sandboxed LLC returns the metadata after translation. Use FORCED_METADATA
# if available instead of llvm-dis to avoid building llvm-dis on arm, and
# to make sure this interface gets tested
FORCED_METADATA = {}
@SimpleCache
def GetBitcodeMetadata(filename, assume_pexe=False):
  assert(IsLLVMBitcode(filename) or IsPNaClBitcode(filename))
  # The argument |assume_pexe| helps break a dependency on bitcode metadata,
  # as the shared library metadata is not finalized yet.
  if assume_pexe:
    return { 'OutputFormat': 'executable',
             'SOName'      : '',
             'NeedsLibrary': [] }
  global FORCED_METADATA
  if filename in FORCED_METADATA:
    return FORCED_METADATA[filename]

  llvm_dis = driver_env.env.getone('LLVM_DIS')
  args = [ llvm_dis, '-dump-metadata', filename ]
  _, stdout_contents, _  = driver_tools.Run(args,
                                            redirect_stdout=subprocess.PIPE)

  metadata = { 'OutputFormat': '',
               'SOName'      : '',
               'NeedsLibrary': [] }
  for line in stdout_contents.split('\n'):
    if not line.strip():
      continue
    k, v = line.split(':')
    k = k.strip()
    v = v.strip()
    if k.startswith('NeededRecord_'):
      metadata[k] = ''
    assert(k in metadata)
    if isinstance(metadata[k], list):
      metadata[k].append(v)
    else:
      metadata[k] = v

  return metadata

def SetBitcodeMetadata(filename, is_shared, soname, needed_libs):
  metadata = {}
  metadata['OutputFormat'] = 'shared' if is_shared else 'executable'
  metadata['NeedsLibrary'] = list(needed_libs)
  metadata['SOName'] = soname
  global FORCED_METADATA
  FORCED_METADATA[filename] = metadata

@SimpleCache
def GetBitcodeType(filename, assume_pexe):
  """ Bitcode type as determined by bitcode metadata """
  if IsPNaClBitcode(filename):
    return 'pexe'
  metadata = GetBitcodeMetadata(filename, assume_pexe)
  format_map = {
    'object': 'po',
    'shared': 'pso',
    'executable': 'pexe'
  }
  return format_map[metadata['OutputFormat']]

# Map from GCC's -x file types and this driver's file types.
FILE_TYPE_MAP = {
    'c'                 : 'c',
    'c++'               : 'c++',
    'assembler'         : 's',
    'assembler-with-cpp': 'S',
}
FILE_TYPE_MAP_REVERSE = dict([reversed(_tmp) for _tmp in FILE_TYPE_MAP.items()])

def FileTypeToGCCType(filetype):
  return FILE_TYPE_MAP_REVERSE[filetype]

def GCCTypeToFileType(gcctype):
  if gcctype not in FILE_TYPE_MAP:
    Log.Fatal('language "%s" not recognized' % gcctype)
  return FILE_TYPE_MAP[gcctype]
