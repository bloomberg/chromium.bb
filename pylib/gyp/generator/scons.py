#!/usr/bin/python


# Skeleton for generating SConscript files from .gyp files.
#
# THIS DOES NOT WORK RIGHT NOW (2 February 2009), but is being checked in
# to capture current progress and provide a head start for next step(s)
# for possibly finishing it off.


import gyp
import gyp.common
# TODO(sgk):  create a separate "project module" for SCons?
#import gyp.SCons as SCons
import os.path
import subprocess
import re
import sys


generator_default_variables = {
  'OS': 'linux',
}


header = """\
# This file is generated; do not edit.

Import('env')
"""


def WriteList(fp, list, prefix='',
                        separator=',\n    ',
                        preamble=None,
                        postamble=None):
  fp.write(preamble or '')
  fp.write((separator or ' ').join([prefix + l for l in list]))
  fp.write(postamble or '')


def GenerateSConscript(output_filename, build_file, spec, config):
  print 'Generating %s' % output_filename

  fp = open(output_filename, 'w')

  fp.write(header)

  #
  fp.write('\n' + 'env = env.Clone()\n')
  fp.write('\n' + 'env.Append(\n')

  cflags = config.get('cflags')
  if cflags:
    WriteList(fp, map(repr, cflags), prefix='    ',
                                     preamble='    CCFLAGS = [\n    ',
                                     postamble='\n    ],\n')

  defines = config.get('defines')
  if defines:
    WriteList(fp, map(repr, defines), prefix='    ',
                                      preamble='    CPPDEFINES = [\n    ',
                                      postamble='\n    ],\n')

  include_dirs = config.get('include_dirs')
  if include_dirs:
    WriteList(fp, map(repr, include_dirs), prefix='    ',
                                           preamble='    CPPPATH = [\n    ',
                                           postamble='\n    ],\n')

  libraries = spec.get('libraries')
  if libraries:
    WriteList(fp, map(repr, libraries), prefix='    ',
                                        preamble='    LIBS = [\n    ',
                                        postamble='\n    ],\n')

  fp.write(')\n')

  sources = spec['sources']
  if sources:
    pre = '\ninput_files = ChromeFileList([\n    '
    WriteList(fp, map(repr, sources), preamble=pre, postamble=',\n])\n')

  t = spec.get('type')
  if not t in (None, 'none'):
    builder = {
                  'executable' : 'ChromeProgram',
                  'static_library' : 'ChromeStaticLibrary',
              }[t]
    fmt = 'env.%s(\'%s\', input_files)\n'
    fp.write('\n')
    fp.write(fmt % (builder, spec['target_name']))

  fp.close()


def GenerateOutput(target_list, target_dicts, data):
  for build_file, build_file_dict in data.iteritems():
    if not build_file.endswith('.gyp'):
      continue

  infix = '_gyp'
  # Uncomment to overwrite existing .scons files.
  #infix = ''

  for qualified_target in target_list:
    build_file, target = gyp.common.BuildFileAndTarget('',
                                                       qualified_target)[0:2]
    if target_list > 1:
      output_file = os.path.join(os.path.split(build_file)[0],
                                 target + infix + '.scons')
    else:
      output_file = os.path.abspath(build_file[:-4] + infix + '.scons')
    spec =  target_dicts[qualified_target]

    if not spec.has_key('libraries'):
      spec['libraries'] = []

    # Add dependent static library targets to the 'libraries' value.
    deps = spec.get('dependencies', [])
    for d in deps:
      td = target_dicts[d]
      if td['type'] == 'static_library':
        spec['libraries'].append(td['target_name'])

    # Simplest thing that works:  just use the first (Debug)
    # configuration right now, until we get the underlying
    # ../build/SConscript.main infrastructure ready for
    # .gyp-generated parallel configurations.
    config = spec['configurations'][0]

    GenerateSConscript(output_file, build_file, spec, config)
