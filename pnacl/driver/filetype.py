#!/usr/bin/python
# Copyright (c) 2013 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Utilities for determining (and overriding) the types of files
"""

import subprocess

import artools
import driver_log
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

  # If this is LLVM bitcode, we don't have a good way of determining if it
  # is an object file or a non-finalized program, so just say 'po' for now.
  if IsLLVMBitcode(filename):
    return 'po'

  if IsPNaClBitcode(filename):
    return 'pexe'

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
