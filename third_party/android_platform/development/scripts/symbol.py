#!/usr/bin/python
#
# Copyright (C) 2013 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""Module for looking up symbolic debugging information.

The information can include symbol names, offsets, and source locations.
"""

import os
import re
import subprocess

CHROME_SRC = os.path.join(os.path.realpath(os.path.dirname(__file__)),
                          os.pardir, os.pardir, os.pardir, os.pardir)
ANDROID_BUILD_TOP = CHROME_SRC
SYMBOLS_DIR = CHROME_SRC
CHROME_SYMBOLS_DIR = CHROME_SRC

ARCH = "arm"

TOOLCHAIN_INFO = None

def Uname():
  """'uname' for constructing prebuilt/<...> and out/host/<...> paths."""
  uname = os.uname()[0]
  if uname == "Darwin":
    proc = os.uname()[-1]
    if proc == "i386" or proc == "x86_64":
      return "darwin-x86"
    return "darwin-ppc"
  if uname == "Linux":
    return "linux-x86"
  return uname

def ToolPath(tool, toolchain_info=None):
  """Return a full qualified path to the specified tool"""
  # ToolPath looks for the tools in the completely incorrect directory.
  # This looks in the checked in android_tools.
  if ARCH == "arm":
    toolchain_source = "arm-linux-androideabi-4.6"
    toolchain_prefix = "arm-linux-androideabi"
    ndk = "ndk"
  elif ARCH == "arm64":
    toolchain_source = "aarch64-linux-android-4.9"
    toolchain_prefix = "aarch64-linux-android"
    ndk = "ndk"
  elif ARCH == "x86":
    toolchain_source = "x86-4.6"
    toolchain_prefix = "i686-android-linux"
    ndk = "ndk"
  elif ARCH == "x86_64":
    toolchain_source = "x86_64-4.9"
    toolchain_prefix = "x86_64-linux-android"
    ndk = "ndk"
  elif ARCH == "mips":
    toolchain_source = "mipsel-linux-android-4.6"
    toolchain_prefix = "mipsel-linux-android"
    ndk = "ndk"
  else:
    raise Exception("Could not find tool chain")

  toolchain_subdir = (
      "third_party/android_tools/%s/toolchains/%s/prebuilt/linux-x86_64/bin" %
       (ndk, toolchain_source))

  return os.path.join(CHROME_SRC,
                      toolchain_subdir,
                      toolchain_prefix + "-" + tool)

def FindToolchain():
  """Look for the latest available toolchain

  Args:
    None

  Returns:
    A pair of strings containing toolchain label and target prefix.
  """
  global TOOLCHAIN_INFO
  if TOOLCHAIN_INFO is not None:
    return TOOLCHAIN_INFO

  ## Known toolchains, newer ones in the front.
  if ARCH == "arm64":
    gcc_version = "4.9"
    known_toolchains = [
      ("aarch64-linux-android-" + gcc_version, "aarch64", "aarch64-linux-android")
    ]
  elif ARCH == "arm":
    gcc_version = "4.6"
    known_toolchains = [
      ("arm-linux-androideabi-" + gcc_version, "arm", "arm-linux-androideabi"),
    ]
  elif ARCH =="x86":
    known_toolchains = [
      ("i686-android-linux-4.4.3", "x86", "i686-android-linux")
    ]
  elif ARCH =="x86_64":
    known_toolchains = [
      ("x86_64-linux-android-4.9", "x86_64", "x86_64-linux-android")
    ]
  elif ARCH == "mips":
    gcc_version = "4.6"
    known_toolchains = [
      ("mipsel-linux-android-" + gcc_version, "mips", "mipsel-linux-android")
    ]
  else:
    known_toolchains = []

  # Look for addr2line to check for valid toolchain path.
  for (label, platform, target) in known_toolchains:
    toolchain_info = (label, platform, target);
    if os.path.exists(ToolPath("addr2line", toolchain_info)):
      TOOLCHAIN_INFO = toolchain_info
      print "Using toolchain from :" + ToolPath("", TOOLCHAIN_INFO)
      return toolchain_info

  raise Exception("Could not find tool chain")

def TranslateLibPath(lib):
  # SymbolInformation(lib, addr) receives lib as the path from symbols
  # root to the symbols file. This needs to be translated to point to the
  # correct .so path. If the user doesn't explicitly specify which directory to
  # use, then use the most recently updated one in one of the known directories.
  # If the .so is not found somewhere in CHROME_SYMBOLS_DIR, leave it
  # untranslated in case it is an Android symbol in SYMBOLS_DIR.
  library_name = os.path.basename(lib)
  out_dir = os.environ.get('CHROMIUM_OUT_DIR', 'out')
  candidate_dirs = ['.',
                    os.path.join(out_dir, 'Debug', 'lib'),
                    os.path.join(out_dir, 'Debug', 'lib.target'),
                    os.path.join(out_dir, 'Release', 'lib'),
                    os.path.join(out_dir, 'Release', 'lib.target'),
                    ]

  candidate_libraries = map(
      lambda d: ('%s/%s/%s' % (CHROME_SYMBOLS_DIR, d, library_name)),
      candidate_dirs)
  candidate_libraries = filter(os.path.exists, candidate_libraries)
  candidate_libraries = sorted(candidate_libraries,
                               key=os.path.getmtime, reverse=True)

  if not candidate_libraries:
    return lib

  library_path = os.path.relpath(candidate_libraries[0], SYMBOLS_DIR)
  return '/' + library_path

def SymbolInformation(lib, addr, get_detailed_info):
  """Look up symbol information about an address.

  Args:
    lib: library (or executable) pathname containing symbols
    addr: string hexidecimal address

  Returns:
    A list of the form [(source_symbol, source_location,
    object_symbol_with_offset)].

    If the function has been inlined then the list may contain
    more than one element with the symbols for the most deeply
    nested inlined location appearing first.  The list is
    always non-empty, even if no information is available.

    Usually you want to display the source_location and
    object_symbol_with_offset from the last element in the list.
  """
  lib = TranslateLibPath(lib)
  info = SymbolInformationForSet(lib, set([addr]), get_detailed_info)
  return (info and info.get(addr)) or [(None, None, None)]


def SymbolInformationForSet(lib, unique_addrs, get_detailed_info):
  """Look up symbol information for a set of addresses from the given library.

  Args:
    lib: library (or executable) pathname containing symbols
    unique_addrs: set of hexidecimal addresses

  Returns:
    A dictionary of the form {addr: [(source_symbol, source_location,
    object_symbol_with_offset)]} where each address has a list of
    associated symbols and locations.  The list is always non-empty.

    If the function has been inlined then the list may contain
    more than one element with the symbols for the most deeply
    nested inlined location appearing first.  The list is
    always non-empty, even if no information is available.

    Usually you want to display the source_location and
    object_symbol_with_offset from the last element in the list.
  """
  if not lib:
    return None

  addr_to_line = CallAddr2LineForSet(lib, unique_addrs)
  if not addr_to_line:
    return None

  if get_detailed_info:
    addr_to_objdump = CallObjdumpForSet(lib, unique_addrs)
    if not addr_to_objdump:
      return None
  else:
    addr_to_objdump = dict((addr, ("", 0)) for addr in unique_addrs)

  result = {}
  for addr in unique_addrs:
    source_info = addr_to_line.get(addr)
    if not source_info:
      source_info = [(None, None)]
    if addr in addr_to_objdump:
      (object_symbol, object_offset) = addr_to_objdump.get(addr)
      object_symbol_with_offset = FormatSymbolWithOffset(object_symbol,
                                                         object_offset)
    else:
      object_symbol_with_offset = None
    result[addr] = [(source_symbol, source_location, object_symbol_with_offset)
        for (source_symbol, source_location) in source_info]

  return result


class MemoizedForSet(object):
  def __init__(self, fn):
    self.fn = fn
    self.cache = {}

  def __call__(self, lib, unique_addrs):
    lib_cache = self.cache.setdefault(lib, {})

    no_cache = filter(lambda x: x not in lib_cache, unique_addrs)
    if no_cache:
      lib_cache.update((k, None) for k in no_cache)
      result = self.fn(lib, no_cache)
      if result:
        lib_cache.update(result)

    return dict((k, lib_cache[k]) for k in unique_addrs if lib_cache[k])


@MemoizedForSet
def CallAddr2LineForSet(lib, unique_addrs):
  """Look up line and symbol information for a set of addresses.

  Args:
    lib: library (or executable) pathname containing symbols
    unique_addrs: set of string hexidecimal addresses look up.

  Returns:
    A dictionary of the form {addr: [(symbol, file:line)]} where
    each address has a list of associated symbols and locations
    or an empty list if no symbol information was found.

    If the function has been inlined then the list may contain
    more than one element with the symbols for the most deeply
    nested inlined location appearing first.
  """
  if not lib:
    return None


  symbols = SYMBOLS_DIR + lib
  if not os.path.isfile(symbols):
    return None

  (label, platform, target) = FindToolchain()
  cmd = [ToolPath("addr2line"), "--functions", "--inlines",
      "--demangle", "--exe=" + symbols]
  child = subprocess.Popen(cmd, stdin=subprocess.PIPE, stdout=subprocess.PIPE)

  result = {}
  addrs = sorted(unique_addrs)
  for addr in addrs:
    child.stdin.write("0x%s\n" % addr)
    child.stdin.flush()
    records = []
    first = True
    while True:
      symbol = child.stdout.readline().strip()
      if symbol == "??":
        symbol = None
      location = child.stdout.readline().strip()
      if location == "??:0":
        location = None
      if symbol is None and location is None:
        break
      records.append((symbol, location))
      if first:
        # Write a blank line as a sentinel so we know when to stop
        # reading inlines from the output.
        # The blank line will cause addr2line to emit "??\n??:0\n".
        child.stdin.write("\n")
        first = False
    result[addr] = records
  child.stdin.close()
  child.stdout.close()
  return result


def StripPC(addr):
  """Strips the Thumb bit a program counter address when appropriate.

  Args:
    addr: the program counter address

  Returns:
    The stripped program counter address.
  """
  global ARCH

  if ARCH == "arm":
    return addr & ~1
  return addr

@MemoizedForSet
def CallObjdumpForSet(lib, unique_addrs):
  """Use objdump to find out the names of the containing functions.

  Args:
    lib: library (or executable) pathname containing symbols
    unique_addrs: set of string hexidecimal addresses to find the functions for.

  Returns:
    A dictionary of the form {addr: (string symbol, offset)}.
  """
  if not lib:
    return None

  symbols = SYMBOLS_DIR + lib
  if not os.path.exists(symbols):
    return None

  symbols = SYMBOLS_DIR + lib
  if not os.path.exists(symbols):
    return None

  result = {}

  # Function lines look like:
  #   000177b0 <android::IBinder::~IBinder()+0x2c>:
  # We pull out the address and function first. Then we check for an optional
  # offset. This is tricky due to functions that look like "operator+(..)+0x2c"
  func_regexp = re.compile("(^[a-f0-9]*) \<(.*)\>:$")
  offset_regexp = re.compile("(.*)\+0x([a-f0-9]*)")

  # A disassembly line looks like:
  #   177b2:  b510        push  {r4, lr}
  asm_regexp = re.compile("(^[ a-f0-9]*):[ a-f0-0]*.*$")

  for target_addr in unique_addrs:
    start_addr_dec = str(StripPC(int(target_addr, 16)))
    stop_addr_dec = str(StripPC(int(target_addr, 16)) + 8)
    cmd = [ToolPath("objdump"),
           "--section=.text",
           "--demangle",
           "--disassemble",
           "--start-address=" + start_addr_dec,
           "--stop-address=" + stop_addr_dec,
           symbols]

    current_symbol = None    # The current function symbol in the disassembly.
    current_symbol_addr = 0  # The address of the current function.

    stream = subprocess.Popen(cmd, stdout=subprocess.PIPE).stdout
    for line in stream:
      # Is it a function line like:
      #   000177b0 <android::IBinder::~IBinder()>:
      components = func_regexp.match(line)
      if components:
        # This is a new function, so record the current function and its address.
        current_symbol_addr = int(components.group(1), 16)
        current_symbol = components.group(2)

        # Does it have an optional offset like: "foo(..)+0x2c"?
        components = offset_regexp.match(current_symbol)
        if components:
          current_symbol = components.group(1)
          offset = components.group(2)
          if offset:
            current_symbol_addr -= int(offset, 16)

      # Is it an disassembly line like:
      #   177b2:  b510        push  {r4, lr}
      components = asm_regexp.match(line)
      if components:
        addr = components.group(1)
        i_addr = int(addr, 16)
        i_target = StripPC(int(target_addr, 16))
        if i_addr == i_target:
          result[target_addr] = (current_symbol, i_target - current_symbol_addr)
    stream.close()

  return result


def CallCppFilt(mangled_symbol):
  cmd = [ToolPath("c++filt")]
  process = subprocess.Popen(cmd, stdin=subprocess.PIPE, stdout=subprocess.PIPE)
  process.stdin.write(mangled_symbol)
  process.stdin.write("\n")
  process.stdin.close()
  demangled_symbol = process.stdout.readline().strip()
  process.stdout.close()
  return demangled_symbol

def FormatSymbolWithOffset(symbol, offset):
  if offset == 0:
    return symbol
  return "%s+%d" % (symbol, offset)
