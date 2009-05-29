#!/usr/bin/python


import gyp
import gyp.common
# TODO(sgk):  create a separate "project module" for SCons?
#import gyp.SCons as SCons
import os.path
import pprint
import re


generator_default_variables = {
    'EXECUTABLE_PREFIX': '',
    'EXECUTABLE_SUFFIX': '',
    'INTERMEDIATE_DIR': '$OBJ_DIR/$COMPONENT_NAME/$TARGET_NAME/intermediate',
    'SHARED_INTERMEDIATE_DIR': '$OBJ_DIR/global_intermediate',
    'OS': 'linux',
    'PRODUCT_DIR': '$TOP_BUILDDIR',
    'RULE_INPUT_ROOT': '${SOURCE.filebase}',
    'RULE_INPUT_EXT': '${SOURCE.suffix}',
    'RULE_INPUT_NAME': '${SOURCE.file}',
    'RULE_INPUT_PATH': '${SOURCE}',
}


header = """\
# This file is generated; do not edit.
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


def _SCons_writer(fp, spec, builder, pre=''):
  fp.write('\n' + pre)
  fp.write('_outputs = %s\n' % builder)
  fp.write('target_files.extend(_outputs)\n')

def _SCons_null_writer(fp, spec):
  fp.write('\n')
  fp.write('target_files.extend(input_files)\n')

def _SCons_program_writer(fp, spec):
  # On Linux, an executable name like 'chrome' could be either the
  # on-disk executable or the Alias that represents the 'chrome' target.
  # Disambiguate that the program we're building is, in fact, the
  # on-disk target, so other dependent targets can't get confused.
  name = full_product_name(spec)
  pre = '_program = env.File(\'${PROGPREFIX}%s${PROGSUFFIX}\')\n' % name
  builder = 'env.ChromeProgram(_program, input_files)'
  return _SCons_writer(fp, spec, builder, pre)

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
  builder = 'env.ChromeLoadableModule(\'%s\', input_files)' % name
  return _SCons_writer(fp, spec, builder)

SConsTypeWriter = {
  None : _SCons_null_writer,
  'none' : _SCons_null_writer,
  'executable' : _SCons_program_writer,
  'static_library' : _SCons_static_library_writer,
  'shared_library' : _SCons_shared_library_writer,
  'loadable_module' : _SCons_loadable_module_writer,
}

_alias_template = """
if GetOption('verbose'):
  _action = Action([%(action)s])
else:
  _action = Action([%(action)s], %(message)s)
_outputs = env.Alias(
  ['_%(target_name)s_action'],
  %(inputs)s,
  _action
)
env.AlwaysBuild(_outputs)
"""

_command_template = """
if GetOption('verbose'):
  _action = Action([%(action)s])
else:
  _action = Action([%(action)s], %(message)s)
_outputs = env.Command(
  %(outputs)s,
  %(inputs)s,
  _action
)
"""

# This is copied from the default SCons action, updated to handle symlinks.
_copy_action_template = """
import shutil
import SCons.Action

def _copy_files_or_dirs_or_symlinks(dest, src):
  SCons.Node.FS.invalidate_node_memos(dest)
  if SCons.Util.is_List(src) and os.path.isdir(dest):
    for file in src:
      shutil.copy2(file, dest)
    return 0
  elif os.path.islink(src):
    linkto = os.readlink(src)
    os.symlink(linkto, dest)
    return 0
  elif os.path.isfile(src):
    return shutil.copy2(src, dest)
  else:
    return shutil.copytree(src, dest, 1)

def _copy_files_or_dirs_or_symlinks_str(dest, src):
  return 'Copying %s to %s ...' % (src, dest)

GYPCopy = SCons.Action.ActionFactory(_copy_files_or_dirs_or_symlinks,
                                     _copy_files_or_dirs_or_symlinks_str,
                                     convert=str)
"""

_rule_template = """
%(name)s_additional_inputs = %(inputs)s
%(name)s_outputs = %(outputs)s
def %(name)s_emitter(target, source, env):
  return (%(name)s_outputs, source + %(name)s_additional_inputs)
if GetOption('verbose'):
  %(name)s_action = Action([%(action)s])
else:
  %(name)s_action = Action([%(action)s], %(message)s)
env['BUILDERS']['%(name)s'] = Builder(action=%(name)s_action, emitter=%(name)s_emitter)
%(name)s_files = [f for f in input_files if str(f).endswith('.%(extension)s')]
for %(name)s_file in %(name)s_files:
  _outputs = env.%(name)s(%(name)s_file)
"""

_spawn_hack = """
import re
import SCons.Platform.posix
needs_shell = re.compile('["\\'><!^&]')
def gyp_spawn(sh, escape, cmd, args, env):
  def strip_scons_quotes(arg):
    if arg[0] == '"' and arg[-1] == '"':
      return arg[1:-1]
    return arg
  stripped_args = [strip_scons_quotes(a) for a in args]
  if needs_shell.search(' '.join(stripped_args)):
    return SCons.Platform.posix.exec_spawnvpe([sh, '-c', ' '.join(args)], env)
  else:
    return SCons.Platform.posix.exec_spawnvpe(stripped_args, env)
"""

escape_quotes_re = re.compile('^([^=]*=)"([^"]*)"$')
def escape_quotes(s):
    return escape_quotes_re.sub('\\1\\"\\2\\"', s)


def GenerateConfig(fp, config, indent=''):
  """
  Generates SCons dictionary items for a gyp configuration.

  This provides the main translation between the (lower-case) gyp settings
  keywords and the (upper-case) SCons construction variables.
  """
  var_mapping = {
      'ASFLAGS' : 'asflags',
      'CCFLAGS' : 'cflags',
      'CFLAGS' : 'cflags_c',
      'CXXFLAGS' : 'cflags_cc',
      'CPPDEFINES' : 'defines',
      'CPPPATH' : 'include_dirs',
      'LINKFLAGS' : 'ldflags',
      'SHLINKFLAGS' : 'ldflags',
  }
  postamble='\n%s],\n' % indent
  for scons_var in sorted(var_mapping.keys()):
      gyp_var = var_mapping[scons_var]
      value = config.get(gyp_var)
      if value:
        if gyp_var in ('defines',):
          value = [escape_quotes(v) for v in value]
        WriteList(fp,
                  map(repr, value),
                  prefix=indent,
                  preamble='%s%s = [\n    ' % (indent, scons_var),
                  postamble=postamble)


def GenerateSConscript(output_filename, spec, build_file):
  """
  Generates a SConscript file for a specific target.

  This generates a SConscript file suitable for building any or all of
  the target's configurations.

  A SConscript file may be called multiple times to generate targets for
  multiple configurations.  Consequently, it needs to be ready to build
  the target for any requested configuration, and therefore contains
  information about the settings for all configurations (generated into
  the SConscript file at gyp configuration time) as well as logic for
  selecting (at SCons build time) the specific configuration being built.

  The general outline of a generated SConscript file is:
 
    --  Header

    --  Import 'env'.  This contains a $CONFIG_NAME construction
        variable that specifies what configuration to build
        (e.g. Debug, Release).

    --  Configurations.  This is a dictionary with settings for
        the different configurations (Debug, Release) under which this
        target can be built.  The values in the dictionary are themselves
        dictionaries specifying what construction variables should added
        to the local copy of the imported construction environment
        (Append), should be removed (FilterOut), and should outright
        replace the imported values (Replace).

    --  Clone the imported construction environment and update
        with the proper configuration settings.

    --  Initialize the lists of the targets' input files and prerequisites.

    --  Target-specific actions and rules.  These come after the
        input file and prerequisite initializations because the
        outputs of the actions and rules may affect the input file
        list (process_outputs_as_sources) and get added to the list of
        prerequisites (so that they're guaranteed to be executed before
        building the target).

    --  Call the Builder for the target itself.

    --  Arrange for any copies to be made into installation directories.

    --  Set up the {name} Alias (phony Node) for the target as the
        primary handle for building all of the target's pieces.

    --  Use env.Require() to make sure the prerequisites (explicitly
        specified, but also including the actions and rules) are built
        before the target itself.

    --  Return the {name} Alias to the calling SConstruct file
        so it can be added to the list of default targets.
  """
  gyp_dir = os.path.split(output_filename)[0]
  if not gyp_dir:
      gyp_dir = '.'
  gyp_dir = os.path.abspath(gyp_dir)
  component_name = os.path.splitext(os.path.basename(build_file))[0]
  target_name = spec['target_name']

  fp = open(output_filename, 'w')
  fp.write(header)

  fp.write('\nimport os\n')
  fp.write('\nImport("env")\n')

  #
  for config in spec['configurations'].itervalues():
    if config.get('scons_line_length'):
      fp.write(_spawn_hack)
      break

  #
  indent = ' ' * 12
  fp.write('\n')
  fp.write('configurations = {\n')
  for config_name, config in spec['configurations'].iteritems():
    fp.write('    \'%s\' : {\n' % config_name)

    fp.write('        \'Append\' : dict(\n')
    GenerateConfig(fp, config, indent)
    libraries = spec.get('libraries')
    if libraries:
      WriteList(fp,
                map(repr, libraries),
                prefix=indent,
                preamble='%sLIBS = [\n    ' % indent,
                postamble='\n%s],\n' % indent)
    fp.write('        ),\n')

    fp.write('        \'FilterOut\' : dict(\n' )
    for key, var in config.get('scons_remove', {}).iteritems():
      fp.write('             %s = %s,\n' % (key, repr(var)))
    fp.write('        ),\n')

    fp.write('        \'Replace\' : dict(\n' )
    scons_settings = config.get('scons_variable_settings', {})
    for key in sorted(scons_settings.keys()):
      val = pprint.pformat(scons_settings[key])
      fp.write('             %s = %s,\n' % (key, val))
    if 'c++' in spec.get('link_languages', []):
      fp.write('             %s = %s,\n' % ('LINK', repr('$CXX')))
    if config.get('scons_line_length'):
      fp.write('             SPAWN = gyp_spawn,\n')
    fp.write('        ),\n')

    fp.write('        \'ImportExternal\' : [\n' )
    for var in config.get('scons_import_variables', []):
      fp.write('             %s,\n' % repr(var))
    fp.write('        ],\n')

    fp.write('        \'PropagateExternal\' : [\n' )
    for var in config.get('scons_propagate_variables', []):
      fp.write('             %s,\n' % repr(var))
    fp.write('        ],\n')

    fp.write('    },\n')
  fp.write('}\n')

  #
  fp.write('\n')
  fp.write('env = env.Clone(COMPONENT_NAME=%s,\n' % repr(component_name))
  fp.write('                TARGET_NAME=%s)\n' % repr(target_name))

  fp.write('\n'
           'config = configurations[env[\'CONFIG_NAME\']]\n'
           'env.Append(**config[\'Append\'])\n'
           'env.FilterOut(**config[\'FilterOut\'])\n'
           'env.Replace(**config[\'Replace\'])\n'
           'for _var in config[\'ImportExternal\']:\n'
           '  if _var in ARGUMENTS:\n'
           '    env[_var] = ARGUMENTS[_var]\n'
           '  elif _var in os.environ:\n'
           '    env[_var] = os.environ[_var]\n'
           'for _var in config[\'PropagateExternal\']:\n'
           '  if _var in ARGUMENTS:\n'
           '    env[_var] = ARGUMENTS[_var]\n'
           '  elif _var in os.environ:\n'
           '    env[\'ENV\'][_var] = os.environ[_var]\n')

  variants = spec.get('variants', {})
  for setting in sorted(variants.keys()):
    if_fmt = 'if ARGUMENTS.get(%s) not in (None, \'0\'):\n'
    fp.write('\n')
    fp.write(if_fmt % repr(setting.upper()))
    fp.write('  env.Append(\n')
    GenerateConfig(fp, variants[setting], ' '*6)
    fp.write('  )\n')

  #
  sources = spec.get('sources')
  if sources:
    pre = '\ninput_files = ChromeFileList([\n    '
    WriteList(fp, map(repr, sources), preamble=pre, postamble=',\n])\n')
  else:
    fp.write('\ninput_files = []\n')

  fp.write('\n')
  fp.write('target_files = []\n')
  prerequisites = spec.get('scons_prerequisites', [])
  fp.write('prerequisites = %s\n' % pprint.pformat(prerequisites))

  actions = spec.get('actions', [])
  for action in actions:
    a = ['cd', gyp_dir, '&&'] + action['action']
    message = action.get('message')
    if message:
      message = repr(message)
    outputs = action.get('outputs', [])
    if outputs:
      template = _command_template
    else:
      template = _alias_template
    fp.write(template % {
                 'inputs' : pprint.pformat(action.get('inputs', [])),
                 'outputs' : pprint.pformat(outputs),
                 'action' : pprint.pformat(a),
                 'message' : message,
                 'target_name': target_name,
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
  if copies:
    fp.write(_copy_action_template)
  for copy in copies:
    destdir = copy['destination']
    files = copy['files']
    fmt = ('\n'
           '_outputs = env.Command(%s,\n'
           '    %s,\n'
           '    GYPCopy(\'$TARGET\', \'$SOURCE\'))\n')
    for f in copy['files']:
      dest = os.path.join(destdir, os.path.split(f)[1])
      fp.write(fmt % (repr(dest), repr(f)))
      fp.write('target_files.extend(_outputs)\n')

  fmt = "\ngyp_target = env.Alias('%s', target_files)\n"
  fp.write(fmt % target_name)
  dependencies = spec.get('scons_dependencies', [])
  if dependencies:
    WriteList(fp, dependencies, preamble='env.Requires(gyp_target, [\n    ',
                                postamble='\n])\n')
  fp.write('env.Requires(gyp_target, prerequisites)\n')
  fp.write('Return("gyp_target")\n')

  fp.close()


#############################################################################
# TEMPLATE BEGIN

_wrapper_template = """\

__doc__ = '''
Wrapper configuration for building this entire "solution,"
including all the specific targets in various *.scons files.
'''

import os
import sys

def GetProcessorCount():
  '''
  Detects the number of CPUs on the system. Adapted form:
  http://codeliberates.blogspot.com/2008/05/detecting-cpuscores-in-python.html
  '''
  # Linux, Unix and Mac OS X:
  if hasattr(os, 'sysconf'):
    if os.sysconf_names.has_key('SC_NPROCESSORS_ONLN'):
      # Linux and Unix or Mac OS X with python >= 2.5:
      return os.sysconf('SC_NPROCESSORS_ONLN')
    else:  # Mac OS X with Python < 2.5:
      return int(os.popen2("sysctl -n hw.ncpu")[1].read())
  # Windows:
  if os.environ.has_key('NUMBER_OF_PROCESSORS'):
    return max(int(os.environ.get('NUMBER_OF_PROCESSORS', '1')), 1)
  return 1  # Default

# Support PROGRESS= to show progress in different ways.
p = ARGUMENTS.get('PROGRESS')
if p == 'spinner':
  Progress(['/\\r', '|\\r', '\\\\\\r', '-\\r'],
           interval=5,
           file=open('/dev/tty', 'w'))
elif p == 'name':
  Progress('$TARGET\\r', overwrite=True, file=open('/dev/tty', 'w'))

# Set the default -j value based on the number of processors.
SetOption('num_jobs', GetProcessorCount() + 1)

# Have SCons use its cached dependency information.
SetOption('implicit_cache', 1)

# Only re-calculate MD5 checksums if a timestamp has changed.
Decider('MD5-timestamp')

# Since we set the -j value by default, suppress SCons warnings about being
# unable to support parallel build on versions of Python with no threading.
default_warnings = ['no-no-parallel-support']
SetOption('warn', default_warnings + GetOption('warn'))

AddOption('--mode', nargs=1, dest='conf_list', default=[],
          action='append', help='Configuration to build.')

AddOption('--verbose', dest='verbose', default=False,
          action='store_true', help='Verbose command-line output.')


#
sconscript_file_map = %(sconscript_files)s

class LoadTarget:
  '''
  Class for deciding if a given target sconscript is to be included
  based on a list of included target names, optionally prefixed with '-'
  to exclude a target name.
  '''
  def __init__(self, load):
    '''
    Initialize a class with a list of names for possible loading.

    Arguments:
      load:  list of elements in the LOAD= specification
    '''
    self.included = set([c for c in load if not c.startswith('-')])
    self.excluded = set([c[1:] for c in load if c.startswith('-')])

    if not self.included:
      self.included = set(['all'])

  def __call__(self, target):
    '''
    Returns True if the specified target's sconscript file should be
    loaded, based on the initialized included and excluded lists.
    '''
    return (target in self.included or
            ('all' in self.included and not target in self.excluded))

if 'LOAD' in ARGUMENTS:
  load = ARGUMENTS['LOAD'].split(',')
else:
  load = []
load_target = LoadTarget(load)

sconscript_files = []
for target, sconscript in sconscript_file_map.iteritems():
  if load_target(target):
    sconscript_files.append(sconscript)


target_alias_list= []

conf_list = GetOption('conf_list')
if conf_list:
    # In case the same --mode= value was specified multiple times.
    conf_list = list(set(conf_list))
else:
    conf_list = ['Debug']

sconsbuild_dir = Dir(%(sconsbuild_dir)s)

src_dir = GetOption('repository')
if src_dir:
  # Deep SCons magick to support --srcdir={chromium_component}:
  # By specifying --srcdir=, a connection has already been set up
  # between our current directory (the build directory) and the
  # component source directory (base/, net/, webkit/, etc.).
  # The Chromium build is really rooted at src/, so we need to
  # repoint the repository connection to that directory.  To
  # do so and have everything just work, we must wipe out the
  # existing connection by hand, including its cached value.
  src_dir = sconsbuild_dir.repositories[0].dir
  sconsbuild_dir.clear()
  sconsbuild_dir.repositories = []
else:
  src_dir = '$SCONSBUILD_DIR/..'

for conf in conf_list:
  env = Environment(
      tools = ['ar', 'as', 'gcc', 'g++', 'gnulink', 'chromium_builders'],
      _GYP='_gyp',
      CONFIG_NAME=conf,
      LIB_DIR='$TOP_BUILDDIR/lib',
      OBJ_DIR='$TOP_BUILDDIR/obj',
      SCONSBUILD_DIR=sconsbuild_dir.abspath,
      SRC_DIR=src_dir,
      TARGET_PLATFORM='LINUX',
      TOP_BUILDDIR='$SCONSBUILD_DIR/$CONFIG_NAME',
  )
  if not GetOption('verbose'):
    env.SetDefault(
        ARCOMSTR='Creating library $TARGET',
        ASCOMSTR='Assembling $TARGET',
        CCCOMSTR='Compiling $TARGET',
        CONCATSOURCECOMSTR='ConcatSource $TARGET',
        CXXCOMSTR='Compiling $TARGET',
        LDMODULECOMSTR='Building loadable module $TARGET',
        LINKCOMSTR='Linking $TARGET',
        MANIFESTCOMSTR='Updating manifest for $TARGET',
        MIDLCOMSTR='Compiling IDL $TARGET',
        PCHCOMSTR='Precompiling $TARGET',
        RANLIBCOMSTR='Indexing $TARGET',
        RCCOMSTR='Compiling resource $TARGET',
        SHCCCOMSTR='Compiling $TARGET',
        SHCXXCOMSTR='Compiling $TARGET',
        SHLINKCOMSTR='Linking $TARGET',
        SHMANIFESTCOMSTR='Updating manifest for $TARGET',
    )
  SConsignFile(env.File('$TOP_BUILDDIR/.sconsign').abspath)

  env.Dir('$OBJ_DIR').addRepository(env.Dir('$SRC_DIR'))

  for sconscript in sconscript_files:
    target_alias = env.SConscript('$OBJ_DIR/%(subdir)s/' + sconscript,
                                  exports=['env'])
    if target_alias:
      target_alias_list.extend(target_alias)

Default(Alias('all', target_alias_list))

help_fmt = '''
Usage: hammer [SCONS_OPTIONS] [VARIABLES] [TARGET] ...

Local command-line build options:
  --mode=CONFIG             Configuration to build:
                              --mode=Debug [default]
                              --mode=Release
  --verbose                 Print actual executed command lines.

Supported command-line build variables:
  LOAD=[module,...]         Comma-separated list of components to load in the
                              dependency graph ('-' prefix excludes)
  PROGRESS=type             Display a progress indicator:
                              name:  print each evaluated target name
                              spinner:  print a spinner every 5 targets

The following TARGET names can also be used as LOAD= module names:

%%s
'''

if GetOption('help'):
  def columnar_text(items, width=78, indent=2, sep=2):
    result = []
    colwidth = max(map(len, items)) + sep
    cols = (width - indent) / colwidth
    if cols < 1:
      cols = 1
    rows = (len(items) + cols - 1) / cols
    indent = '%%*s' %% (indent, '')
    sep = indent
    for row in xrange(0, rows):
      result.append(sep)
      for i in xrange(row, len(items), rows):
        result.append('%%-*s' %% (colwidth, items[i]))
      sep = '\\n' + indent
    result.append('\\n')
    return ''.join(result)

  load_list = set(sconscript_file_map.keys())
  target_aliases = set(map(str, target_alias_list))

  common = load_list and target_aliases
  load_only = load_list - common
  target_only = target_aliases - common
  help_text = [help_fmt %% columnar_text(sorted(list(common)))]
  if target_only:
    fmt = "The following are additional TARGET names:\\n\\n%%s\\n"
    help_text.append(fmt %% columnar_text(sorted(list(target_only))))
  if load_only:
    fmt = "The following are additional LOAD= module names:\\n\\n%%s\\n"
    help_text.append(fmt %% columnar_text(sorted(list(load_only))))
  Help(''.join(help_text))
"""

# TEMPLATE END
#############################################################################


def GenerateSConscriptWrapper(build_file_data, name,
                              output_filename, sconscript_files):
  """
  Generates the "wrapper" SConscript file (analogous to the Visual Studio
  solution) that calls all the individual target SConscript files.
  """
  subdir = os.path.basename(os.path.split(output_filename)[0])
  scons_settings = build_file_data.get('scons_settings', {})
  sconsbuild_dir = scons_settings.get('sconsbuild_dir', '#')

  sconscript_file_lines = ['dict(']
  for target in sorted(sconscript_files.keys()):
    sconscript = sconscript_files[target]
    sconscript_file_lines.append('    %s = %r,' % (target, sconscript))
  sconscript_file_lines.append(')')

  fp = open(output_filename, 'w')
  fp.write(header)
  fp.write(_wrapper_template % {
               'name' : name,
               'sconsbuild_dir' : repr(sconsbuild_dir),
               'sconscript_files' : '\n'.join(sconscript_file_lines),
               'subdir' : subdir,
           })
  fp.close()

  # Generate the SConstruct file that invokes the wrapper SConscript.
  dir, fname = os.path.split(output_filename)
  SConstruct = os.path.join(dir, 'SConstruct')
  fp = open(SConstruct, 'w')
  fp.write(header)
  fp.write('SConscript(%s)\n' % repr(fname))
  fp.close()


def TargetFilename(target, build_file=None, output_suffix=''):
  """Returns the .scons file name for the specified target.
  """
  if build_file is None:
    build_file, target = gyp.common.BuildFileAndTarget('', target)[:2]
  output_file = os.path.join(os.path.split(build_file)[0],
                             target + output_suffix + '.scons')
  return output_file


def GenerateOutput(target_list, target_dicts, data, params):
  options = params['options']
  """
  Generates all the output files for the specified targets.
  """
  for build_file, build_file_dict in data.iteritems():
    if not build_file.endswith('.gyp'):
      continue

  for qualified_target in target_list:
    spec = target_dicts[qualified_target]

    if spec['type'] == 'settings':
      continue

    build_file, target = gyp.common.BuildFileAndTarget('', qualified_target)[:2]
    output_file = TargetFilename(target, build_file, options.suffix)

    if not spec.has_key('libraries'):
      spec['libraries'] = []

    # Add dependent static library targets to the 'libraries' value.
    deps = spec.get('dependencies', [])
    spec['scons_dependencies'] = []
    for d in deps:
      td = target_dicts[d]
      target_name = td['target_name']
      spec['scons_dependencies'].append("Alias('%s')" % target_name)
      if td['type'] in ('static_library', 'shared_library'):
        libname = td.get('product_name', target_name)
        spec['libraries'].append(libname)
      if td['type'] == 'loadable_module':
        prereqs = spec.get('scons_prerequisites', [])
        # TODO:  parameterize with <(SHARED_LIBRARY_*) variables?
        name = full_product_name(td, '${SHLIBPREFIX}', '${SHLIBSUFFIX}')
        prereqs.append(name)
        spec['scons_prerequisites'] = prereqs

    GenerateSConscript(output_file, spec, build_file)

  for build_file in sorted(data.keys()):
    path, ext = os.path.splitext(build_file)
    if ext != '.gyp':
      continue
    output_dir, basename = os.path.split(path)
    output_filename  = path + '_main' + options.suffix + '.scons'

    all_targets = gyp.common.AllTargets(target_list, target_dicts, build_file)
    sconscript_files = {}
    for t in all_targets:
      if target_dicts[t]['type'] == 'settings':
        continue
      bf, target = gyp.common.BuildFileAndTarget('', t)[:2]
      target_filename = TargetFilename(target, bf, options.suffix)
      tpath = gyp.common.RelativePath(target_filename, output_dir)
      sconscript_files[target] = tpath

    if sconscript_files:
      GenerateSConscriptWrapper(data[build_file], basename,
                                output_filename, sconscript_files)
