#!/usr/bin/python

# Notes:
#
# This is all roughly based on the Makefile system used by the Linux
# kernel.
#
# The code below generates a separate .mk file for each target, but
# all are sourced by the top-level Makefile.  This means that all
# variables in .mk-files clobber one another.  Be careful to use :=
# where appropriate for immediate evaluation.
#
# Global settings and utility functions are currently stuffed in the
# toplevel Makefile.  It may make sense to generate some .mk files on
# the side to keep the the files readable.
#
# Dependencies don't work (yet).  The XCode and Visual Studio
# generators do the dependency analysis among the source files
# basically for free.  Make doesn't.  For a build object foo/bar.o,
# the kernel build creates foo/.bar.o.d which contains the dependency
# graph.  We can do this; it just hasn't been implemented yet.

import gyp
import gyp.common
import os.path
import pprint

generator_default_variables = {
  'EXECUTABLE_PREFIX': '',
  'EXECUTABLE_SUFFIX': '',
  'OS': 'linux',
  'INTERMEDIATE_DIR': '$(obj)/geni',  # pick a wacky name because i don't understand it
  'SHARED_INTERMEDIATE_DIR': '$(obj)/gen',
  'PRODUCT_DIR': '$(obj)/bin',
  'RULE_INPUT_ROOT': 'wot',
  'RULE_INPUT_EXT': 'wot',
  'RULE_INPUT_NAME': 'wot',
  'RULE_INPUT_PATH': 'wot',
}

# Header of toplevel Makefile.
# This should go into the build tree, but it's easier to keep it here for now.
SHARED = """\
# We borrow heavily from the kernel build setup, though we are simpler since
# we don't have Kconfig tweaking settings on us.

# The V=1 flag on command line makes us verbosely print command lines.
ifdef V
  quiet=
else
  quiet=quiet_
endif

# Leave these in here for now to simplify iteration on testing.
CC := ccache distcc
CXX := ccache distcc g++

# C++ apps need to be linked with g++.  Not sure what's appropriate.
LD := g++
RANLIB ?= ranlib

# This is a hack; we should put these settings in the .gyp files.
PACKAGES := gtk+-2.0 nss
CFLAGS := $(CFLAGS) -m32 `pkg-config --cflags $(PACKAGES)`
LDFLAGS := $(CFLAGS) `pkg-config --libs $(PACKAGES)` -lrt

# Build output directory.
obj = out

# Command definitions:
# - cmd_foo is the actual command to run;
# - quiet_cmd_foo is the brief-output summary of the command.

quiet_cmd_cc = CC $@
cmd_cc = $(CC) $(CFLAGS) -c -o $@ $<

quiet_cmd_cxx = CXX $@
cmd_cxx = $(CXX) $(CXXFLAGS) -c -o $@ $<

quiet_cmd_ar = AR $@
cmd_ar = $(AR) rc $@ $^

quiet_cmd_ranlib = RANLIB $@
cmd_ranlib = $(RANLIB) $@

# We wrap the special "figure out circular dependencies" flags around the
# entire input list during linking.
quiet_cmd_link = LINK $@
cmd_link = $(LD) $(LDFLAGS) -o $@ -Wl,--start-group $^ -Wl,--end-group

# do_cmd: run a command via the above cmd_foo names.
# This should also set up dependencies of the target on the command line
# used to generate the target (so we rebuild on CFLAGS change etc.) as
# well as manage header dependencies.
# See if_changed in the kernel source scripts/Kbuild.include.
do_cmd = @\
echo " " $($(quiet)cmd_$(1)); \
mkdir -p $(dir $@); \
$(cmd_$(1))

# objectify: Prepend $(obj)/ to a list of paths.
objectify = $(foreach o,$(1),$(obj)/$(o))

$(obj)/%.o: %.c
	$(call do_cmd,cc)

$(obj)/%.o: %.s
	$(call do_cmd,cc)

$(obj)/%.o: %.cpp
	$(call do_cmd,cxx)

$(obj)/%.o: %.cc
	$(call do_cmd,cxx)

"""

header = """\
# This file is generated; do not edit.

"""

footer = """\
"""


def Compilable(filename):
  """Return true if the file is compilable (should be in OBJS)."""
  return any(filename.endswith(e) for e in ['.c', '.cc', '.cpp', '.cxx', '.s'])


def Target(filename):
  """Translate a compilable filename to its .o target."""
  return os.path.splitext(filename)[0] + '.o'


def WriteList(fp, list, variable=None,
                        prefix=''):
  fp.write(variable + " := ")
  if list:
    fp.write(" \\\n\t".join([prefix + l for l in list]))
  fp.write("\n\n")


# Map from qualified target to path to library.
libpaths = {}


def Absolutify(dir, path):
  """Convert a subdirectory-relative path into a base-relative path.
  Skips over paths that contain variables."""
  if '$(' in path:
    return path
  return os.path.normpath(os.path.join(dir, path))


def AbsolutifyL(dir, paths):
  """Absolutify lifted to lists."""
  return [Absolutify(dir, p) for p in paths]


def Objectify(path):
  """Convert a path to its output directory form."""
  if '$(' in path:
    return path
  return '$(obj)/' + path


def GenerateMakefile(output_filename, build_file, spec, config):
  print 'Generating %s' % output_filename

  fp = open(output_filename, 'w')
  fp.write(header)

  # Paths in gyp files are relative to the .gyp file, but we need
  # paths relative to the source root for the master makefile.  Grab
  # the path of the .gyp file as the base to relativize against.
  dir = os.path.split(output_filename)[0]

  WriteList(fp, config.get('defines'), 'DEFS', prefix='-D')
  WriteList(fp, config.get('cflags'), 'local_CFLAGS')
  includes = config.get('include_dirs')
  if includes:
    includes = AbsolutifyL(dir, includes)
    includes = sorted(list(set(includes)))  # Uniquify.
  WriteList(fp, includes, 'INCS', prefix='-I')
  WriteList(fp, spec.get('libraries'), 'LIBS', prefix='-l')

  objs = None
  if 'sources' in spec:
    sources = spec['sources']
    objs = map(Objectify, AbsolutifyL(dir, map(Target, filter(Compilable, sources))))
  WriteList(fp, objs, 'OBJS')

  output = None
  typ = spec.get('type')
  target = spec['target_name']
  if typ == 'static_library':
    target = 'lib%s.a' % spec['target_name']
  fp.write('\n')
  output = os.path.join('$(obj)', dir, target)
  # CFLAGS et al overrides must be target-local.
  # See "Target-specific Variable Values" in the GNU Make manual.
  fp.write('%s: CFLAGS := $(CFLAGS) $(local_CFLAGS) $(DEFS) $(INCS)\n' % output)
  fp.write('%s: CXXFLAGS := $(CFLAGS) $(local_CFLAGS) $(DEFS) $(INCS)\n' % output)
  deps = []
  if 'dependencies' in spec:
    deps = [libpaths[dep] for dep in spec['dependencies']]
    libs = filter(lambda dep: dep.endswith('.a'), deps)
    diff = set(libs) - set(deps)
    if diff:
      print "Warning: filtered out", diff, "from deps list."
    deps = libs

  # Now write the actual build rule.
  if typ == 'executable':
    fp.write('%s: $(OBJS) %s\n' % (output, ' '.join(deps)))
    fp.write('\t$(call do_cmd,link)\n')
    binpath = '$(obj)/bin/' + target
    fp.write('%s: %s\n' % (binpath, output))
    fp.write('\tmkdir -p $(obj)/bin\n')
    fp.write('\tln %s %s\n' % (output, binpath))
    fp.write('# Also provide a short alias for building this executable:\n')
    fp.write('%s: %s\n' % (target, binpath))
  elif typ == 'static_library':
    fp.write('%s: $(OBJS) %s\n' % (output, ' '.join(deps)))
    fp.write('\t$(call do_cmd,ar)\n')
    fp.write('\t$(call do_cmd,ranlib)\n')
  elif typ == 'resource':
    if target == 'net_resources':
      # Hack around SCons-specific net.gyp file.
      fp.write('.PHONY: %s\n' % output)
      fp.write('$(obj)/gen/net_resources.h: tools/grit/grit.py\n')
      fp.write('\tpython tools/grit/grit.py -i net/base/net_resources.grd build -o $(obj)/gen\n')
      fp.write('net_resources: $(obj)/gen/net_resources.h\n')

  if 'actions' in spec:
    for action in spec['actions']:
      name = target + '_' + action['action_name']
      fp.write('\n# Rules for action %s:\n' % name)
      inputs = AbsolutifyL(dir, action['inputs'])
      outputs = AbsolutifyL(dir, action['outputs'])
      if target == 'js2c':
        # Hack around SCons-specific v8.gyp file.
        action['action'] = ['python', inputs[0]] + outputs + ['CORE'] + inputs[1:]
      # Make a phony target with the name of this target.
      fp.write('.PHONY: %s\n' % name)
      # Make the outputs depend on the phony target.
      dirs = set()
      for out in outputs:
        if '../grit_derived' in out:
          # Hack around SCons-specific gyp file.
          out = out.replace('../grit_derived_sources', '$(out)/gen')
        fp.write('%s: %s\n' % (out, name))
        dirs.add(os.path.split(out)[0])
      # Make the phony target depend on the inputs and generate the outputs.
      fp.write('%s: %s\n' % (name, ' '.join(inputs)))
      if len(dirs) > 0:
        fp.write('\tmkdir -p %s\n' % ' '.join(dirs))
      fp.write('\t%s\n' % ' '.join(action['action']))

  if typ not in ('executable', 'resource', 'none', 'static_library'):
    raise "unhandled typ", typ

  fp.write('\n')
  fp.write(footer)
  fp.write('\n')

  fp.close()
  return output

def GenerateOutput(target_list, target_dicts, data, params):
  for build_file, build_file_dict in data.iteritems():
    if not build_file.endswith('.gyp'):
      continue

  root_makefile = open('Makefile', 'w')
  root_makefile.write(SHARED)

  for qualified_target in target_list:
    build_file, target = gyp.common.BuildFileAndTarget('',
                                                       qualified_target)[0:2]

    output_file = os.path.join(os.path.split(build_file)[0],
                               target + params['options'].suffix + '.mk')

    spec = target_dicts[qualified_target]
    config = spec['configurations']['Debug']
    output = GenerateMakefile(output_file, build_file, spec, config)
    if output:
      libpaths[qualified_target] = output
    root_makefile.write('include ' + output_file + "\n")

  root_makefile.close()
