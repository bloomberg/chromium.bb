#!/usr/bin/env python3

# Copyright (C) 2021 Bloomberg L.P. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

# This script is based on cef/tools/gn_args.py, and contains code copied
# from it.  That file carries the following license:
# Copyright 2016 The Chromium Embedded Framework Authors. Portions copyright
# 2012 Google Inc. All rights reserved. Use of this source code is governed by
# a BSD-style license that can be found in the LICENSE file.


import argparse
import os
import shlex
import subprocess
import sys


SCRIPT_DIR = os.path.dirname(os.path.realpath(__file__))
SRC_DIR = os.path.normpath(os.path.join(SCRIPT_DIR,
                                        os.path.pardir,
                                        os.path.pardir))


def name_value_list_to_dict(name_value_list):
  """
  Takes an array of strings of the form 'NAME=VALUE' and creates a dictionary
  of the pairs. If a string is simply NAME, then the value in the dictionary
  is set to True. If VALUE can be converted to a boolean or integer, it is.
  """
  result = {}
  for item in name_value_list:
    tokens = item.split('=', 1)
    if len(tokens) == 2:
      token_value = tokens[1]
      if token_value.lower() == 'true':
        token_value = True
      elif token_value.lower() == 'false':
        token_value = False
      else:
        # If we can make it an int, use that, otherwise, use the string.
        try:
          token_value = int(token_value)
        except ValueError:
          pass
      # Set the variable to the supplied value.
      result[tokens[0]] = token_value
    else:
      # No value supplied, treat it as a boolean and set it.
      result[tokens[0]] = True
  return result


def shlex_env(env_name):
  """
  Split an environment variable using shell-like syntax.
  """
  flags = os.environ.get(env_name, [])
  if flags:
    flags = shlex.split(flags)
  return flags


def merge_dicts(*dict_args):
  """
  Given any number of dicts, shallow copy and merge into a new dict.
  Precedence goes to key value pairs in latter dicts.
  """
  result = {}
  for dictionary in dict_args:
    result.update(dictionary)
  return result


def get_value_string(val):
  """
  Return the string representation of |val| expected by GN.
  """
  if isinstance(val, bool):
    if val:
      return 'true'
    else:
      return 'false'
  elif isinstance(val, int):
    return val
  else:
    return '"%s"' % val
  return val


def get_config_file_contents(config):
  """
  Generate config file contents for the config.
  """
  pairs = []
  for k in sorted(config.keys()):
    pairs.append("%s=%s" % (k, get_value_string(config[k])))
  return "\n".join(pairs)


def get_chromium_version():
  with open(os.path.join(SRC_DIR, 'chrome', 'VERSION'), encoding='utf-8') as f:
    data = f.read()
  lines = [x[6:] for x in data.splitlines()]  # strip MAJOR=,MINOR=,etc
  return '.'.join(lines)


def get_config(is_component_mode, is_debug_mode, cpu, defines):
  initial_config = {
    # Disable PGO by default because it requires profiling data.  When
    # profiling data is added to .nongit files, then remove this setting.
    'chrome_pgo_phase': 0,

    # Enable proprietary codecs.
    'proprietary_codecs': True,
    'ffmpeg_branding': 'Chrome',

    # Apply the content shell version.
    'content_shell_version': get_chromium_version(),
  }
  gn_env_config = name_value_list_to_dict(shlex_env('GN_DEFINES'))
  cmdline_config = name_value_list_to_dict(defines)

  config = merge_dicts(initial_config, gn_env_config, cmdline_config)

  if config.get('is_official_build', False):
    if is_component_mode:
      # Official builds do not work in component mode. This restriction is
      # enforced in //BUILD.gn.
      return None

    if is_debug_mode:
      # Cannot create is_official_build=true is_debug=true builds.
      # This restriction is enforced in //build/config/BUILDCONFIG.gn.
      # Instead, our "official Debug" build is a Release build with dchecks and
      # symbols. Symbols will be generated by default for official builds; see
      # definition of 'symbol_level' in //build/config/compiler/compiler.gni.
      is_debug_mode = False
      config['dcheck_always_on'] = True

  if is_debug_mode and not is_component_mode:
    # debug mode with non-component mode is not supported due to very large
    # debug files.
    return None

  config['is_component_build'] = is_component_mode
  config['is_debug'] = is_debug_mode
  config['target_cpu'] = cpu
  return config


def get_gn():
  return os.path.join(SRC_DIR, 'buildtools', 'win', 'gn.exe')


summary = ('''Generate ninja build output directories

    This script generates the ninja build output directories based on 3
    parameters:
      * component mode (shared libraries vs static libraries)
      * debug mode (debug or release)
      * target cpu (x86 or x64)

    Accordingly, it generates the following directories by default:
      * out/shared_debug
      * out/shared_debug64
      * out/shared_release
      * out/shared_release64
      * out/static_release
      * out/static_release64

    Static builds are more performant during runtime but their linking time is
    quite long.  Debug builds include more debugging information into the build
    but produces much less efficient code compared to release builds.  64-bit
    builds require the embedder to be 64-bit as well.

    Note that static debug builds are not generated because the debug
    information is too large and therefore not supported.

    Individual parameters can be turned off using the flags.  For example,
    specifying the --no-x64 flag will suppress the x64 build directories from
    being generated.
    ''').rstrip()

example_text = ('''EXAMPLES
    To generate all build directories with default options:
      {prog}

    To generate release build directories only:
      {prog} --no-debug

    To generate official build directories:
      {prog} -D is_official_build=true

    To generate the static release x86 official build directory:
      {prog} -D is_official_build=true --no-debug --no-x64

    Note that the shared directory is automatically excluded because component
    builds are not supported in official build mode, so specifying --no-shared
    is not necessary.

    Multiple -D flags may be used as follows:
      {prog} -D is_official_build=true -D proprietary_codecs=false
    '''.format(prog='gn.py')).rstrip()

parser = argparse.ArgumentParser(description=summary,
                                 epilog=example_text,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
parser.add_argument('-o', '--out', default='out', help='Out directory')
parser.add_argument('--shared', action=argparse.BooleanOptionalAction,
                    default=True, help='Generate shared (component) config')
parser.add_argument('--static', action=argparse.BooleanOptionalAction,
                    default=True, help='Generate static config')
parser.add_argument('--debug', action=argparse.BooleanOptionalAction,
                    default=True, help='Generate debug config')
parser.add_argument('--release', action=argparse.BooleanOptionalAction,
                    default=True, help='Generate release config')
parser.add_argument('--x86', action=argparse.BooleanOptionalAction,
                    default=True, help='Generate x86 config')
parser.add_argument('--x64', action=argparse.BooleanOptionalAction,
                    default=True, help='Generate x64 config')
parser.add_argument('-D', '--define', action='append', default=[],
                    help='Define GN var=value')
args = parser.parse_args(sys.argv[1:])

component_modes = []
debug_modes = []
cpus = []

if args.shared:
  component_modes.append(True)
if args.static:
  component_modes.append(False)
if args.debug:
  debug_modes.append(True)
if args.release:
  debug_modes.append(False)
if args.x86:
  cpus.append('x86')
if args.x64:
  cpus.append('x64')

for is_component_mode in component_modes:
  for is_debug_mode in debug_modes:
    for cpu in cpus:
      config = get_config(is_component_mode, is_debug_mode, cpu, args.define)
      if config is None:
        continue
      out_dir = os.path.join(SRC_DIR,
                             args.out,
                             '%s_%s%s' % (
                               'shared' if is_component_mode else 'static',
                               'debug' if is_debug_mode else 'release',
                               '64' if 'x64' == cpu else ''))
      print('Preparing %s...' % os.path.relpath(out_dir))
      sys.stdout.flush()
      os.makedirs(out_dir, exist_ok=True)
      with open(os.path.join(out_dir, 'args.gn'), 'w', encoding='utf-8') as f:
        f.write(get_config_file_contents(config))

      # Run gn
      cmd = [
          get_gn(),
          'gen',
          '--script-executable=' + sys.executable,
          os.path.relpath(out_dir, SRC_DIR),
      ]
      if 'GN_ARGUMENTS' in os.environ.keys():
        cmd.extend(os.environ['GN_ARGUMENTS'].split(' '))
      subprocess.run(cmd, check=True, cwd=SRC_DIR)
