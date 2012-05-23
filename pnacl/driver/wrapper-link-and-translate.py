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

import os
import re

from driver_env import env as Env
from driver_log import Log
import driver_tools

EXTRA_ENV = {
  'INPUTS'                 : '',
  'OUTPUT'                 : '',

  # bitcode linking flags - will be passed through to the pexe linking phase
  'RELEVANT_FLAGS'         : '',
  'GENERATE_PIC'           : '0',

  'MODE'                   : '',

  'LIB_PATHS'              : '',
  'LIBS'                   : '',

  'SONAME_FLAG'            : '',
  'OPT_FLAG'               : '',
}

RawRelevantArgs  = []

RE_PATH = r'((?:-Wl,-rpath-link,|-L|-B).+)'
RE_LIB = r'(-l.+)'

def RecordRelevantArg(category, value):
  Env.append(category, value)
  RawRelevantArgs.append(value)

def RecordInput(value):
  RecordRelevantArg('INPUTS', value)

def RecordLib(value):
  RecordRelevantArg('LIBS', value)

def RecordPath(value):
  RecordRelevantArg('LIB_PATHS', value)


ARG_PATTERNS = [
    ( ('-o','(.*)'),          "env.set('OUTPUT', $0)"),

    ( RE_PATH,                RecordPath),
    ( RE_LIB,                 RecordLib),

    ( '(-l.+)',               "env.append('RELEVANT_FLAGS', $0)"),

    ( '(-Wl,-soname,.+)',     "env.append('SONAME_FLAG', $0)"),
    ( '(-O.+)',               "env.set('OPT_FLAGS_LINK', '$0')"),

    ( '-(dynamic|shared|static)',  "env.set('MODE', $0)"),

    ( '-fPIC',                "env.set('GENERATE_PIC', '1')"),

    ( '(-.*)',                driver_tools.UnrecognizedOption),

    ( '(.*)',                 RecordInput),
]


def ProcessBuildCommand(template):
  result = []
  for t in template:
    if type(t) == str:
      s = driver_tools.env.eval(t)
      if s:
        result += [s]
    # This is the more or less standard to test for a function
    elif hasattr(t, '__call__'):
      result += t()
    else:
      Log.Fatal('do now know how to process template member %s of type %s',
                repr(t), type(t))
  return result


def DumpCommand(prefix, args):
  for a in args:
    print prefix + ": " + a

# ======================================================================
# -shared mode
# ======================================================================

def GetPsoTarget():
  out = Env.getone('OUTPUT')
  if not out.endswith('.so'):
    Log.Fatal('Unexpected target name %s', out)
  return [out[:-2] + "pso"]


def GetPsoInputs():
  return Env.get('INPUTS')


BUILD_COMMAND_PSO_FROM_BC = [
  '--pnacl-driver-verbose',
  '${OPT_FLAG}',
  '${SONAME_FLAG}',
  '-shared',
  # for now force not dt_needed in .pso/.so
  '-nodefaultlibs',
  '-o', GetPsoTarget,
  GetPsoInputs,
]


BUILD_COMMAND_SO_FROM_PSO = [
  '--pnacl-driver-verbose',
  '--newlib-shared-experiment',
  '-shared',
  '-fPIC',
  '-arch', '${ARCH}',
  '-o', '${OUTPUT}',
  GetPsoTarget,
]


def BuildSharedLib():
  # Shared libs must be built PIC
  Env.set('GENERATE_PIC', '1')
  inputs = Env.get('INPUTS')
  if len(inputs) == 0:
    Log.Fatal('No input specified.')
  if not Env.getbool('GENERATE_PIC'):
    Log.Fatal('Shared libs must be build in pic mode. Use -fPIC')
  if Env.getone('SONAME_FLAG') == '':
    Log.Fatal('Shared libs must be given a soname.')

  pso_command = ProcessBuildCommand(BUILD_COMMAND_PSO_FROM_BC)
  DumpCommand("@bc->pso", pso_command)
  # suppress_arch is needed to prevent clang to inherit the
  # -arch argument
  driver_tools.RunDriver('clang', pso_command, suppress_arch=True)

  so_command = ProcessBuildCommand(BUILD_COMMAND_SO_FROM_PSO)
  DumpCommand("@pso->so", so_command)
  driver_tools.RunDriver('translate', so_command)

# ======================================================================
# -static mode
# ======================================================================

def BuildStaticExecutable():
  Log.Fatal('PNaCl scons wrapper does not support static executables')

# ======================================================================
# -dynamic mode
# ======================================================================

def GetPexeTarget():
  out = Env.getone('OUTPUT')
  if not out.endswith('.nexe'):
    Log.Fatal('Unexpected target name %s', out)
  return [out[:-4] + "pexe"]


def GetPexeInputsDynamic():
  return RawRelevantArgs


BUILD_COMMAND_PEXE_FROM_BC = [
  '--pnacl-driver-verbose',
  '${OPT_FLAG}',
  '-o', GetPexeTarget,
  GetPexeInputsDynamic,
]


BUILD_COMMAND_NEXE_FROM_PEXE = [
  '--pnacl-driver-verbose',
  '-arch', '${ARCH}',
  '--newlib-shared-experiment',
  '-o', '${OUTPUT}',
  GetPexeTarget,
]

def BuildDynamicExecutable():
  inputs = Env.get('INPUTS')
  if len(inputs) != 1:
    Log.Fatal('You must specify exactly on input for a dynamic executable.')

  pexe_command = ProcessBuildCommand(BUILD_COMMAND_PEXE_FROM_BC)
  DumpCommand("@bc->pexe", pexe_command)
  driver_tools.RunDriver('clang', pexe_command, suppress_arch=True)

  nexe_command = ProcessBuildCommand(BUILD_COMMAND_NEXE_FROM_PEXE)
  DumpCommand("@pexe->nexe", nexe_command)
  driver_tools.RunDriver('translate', nexe_command)

# ======================================================================
#
# ======================================================================

def main(argv):
  DumpCommand("@incoming", argv)

  Env.update(EXTRA_ENV)
  driver_tools.ParseArgs(argv, ARG_PATTERNS)

  output = Env.getone('OUTPUT')

  if output == '':
    Log.Fatal('No output specified. Use -o to specify output')

  if Env.getone('ARCH') == '':
    Log.Fatal('No arch specified')

  mode = Env.getone('MODE')
  if mode == 'shared':
    BuildSharedLib()
  elif mode == 'dynamic':
    BuildDynamicExecutable()
  elif mode == 'static':
    BuildStaticExecutable()
  else:
    Log.Fatal('You must specify at least one of -static, -shared, -dynamic')
  return 0
