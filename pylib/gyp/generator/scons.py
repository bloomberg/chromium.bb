#!/usr/bin/python


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


def full_product_name(spec, prefix='', suffix=''):
  name = spec.get('product_name') or spec['target_name']
  name = prefix + name + suffix
  product_dir = spec.get('product_dir')
  if product_dir:
    name = os.path.join(product_dir, name)
  return name


def _SCons_null_writer(fp, spec):
  pass

def _SCons_writer(fp, spec, builder):
  fp.write('\n_outputs = %s\n' % builder)
  fp.write('target_files.extend(_outputs)\n')

def _SCons_program_writer(fp, spec):
  name = full_product_name(spec)
  builder = 'env.ChromeProgram(\'%s\', input_files)' % name
  return _SCons_writer(fp, spec, builder)

def _SCons_static_library_writer(fp, spec):
  name = full_product_name(spec)
  builder = 'env.ChromeStaticLibrary(\'%s\', input_files)' % name
  return _SCons_writer(fp, spec, builder)

def _SCons_shared_library_writer(fp, spec):
  name = full_product_name(spec)
  builder = 'env.ChromeSharedLibrary(\'%s\', input_files)' % name
  return _SCons_writer(fp, spec, builder)

def _SCons_loadable_module_writer(fp, spec):
  name = full_product_name(spec)
  # Note:  LoadableModule() isn't part of the Hammer API, and there's no
  # ChromeLoadableModule() wrapper for this, so use base SCons for now.
  builder = 'env.LoadableModule(\'%s\', input_files)' % name
  return _SCons_writer(fp, spec, builder)

SConsTypeWriter = {
  None : _SCons_null_writer,
  'none' : _SCons_null_writer,
  'application' : _SCons_program_writer,
  'executable' : _SCons_program_writer,
  'static_library' : _SCons_static_library_writer,
  'shared_library' : _SCons_shared_library_writer,
  'loadable_module' : _SCons_loadable_module_writer,
}

_command_template = """
_outputs = env.Command(
  %(outputs)s,
  %(inputs)s,
  Action([%(action)s], %(message)s),
)
"""

_rule_template = """
%(name)s_additional_inputs = %(inputs)s
%(name)s_outputs = %(outputs)s
def %(name)s_emitter(target, source, env):
  return (%(name)s_outputs, source + %(name)s_additional_inputs)
%(name)s_action = Action([%(action)s], %(message)s)
env['BUILDERS']['%(name)s'] = Builder(action=%(name)s_action, emitter=%(name)s_emitter)
%(name)s_files = [f for f in input_files if str(f).endswith('.%(extension)s')]
for %(name)s_file in %(name)s_files:
  _outputs = env.%(name)s(%(name)s_file)
"""

escape_quotes_re = re.compile('^([^=]*=)"([^"]*)"$')
def escape_quotes(s):
    return escape_quotes_re.sub('\\1\\"\\2\\"', s)

def GenerateSConscript(output_filename, spec, config):
  print 'Generating %s' % output_filename

  gyp_dir = os.path.split(output_filename)[0]
  if not gyp_dir:
      gyp_dir = '.'

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

  # Allow removal of flags (e.g. -Werror) and other values from the
  # default settings.
  scons_remove = config.get('scons_remove')
  if scons_remove:
    fp.write('\n')
    fp.write('env.FilterOut(\n')
    for key, var in scons_remove.iteritems():
      fp.write('    %s = %s\n' % (key, repr(var)))
    fp.write(')\n')

  sources = spec.get('sources')
  if sources:
    pre = '\ninput_files = ChromeFileList([\n    '
    WriteList(fp, map(repr, sources), preamble=pre, postamble=',\n])\n')
  else:
    fp.write('\ninput_files = []\n')

  fp.write('\ntarget_files = []\n')

  prerequisites = spec.get('scons_prerequisites', [])
  fp.write('\nprerequisites = %s\n' % pprint.pformat(prerequisites))

  actions = spec.get('actions', [])
  for action in actions:
    a = ['cd', gyp_dir, '&&'] + action['action']
    message = action.get('message')
    if message:
        message = repr(message)
    fp.write(_command_template % {
                 'inputs' : pprint.pformat(action.get('inputs', [])),
                 'outputs' : pprint.pformat(action.get('outputs', [])),
                 'action' : pprint.pformat(a),
                 'message' : message,
             })
    if action.get('process_outputs_as_sources'):
      fp.write('input_files.extend(_outputs)\n')
    fp.write('prerequisites.extend(_outputs)\n')

  rules = spec.get('rules', [])
  for rule in rules:
    name = rule['rule_name']
    a = ['cd', gyp_dir, '&&'] + rule['action']
    message = rule.get('message')
    if message:
        message = repr(message)
    fp.write(_rule_template % {
                 'inputs' : pprint.pformat(rule.get('inputs', [])),
                 'outputs' : pprint.pformat(rule.get('outputs', [])),
                 'action' : pprint.pformat(a),
                 'extension' : rule['extension'],
                 'name' : name,
                 'message' : message,
             })
    if rule.get('process_outputs_as_sources'):
      fp.write('  input_files.Replace(%s_file, _outputs)\n' % name)
    fp.write('prerequisites.extend(_outputs)\n')

  SConsTypeWriter[spec.get('type')](fp, spec)

  copies = spec.get('copies', [])
  for copy in copies:
    destdir = copy['destination']
    files = copy['files']
    fmt = '\n_outputs = env.Install(%s,\n    %s\n)\n'
    fp.write(fmt % (repr(destdir), pprint.pformat(files)))
    fp.write('prerequisites.extend(_outputs)\n')

  fmt = "\ngyp_target = env.Alias('gyp_target_%s', target_files)\n"
  fp.write(fmt % spec['target_name'])
  dependencies = spec.get('scons_dependencies', [])
  if dependencies:
    WriteList(fp, dependencies, preamble='env.Requires(gyp_target, [\n    ',
                                postamble='\n])\n')
  fp.write('env.Requires(gyp_target, prerequisites)\n')
  fp.write('Return("gyp_target")\n')

  fp.close()


_wrapper_template = """\

__doc__ = '''
Wrapper configuration for building this entire "solution,"
including all the specific targets in various *.scons files.
'''

sconscript_files = %(sconscript_files)s

target_alias_list= []
for sconscript in sconscript_files:
    target_alias = env.SConscript(sconscript, exports=['env'])
    if target_alias:
      target_alias_list.extend(target_alias)

Default(Alias('%(name)s', target_alias_list))
"""

def GenerateSConscriptWrapper(name, output_filename, sconscript_files):
  print 'Generating %s' % output_filename
  fp = open(output_filename, 'w')
  fp.write(header)
  fp.write(_wrapper_template % {
               'name' : name,
               'sconscript_files' : pprint.pformat(sconscript_files),
           })
  fp.close()


def TargetFilename(target, output_suffix):
  """Returns the .scons file name for the specified target.
  """
  build_file, target = gyp.common.BuildFileAndTarget('', target)[:2]
  output_file = os.path.join(os.path.split(build_file)[0],
                             target + output_suffix + '.scons')
  return output_file


def GenerateOutput(target_list, target_dicts, data, options):
  for build_file, build_file_dict in data.iteritems():
    if not build_file.endswith('.gyp'):
      continue

  for qualified_target in target_list:
    spec = target_dicts[qualified_target]
    output_file = TargetFilename(qualified_target, options.suffix)

    if not spec.has_key('libraries'):
      spec['libraries'] = []

    # Add dependent static library targets to the 'libraries' value.
    deps = spec.get('dependencies', [])
    spec['scons_dependencies'] = []
    for d in deps:
      td = target_dicts[d]
      targetname = td['target_name']
      spec['scons_dependencies'].append("Alias('gyp_target_%s')" % targetname)
      if td['type'] in ('static_library', 'shared_library'):
        libname = td.get('product_name') or targetname
        spec['libraries'].append(libname)
      if td['type'] == 'loadable_module':
        prereqs = spec.get('scons_prerequisites', [])
        # TODO:  parameterize with <(SHARED_LIBRARY_*) variables?
        name = full_product_name(td, '${SHLIBPREFIX}', '${SHLIBSUFFIX}')
        prereqs.append(name)
        spec['scons_prerequisites'] = prereqs

    # Simplest thing that works:  just use the Debug
    # configuration right now, until we get the underlying
    # ../build/SConscript.main infrastructure ready for
    # .gyp-generated parallel configurations.
    config = spec['configurations']['Debug']

    GenerateSConscript(output_file, spec, config)

  for build_file in sorted(data.keys()):
    path, ext = os.path.splitext(build_file)
    if ext != '.gyp':
      continue
    output_dir, basename = os.path.split(path)
    output_filename  = path + '_main' + options.suffix + '.scons'

    all_targets = gyp.common.AllTargets(target_list, target_dicts, build_file)
    sconscript_files = []
    for t in all_targets:
      t = gyp.common.RelativePath(TargetFilename(t, options.suffix),
                                  output_dir)
      sconscript_files.append(t)
    sconscript_files.sort()

    GenerateSConscriptWrapper(basename, output_filename, sconscript_files)
