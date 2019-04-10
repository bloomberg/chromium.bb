#!/usr/bin/env python2

import sys
import re
import gc

# Purpose: The purpose of this tool is to generate a MSVS style map file from
# a combination of dumpbin-style map and a llvm-style map.  Some in-house
# tools that parse these map files require them to be MSVS formatted.  These
# parsers are very sensitive to lexical tokens so even the slightest format
# changes can break the parser.  These mynute requirements will be noted with
# comments.

# The following two functions parse a dumpbin-style map file and extract only
# the necessary information needed during the construction of the MSVS style
# map file.
def parse_dumpbin_line(dumpbin_map, match):
  if match:
    rva = int(match.group(2), 16)
    dumpbin_map[rva] = {
      'addr': match.group(1),
      'sym': match.group(3),
    }

def read_dumpbin_map(dumpbin_map, filename):
  c_pattern = re.compile(
    '([0-9a-f]{4}:[0-9a-f]{8})' + # address
    ' +' +                        # [whitespace]
    '([0-9a-f]{8,16})' +          # RVA
    ' +' +                        # [whitespace]
    '[0-9a-f]{8,16}' +            # (unknown)
    ' +' +                        # [whitespace]
    '[0-9]+'                      # size
    ' +' +                        # [whitespace]
    '([^ ]{2,})' +                # symbol
    '\\Z',                        # [end of string]
    re.IGNORECASE)

  cpp_pattern = re.compile(
    '([0-9a-f]{4}:[0-9a-f]{8})' + # address
    ' +' +                        # [whitespace]
    '([0-9a-f]{8,16})' +          # RVA
    ' +' +                        # [whitespace]
    '[0-9a-f]{8,16}' +            # (unknown)
    ' +' +                        # [whitespace]
    '[0-9]+'                      # size
    ' +' +                        # [whitespace]
    '[^ ]{2,}' +                  # mangled symbol
    ' +' +                        # [whitespace]
    '[(]([^)]+)[)]' +             # demangled symbol
    '\\Z',                        # [end of string]
    re.IGNORECASE)

  with open(filename, 'r') as fp:
    line = fp.readline()
    while line:
      stripped_line = line.strip()
      parse_dumpbin_line(
          dumpbin_map,
          c_pattern.match(stripped_line) or cpp_pattern.match(stripped_line))
      line = fp.readline()

# The following three functions parse a llvm-style map file and extract only
# necessary information needed during the construction of the MSVS style
# map file.
def parse_llvm_object_line(match):
  if match:
    return match.group(1)

def parse_llvm_symbol_line(llvm_map, match, object_name):
  if match:
    rva = int(match.group(1), 16)
    symbol = match.group(2)

    if rva not in llvm_map:
      llvm_map[rva] = {
        'sym': symbol,
        'obj': object_name
      }
    else:
      llvm_map[rva]['sym'] = '<optimized>'

def read_llvm_map(llvm_map, filename):
  object_pattern = re.compile(
    '[0-9a-f]{8,16}' +      # RVA
    ' +' +                  # [whitespace]
    '[0-9a-f]{8,16}' +      # size
    ' +' +                  # [whitespace]
    '[0-9]+'                # alignment
    ' +' +                  # [whitespace]
    '([^ ]{2,}[.]obj):.*' + # object name
    '\\Z',                  # [end of string]
    re.IGNORECASE)

  c_symbol_pattern = re.compile(
    '([0-9a-f]{8,16})' + # RVA
    ' +' +               # [whitespace]
    '[0-9a-f]{8,16}' +   # size
    ' +' +               # [whitespace]
    '[0-9]+'             # alignment
    ' {17}' +            # [whitespace]
    '([^ ]{2,})' +       # symbol
    '\\Z',               # [end of string]
    re.IGNORECASE)

  cpp_symbol_pattern = re.compile(
    '([0-9a-f]{8,16})' + # RVA
    ' +' +               # [whitespace]
    '[0-9a-f]{8,16}' +   # size
    ' +' +               # [whitespace]
    '[0-9]+'             # alignment
    ' {17}' +            # [whitespace]
    '["]([^"]+)["]' +    # demangled symbol
    ' +' +               # [whitespace]
    '[(][^ ]{2,}[)]' +   # mangled symbol
    '\\Z',               # [end of string]
    re.IGNORECASE)

  with open(filename, 'r') as fp:
    last_object_name = '<unknown>'
    line = fp.readline()

    while line:
      stripped_line = line.strip()
      object_name = parse_llvm_object_line(object_pattern.match(stripped_line))

      if object_name:
        last_object_name = object_name
      else:
        parse_llvm_symbol_line(
            llvm_map,
            c_symbol_pattern.match(stripped_line) or cpp_symbol_pattern.match(stripped_line),
            last_object_name)

      line = fp.readline()

# merge_map consolidates the data collected from the dumpbin-style map file
# and the llvm-style map file into a merged dataset
def merge_maps(merged_map, dumpbin_map, llvm_map):
  # dumpbin_map provides 'addr', 'sym'
  # llvm_map provides 'sym', obj'

  # Import dumpbin_map and fill in the missing obj field from the llvm_map
  for rva, mapping in dumpbin_map.iteritems():
    merged_map[rva] = {
      'addr': mapping['addr'],
      'sym': mapping['sym'],
      'obj': llvm_map[rva]['obj'] if rva in llvm_map else None
    }

  # Import llvm_map and assume 'addr' to be 0.  This value is ignored by the
  # formentioned "in-house" tools
  for rva, mapping in llvm_map.iteritems():
    if rva not in merged_map:
      merged_map[rva] = {
        'addr': '0000:00000000',
        'sym': mapping['sym'],
        'obj': mapping['obj']
      }

# The following two functions consume the merged dataset to format and
# generate a dataset that can be used to write a MSVS-style map
def format_object_name(
    raw_object_name, general_pattern, nativeproj_pattern, symbol):
  object_name = ''

  if not raw_object_name:
    if '@' in symbol:
      object_name = 'kernel32:KERNEL32.dll'
    elif symbol.startswith('___safe_se_handler_'):
      # Reference:
      # https://github.com/llvm-mirror/lld/blob/master/COFF/Driver.cpp#L1500
      object_name = '<linker-defined>'
    else:
      object_name = '<common>'
  else:
    if raw_object_name.startswith('obj/'):
      match = general_pattern.match(raw_object_name)
      object_name = match.group(1)
    elif '\\minkernel\\crts\\crtw32\\' in raw_object_name:
      match = general_pattern.match(raw_object_name)
      object_name = 'libcmt:' + match.group(1)
    elif 'nativeproj' in raw_object_name:
      match = nativeproj_pattern.match(raw_object_name)
      object_name = match.group(1) + ':' + match.group(2)
    elif '\\minkernel\\crts\\ucrt\\' in raw_object_name:
      match = general_pattern.match(raw_object_name)
      object_name = 'libucrt:' + match.group(1)
    elif '\\dxguid\\' in raw_object_name:
      match = general_pattern.match(raw_object_name)
      object_name = 'dxguid:' + match.group(1)
    elif '\\portabledeviceguids\\' in raw_object_name:
      match = general_pattern.match(raw_object_name)
      object_name = 'portabledeviceguids:' + match.group(1)
    elif '\\strmiids\\' in raw_object_name:
      match = general_pattern.match(raw_object_name)
      object_name = 'strmiids:' + match.group(1)
    elif 'sensorsapi' in raw_object_name:
      match = general_pattern.match(raw_object_name)
      object_name = 'sensorsapi:' + match.group(1)
    elif '\\shell\\' in raw_object_name:
      match = general_pattern.match(raw_object_name)
      object_name = 'shell32:' + match.group(1)
    elif '\\netio\\' in raw_object_name:
      match = general_pattern.match(raw_object_name)
      object_name = 'ws2_32:' + match.group(1)
    elif '\\mf\\' in raw_object_name:
      match = general_pattern.match(raw_object_name)
      object_name = 'mfuuid:' + match.group(1)
    elif ('idl' in raw_object_name or 'uuid' in raw_object_name or
          'guid' in raw_object_name or symbol.startswith('_IID_') or
          symbol.startswith('_CLSID_') or symbol.startswith('_LIBID_')):
      match = general_pattern.match(raw_object_name)
      object_name = 'uuid:' + match.group(1)
    else:
      object_name = raw_object_name

  return object_name

def generate_msvs_map(msvs_map, merged_map):
  general_object_pattern = re.compile('.*[\\\\/](.*[.]obj)')
  nativeproj_object_pattern = re.compile(
          '.*\\\\([^\\\\]*).nativeproj.*[\\\\/](.*[.]obj)')

  rva_list = sorted(merged_map)

  for rva in rva_list:
    mapping = merged_map[rva]
    symbol = mapping['sym']
    obj = mapping['obj']

    msvs_map.append({
      'addr': mapping['addr'],
      'rva': rva,
      'sym': symbol,
      'obj': format_object_name(
              obj, general_object_pattern, nativeproj_object_pattern, symbol)
    })

# Write the MSVS style map to standard out
def write_msvs_map(msvs_map):
  # Parser requires: '.+Preferred load address is [0-9]{8,16}'
  # This is the base address of the image file.
  #
  # In the MSVS formatted map, each row contains the virtual address of the
  # function.  In the dumpbin formatted map, each row contains the RVA of the
  # function.  Since the parser only cares about the RVA value, we specify a
  # base address of 0.  This allows the parser to subtract 0 from the
  # "virtual address" to get the RVA.
  print("Fake Preferred load address is 00000000\n")
  print(" {0}         {1}              {2}       {3}\n".format(
    "Address",
    "Publics by Value",
    "Rva+Base",
    "Lib:Object"));

  for mapping in msvs_map:
    print(' {0}       {1: <26} {2}     {3}'.format(
        mapping['addr'],
        mapping['sym'],
        format(mapping['rva'], '08x'),
        mapping['obj']))

def main(args):
  llvm_map_filename = None
  dumpbin_map_filename = None

  for i in range(len(args[1:])):
    if args[i] == '--llvm-map':
      llvm_map_filename = args[i+1]
    elif args[i] == '--dumpbin-map':
      dumpbin_map_filename = args[i+1]
    elif args[i].startswith('-'):
      print(
        "Usage: {0} --llvm-map <input> --dumpbin-map <input>".format(args[0]))
      return 1

  if not llvm_map_filename or not dumpbin_map_filename:
    print("Usage: {0} --llvm-map <input> --dumpbin-map <input>".format(args[0]))
    return 1

  dumpbin_map = {}
  read_dumpbin_map(dumpbin_map, dumpbin_map_filename)
  gc.collect()

  llvm_map = {}
  read_llvm_map(llvm_map, llvm_map_filename)
  gc.collect()

  merged_map = {}
  merge_maps(merged_map, dumpbin_map, llvm_map)
  gc.collect()

  msvs_map = []
  generate_msvs_map(msvs_map, merged_map)
  gc.collect()

  write_msvs_map(msvs_map)


if __name__ == '__main__':
  sys.exit(main(sys.argv))

