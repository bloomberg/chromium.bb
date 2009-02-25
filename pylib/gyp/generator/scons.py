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
import os
import pprint
import re


generator_default_variables = {
    'EXECUTABLE_PREFIX': '',
    'EXECUTABLE_SUFFIX': '',
    'INTERMEDIATE_DIR': '$DESTINATION_ROOT/obj/intermediate',
    'SHARED_INTERMEDIATE_DIR': '$DESTINATION_ROOT/obj/global_intermediate',
    'OS': 'linux',
    'PRODUCT_DIR': '$DESTINATION_ROOT',
    'RULE_INPUT_ROOT': '${SOURCE.filebase}',
    'RULE_INPUT_EXT': '${SOURCE.suffix}',
    'RULE_INPUT_NAME': '${SOURCE.file}',
    'RULE_INPUT_PATH': '${SOURCE}',
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


def _SCons_null_writer(fp, spec):
  pass

def _SCons_program_writer(fp, spec):
  fmt = '\nenv.ChromeProgram(\'%s\', input_files)\n'
  fp.write(fmt % spec['target_name'])

def _SCons_static_library_writer(fp, spec):
  fmt = '\nenv.ChromeStaticLibrary(\'%s\', input_files)\n'
  fp.write(fmt % spec['target_name'])

_resource_preamble = """
import sys
sys.path.append(env.Dir('$CHROME_SRC_DIR/tools/grit').abspath)
env.Tool('scons', toolpath=[env.Dir('$CHROME_SRC_DIR/tools/grit/grit')])
#  This dummy target is used to tell the emitter where to put the target files.
env.GRIT('$TARGET_ROOT/grit_derived_sources/%s',
         [
"""

def _SCons_resource_writer(fp, spec):
  grd = spec['sources'][0]
  dummy = os.path.splitext(os.path.split(grd)[1])[0] + '.dummy'
  WriteList(fp, map(repr, spec['sources']),
                prefix='           ',
                separator=',\n',
                preamble=_resource_preamble % dummy,
                postamble=',\n         ])\n')

SConsTypeWriter = {
  None : _SCons_null_writer,
  'none' : _SCons_null_writer,
  'application' : _SCons_program_writer,
  'executable' : _SCons_program_writer,
  'resource' : _SCons_resource_writer,
  'static_library' : _SCons_static_library_writer,
}

_command_template = """
_outputs = env.Command(
  %(outputs)s,
  %(inputs)s,
  [%(action)s],
)
"""

_rule_template = """
%(name)s_additional_inputs = %(inputs)s
%(name)s_outputs = %(outputs)s
def %(name)s_emitter(target, source, env):
  return (%(name)s_outputs, source + %(name)s_additional_inputs)
%(name)s_action = Action([%(action)s])
env['BUILDERS']['%(name)s'] = Builder(action=%(name)s_action, emitter=%(name)s_emitter)
%(name)s_files = [f for f in input_files if str(f).endswith('.%(extension)s')]
for %(name)s_file in %(name)s_files:
  _outputs = env.%(name)s(%(name)s_file)
"""

escape_quotes_re = re.compile('^([^=]*=)"([^"]*)"$')
def escape_quotes(s):
    return escape_quotes_re.sub('\\1\\"\\2\\"', s)

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
    defines = [escape_quotes(d) for d in defines]
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

  sources = spec.get('sources')
  if sources:
    pre = '\ninput_files = ChromeFileList([\n    '
    WriteList(fp, map(repr, sources), preamble=pre, postamble=',\n])\n')

  actions = spec.get('actions',[])
  for action in actions:
    fp.write(_command_template % {
                 'inputs' : pprint.pformat(action.get('inputs', [])),
                 'outputs' : pprint.pformat(action.get('outputs', [])),
                 'action' : pprint.pformat(action['action']),
             })
    if action.get('process_outputs_as_sources'):
      fp.write('input_files.extend(_outputs)\n')

  rules = spec.get('rules', [])
  for rule in rules:
    name = rule['rule_name']
    fp.write(_rule_template % {
                 'inputs' : pprint.pformat(rule.get('inputs', [])),
                 'outputs' : pprint.pformat(rule.get('outputs', [])),
                 'action' : pprint.pformat(rule['action']),
                 'extension' : rule['extension'],
                 'name' : name,
             })
    if rule.get('process_outputs_as_sources'):
      fp.write('  input_files.Replace(%s_file, _outputs)\n' % name)

  SConsTypeWriter[spec.get('type')](fp, spec)

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

    # Simplest thing that works:  just use the Debug
    # configuration right now, until we get the underlying
    # ../build/SConscript.main infrastructure ready for
    # .gyp-generated parallel configurations.
    config = spec['configurations']['Debug']

    GenerateSConscript(output_file, build_file, spec, config)
