#!/usr/bin/python

import gyp.input
import optparse
import os.path
import sys


def FindBuildFiles():
  extension = '.gyp'
  files = os.listdir(os.getcwd())
  build_files = []
  for file in files:
    if file[-len(extension):] == extension:
      build_files.append(file)
  return build_files


def main(args):
  my_name = os.path.basename(sys.argv[0])

  parser = optparse.OptionParser()
  usage = 'usage: %s [-D var=val ...] [-f format] [build_file ...]'
  parser.set_usage(usage.replace('%s', '%prog'))
  parser.add_option('-D', dest='defines', action='append', metavar='VAR=VAL',
                    help='sets variable VAR to value VAL')
  parser.add_option('-f', '--format', dest='format',
                    help='output format to generate')
  (options, build_files) = parser.parse_args(args)
  if not options.format:
    options.format = {'darwin': 'xcode',
                      'win32':  'msvs',
                      'cygwin': 'msvs'}[sys.platform]
  if not build_files:
    build_files = FindBuildFiles()
  if not build_files:
    print >>sys.stderr, (usage + '\n\n%s: error: no build_file') % \
                        (my_name, my_name)
    return 1

  default_variables = {}

  # -D on the command line sets variable defaults - D isn't just for define,
  # it's for default.  Perhaps there should be a way to force (-F?) a
  # variable's value so that it can't be overridden by anything else.
  if options.defines:
    for define in options.defines:
      tokens = define.split('=', 1)
      if len(tokens) == 2:
        # Set the variable to the supplied value.
        default_variables[tokens[0]] = tokens[1]
      else:
        # No value supplied, treat it as a boolean and set it.
        default_variables[tokens[0]] = True

  # Default variables provided by this program and its modules should be
  # named WITH_CAPITAL_LETTERS to provide a distinct "best practice" namespace,
  # avoiding collisions with user and automatic variables.
  default_variables['GENERATOR'] = options.format

  generator_name = 'gyp.generator.' + options.format
  # These parameters are passed in order (as opposed to by key)
  # because ActivePython cannot handle key parameters to __import__.
  generator = __import__(generator_name, globals(), locals(), generator_name)
  default_variables.update(generator.generator_default_variables)

  [flat_list, targets, data] = gyp.input.Load(build_files, default_variables)

  # TODO(mark): Pass |data| for now because the generator needs a list of
  # build files that came in.  In the future, maybe it should just accept
  # a list, and not the whole data dict.
  # NOTE: flat_list is the flattened dependency graph specifying the order
  # that targets may be built.  Build systems that operate serially or that
  # need to have dependencies defined before dependents reference them should
  # generate targets in the order specified in flat_list.
  generator.GenerateOutput(flat_list, targets, data)
  return 0


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
