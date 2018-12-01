#!/usr/bin/env python
# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import os
import re
import subprocess
import sys

import common

# If something adds a static initializer, revert it, don't increase these
# numbers. We don't accept regressions in static initializers.
EXPECTED_LINUX_SI_COUNTS = {
    'chrome': 4,
    'nacl_helper': 4,
    'nacl_helper_bootstrap': 0,
}

EXPECTED_MAC_SI_COUNT = 0


def run_process(command):
  p = subprocess.Popen(command, stdout=subprocess.PIPE)
  stdout = p.communicate()[0]
  if p.returncode != 0:
    raise Exception(
        'ERROR from command "%s": %d' % (' '.join(command), p.returncode))
  return stdout


def main_mac(src_dir):
  base_names = ('Chromium', 'Google Chrome')
  ret = 0
  for base_name in base_names:
    app_bundle = base_name + '.app'
    framework_name = base_name + ' Framework'
    framework_bundle = framework_name + '.framework'
    framework_dsym_bundle = framework_bundle + '.dSYM'
    framework_unstripped_name = framework_name + '.unstripped'
    chromium_executable = os.path.join(app_bundle, 'Contents', 'MacOS',
                                       base_name)
    chromium_framework_executable = os.path.join(framework_bundle,
                                                 framework_name)
    chromium_framework_dsym = os.path.join(framework_dsym_bundle, 'Contents',
                                           'Resources', 'DWARF', framework_name)
    if os.path.exists(chromium_executable):
      # Count the number of files with at least one static initializer.
      si_count = 0
      # Find the __DATA,__mod_init_func section.
      stdout = run_process(['otool', '-l', chromium_framework_executable])
      section_index = stdout.find('sectname __mod_init_func')
      if section_index != -1:
        # If the section exists, the "size" line must follow it.
        initializers_s = re.search('size 0x([0-9a-f]+)',
                                   stdout[section_index:]).group(1)
        word_size = 8  # Assume 64 bit
        si_count = int(initializers_s, 16) / word_size

      # Print the list of static initializers.
      if si_count > EXPECTED_MAC_SI_COUNT:
        print('Expected <= %d static initializers in %s, but found %d' %
              (EXPECTED_MAC_SI_COUNT, chromium_framework_executable, si_count))
        ret = 1

        # First look for a dSYM to get information about the initializers. If
        # one is not present, check if there is an unstripped copy of the build
        # output.
        mac_tools_path = os.path.join(src_dir, 'tools', 'mac')
        if os.path.exists(chromium_framework_dsym):
          dump_static_initializers = os.path.join(
              mac_tools_path, 'dump-static-initializers.py')
          stdout = run_process(
              [dump_static_initializers, chromium_framework_dsym])
          print stdout
        else:
          show_mod_init_func = os.path.join(mac_tools_path,
                                            'show_mod_init_func.py')
          args = [show_mod_init_func]
          if os.path.exists(framework_unstripped_name):
            args.append(framework_unstripped_name)
          else:
            print '# Warning: Falling back to potentially stripped output.'
            args.append(chromium_framework_executable)
          stdout = run_process(args)
          print stdout
  return ret


def main_linux(src_dir):

  def get_elf_section_size(readelf_stdout, section_name):
    # Matches: .ctors PROGBITS 000000000516add0 5169dd0 000010 00 WA 0 0 8
    match = re.search(r'\.%s.*$' % re.escape(section_name), readelf_stdout,
                      re.MULTILINE)
    if not match:
      return (False, -1)
    size_str = re.split(r'\W+', match.group(0))[5]
    return (True, int(size_str, 16))

  def get_word_size(binary_name):
    stdout = run_process(['readelf', '-h', binary_name])
    elf_class_line = re.search('Class:.*$', stdout, re.MULTILINE).group(0)
    elf_class = re.split(r'\W+', elf_class_line)[1]
    if elf_class == 'ELF32':
      return 4
    elif elf_class == 'ELF64':
      return 8
    raise Exception('Unsupported architecture')

  ret = 0
  for binary_name in EXPECTED_LINUX_SI_COUNTS:
    if not os.path.exists(binary_name):
      continue
    # NOTE: this is very implementation-specific and makes assumptions
    # about how compiler and linker implement global static initializers.
    si_count = 0
    stdout = run_process(['readelf', '-SW', binary_name])
    has_init_array, init_array_size = get_elf_section_size(stdout, 'init_array')
    if has_init_array:
      si_count = init_array_size / get_word_size(binary_name)
      # In newer versions of gcc crtbegin.o inserts frame_dummy into .init_array
      # but we don't want to count this entry, since its always present and not
      # related to our code.
      stdout = run_process(['objdump', '-t', binary_name, '-j' '.init_array'])
      if '__frame_dummy_init_array_entry' in stdout:
        si_count -= 1

    # Print the list of static initializers.
    if (binary_name in EXPECTED_LINUX_SI_COUNTS and
        si_count > EXPECTED_LINUX_SI_COUNTS[binary_name]):
      print('Expected <= %d static initializers in %s, but found %d' %
            (EXPECTED_LINUX_SI_COUNTS[binary_name], binary_name, si_count))
      ret = 1
    if si_count > 0:
      dump_static_initializers = os.path.join(src_dir, 'tools', 'linux',
                                              'dump-static-initializers.py')
      stdout = run_process([dump_static_initializers, '-d', binary_name])
      print '\n# Static initializers in %s:' % binary_name
      print stdout
  return ret


def main_run(args):
  if args.build_config_fs != 'Release':
    raise Exception('Only release builds are supported')

  src_dir = args.paths['checkout']
  build_dir = os.path.join(src_dir, 'out', args.build_config_fs)
  os.chdir(build_dir)

  if sys.platform.startswith('darwin'):
    rc = main_mac(src_dir)
  elif sys.platform == 'linux2':
    rc = main_linux(src_dir)
  else:
    sys.stderr.write('Unsupported platform %s.\n' % repr(sys.platform))
    return 2

  json.dump({
      'valid': rc == 0,
      'failures': [],
  }, args.output)

  return rc


def main_compile_targets(args):
  if sys.platform.startswith('darwin'):
    compile_targets = ['chrome']
  elif sys.platform == 'linux2':
    compile_targets = ['chrome', 'nacl_helper', 'nacl_helper_bootstrap']
  else:
    compile_targets = []

  json.dump(compile_targets, args.output)

  return 0


if __name__ == '__main__':
  funcs = {
      'run': main_run,
      'compile_targets': main_compile_targets,
  }
  sys.exit(common.run_script(sys.argv[1:], funcs))
