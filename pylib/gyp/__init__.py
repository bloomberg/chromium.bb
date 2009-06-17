#!/usr/bin/python

import copy
import gyp.input
import optparse
import os.path
import shlex
import sys


def FindBuildFiles():
  extension = '.gyp'
  files = os.listdir(os.getcwd())
  build_files = []
  for file in files:
    if file[-len(extension):] == extension:
      build_files.append(file)
  return build_files


def Load(build_files, format, default_variables={}, includes=[], depth='.'):
  """
  Loads one or more specified build files.
  default_variables and includes will be copied before use.
  Returns the generator for the specified format and the
  data returned by loading the specified build files.
  """
  default_variables = copy.copy(default_variables)

  # Default variables provided by this program and its modules should be
  # named WITH_CAPITAL_LETTERS to provide a distinct "best practice" namespace,
  # avoiding collisions with user and automatic variables.
  default_variables['GENERATOR'] = format

  generator_name = 'gyp.generator.' + format
  # These parameters are passed in order (as opposed to by key)
  # because ActivePython cannot handle key parameters to __import__.
  generator = __import__(generator_name, globals(), locals(), generator_name)
  default_variables.update(generator.generator_default_variables)

  # Fetch the generator specific info that gets fed to input, we use getattr
  # so we can default things and the generators only have to provide what
  # they need.
  generator_input_info = {
    'generator_handles_variants':
        getattr(generator, 'generator_handles_variants', False),
    'non_configuration_keys':
        getattr(generator, 'generator_additional_non_configuration_keys', []),
    'path_sections':
        getattr(generator, 'generator_additional_path_sections', []),
    'extra_sources_for_rules':
        getattr(generator, 'generator_extra_sources_for_rules', []),
  }

  # Process the input specific to this generator.
  result = gyp.input.Load(build_files, default_variables, includes[:],
                          depth, generator_input_info)
  return [generator] + result


def main(args):
  my_name = os.path.basename(sys.argv[0])

  parser = optparse.OptionParser()
  usage = 'usage: %s [options ...] [build_file ...]'
  parser.set_usage(usage.replace('%s', '%prog'))
  parser.add_option('-D', dest='defines', action='append', metavar='VAR=VAL',
                    help='sets variable VAR to value VAL')
  parser.add_option('-f', '--format', dest='formats', action='append',
                    help='output formats to generate')
  parser.add_option('--msvs-version', dest='msvs_version',
                    default='auto', help='visual studio sub-format to generate')
  parser.add_option('-I', '--include', dest='includes', action='append',
                    metavar='INCLUDE',
                    help='files to include in all loaded .gyp files')
  parser.add_option('--depth', dest='depth', metavar='PATH',
                    help='set DEPTH gyp variable to a relative path to PATH')
  parser.add_option('-S', '--suffix', dest='suffix', default='',
                    help='suffix to add to generated files')
  parser.add_option('--generator-flags', dest='generator_flags', default='',
                    help='comma separated list of flag names to pass to the '
                         'generator')

  # We read a few things from ~/.gyp, so set up a var for that.
  home_vars = ['HOME']
  if sys.platform in ('cygwin', 'win32'):
    home_vars.append('USERPROFILE')
  home = None
  for home_var in home_vars:
    home = os.getenv(home_var)
    if home != None:
      break
  home_dot_gyp = None
  if home != None:
    home_dot_gyp = os.path.join(home, '.gyp')
    if not os.path.exists(home_dot_gyp):
      home_dot_gyp = None

  # TODO(thomasvl): add support for ~/.gyp/defaults

  (options, build_files) = parser.parse_args(args)

  if not options.formats:
    # If no format was given on the command line, then check the env variable.
    generate_formats = os.environ.get('GYP_GENERATORS', [])
    if generate_formats:
      generate_formats = shlex.split(generate_formats)
    if generate_formats:
      options.formats = generate_formats
    else:
      # Nothing in the variable, default based on platform.
      options.formats = [ {'darwin': 'xcode',
                           'win32':  'msvs',
                           'cygwin': 'msvs',
                           'linux2': 'scons',}[sys.platform] ]

  if not build_files:
    build_files = FindBuildFiles()
  if not build_files:
    print >>sys.stderr, (usage + '\n\n%s: error: no build_file') % \
                        (my_name, my_name)
    return 1

  # TODO(mark): Chromium-specific hack!
  # For Chromium, the gyp "depth" variable should always be a relative path
  # to Chromium's top-level "src" directory.  If no depth variable was set
  # on the command line, try to find a "src" directory by looking at the
  # absolute path to each build file's directory.  The first "src" component
  # found will be treated as though it were the path used for --depth.
  if not options.depth:
    for build_file in build_files:
      build_file_dir = os.path.abspath(os.path.dirname(build_file))
      build_file_dir_components = build_file_dir.split(os.path.sep)
      components_len = len(build_file_dir_components)
      for index in xrange(components_len - 1, -1, -1):
        if build_file_dir_components[index] == 'src':
          options.depth = os.path.sep.join(build_file_dir_components)
          break
        del build_file_dir_components[index]

      # If the inner loop found something, break without advancing to another
      # build file.
      if options.depth:
        break

    if not options.depth:
      raise Exception, \
            'Could not automatically locate src directory.  This is a ' + \
            'temporary Chromium feature that will be removed.  Use ' + \
            '--depth as a workaround.'

  # -D on the command line sets variable defaults - D isn't just for define,
  # it's for default.  Perhaps there should be a way to force (-F?) a
  # variable's value so that it can't be overridden by anything else.
  cmdline_default_variables = {}
  defines = os.environ.get('GYP_DEFINES', [])
  if defines:
    defines = shlex.split(defines)
  if options.defines:
    defines += options.defines
  for define in defines:
    tokens = define.split('=', 1)
    if len(tokens) == 2:
      # If we can make it an int, use that, otherwise, use the string.
      try:
        token_value = int(tokens[1])
      except ValueError:
        token_value = tokens[1]
      # Set the variable to the supplied value.
      cmdline_default_variables[tokens[0]] = token_value
    else:
      # No value supplied, treat it as a boolean and set it.
      cmdline_default_variables[tokens[0]] = True

  # Set up includes.
  includes = []

  # If ~/.gyp/include.gypi exists, it'll be forcibly included into every
  # .gyp file that's loaded, before anything else is included.
  if home_dot_gyp != None:
    default_include = os.path.join(home_dot_gyp, 'include.gypi')
    if os.path.exists(default_include):
      includes.append(default_include)

  # Command-line --include files come after the default include.
  if options.includes:
    includes.extend(options.includes)

  # Generator flags should be prefixed with the target generator since they
  # are global across all generator runs.
  generator_flags = options.generator_flags.split(',')

  # Generate all requested formats (use a set in case we got one format request
  # twice)
  for format in set(options.formats):

    # Start with the default variables from the command line.
    [generator, flat_list, targets, data] = Load(build_files, format,
                                                 cmdline_default_variables,
                                                 includes, options.depth)

    params = {'options': options,
              'build_files': build_files,
              'generator_flags': generator_flags}

    # TODO(mark): Pass |data| for now because the generator needs a list of
    # build files that came in.  In the future, maybe it should just accept
    # a list, and not the whole data dict.
    # NOTE: flat_list is the flattened dependency graph specifying the order
    # that targets may be built.  Build systems that operate serially or that
    # need to have dependencies defined before dependents reference them should
    # generate targets in the order specified in flat_list.
    generator.GenerateOutput(flat_list, targets, data, params)

  # Done
  return 0


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
