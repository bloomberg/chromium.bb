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
  'OS': 'linux',
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

# C++ apps need to be linked with g++.  Not sure what's appropriate.
LD := g++
RANLIB ?= ranlib

# This is a hack; we should put these settings in the .gyp files.
PACKAGES := gtk+-2.0 nss
CFLAGS := $(CFLAGS) `pkg-config --cflags $(PACKAGES)`
LDFLAGS := $(CFLAGS) `pkg-config --libs $(PACKAGES)` -lrt

# Build output directory.
obj = obj

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

quiet_cmd_link = LINK $@
cmd_link = $(LD) $(LDFLAGS) $(LIBS) -o $@ $^

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
  if list:
    fp.write(variable + " := \\\n")
    fp.write(" \\\n".join(["\t" + prefix + l for l in list]))
    fp.write("\n\n")
  else:
    fp.write(variable + " :=\n\n")


# Map from qualified target to path to library.
libpaths = {}


def GenerateMakefile(output_filename, build_file, spec, config):
  print 'Generating %s' % output_filename

  fp = open(output_filename, 'w')

  fp.write(header)

  dir = os.path.split(output_filename)[0]

  WriteList(fp, config.get('defines'), 'DEFS', prefix='-D')
  WriteList(fp, config.get('cflags'), 'local_CFLAGS')
  WriteList(fp, config.get('include_dirs'), 'INCS', prefix='-I' + dir)
  WriteList(fp, spec.get('libraries'), 'LIBS', prefix='-l')

  sources = spec['sources']
  if sources:
    objs = map(Target, filter(Compilable, sources))
    WriteList(fp, objs, 'OBJS', prefix=dir + "/")

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
    WriteList(fp, deps, '%s: LIBS' % output)

  # Now write the actual build rule.
  fp.write('%s: $(call objectify,$(OBJS)) %s\n' % (output, ' '.join(deps)))
  if typ in ('executable', 'application'):
    fp.write('\t$(call do_cmd,link)\n')
    fp.write('# Also provide a short alias for building this executable:\n')
    fp.write('%s: %s\n' % (target, output))
  elif typ == 'static_library':
    fp.write('\t$(call do_cmd,ar)\n')
    fp.write('\t$(call do_cmd,ranlib)\n')

  fp.write('\n')
  fp.write(footer)
  fp.write('\n')

  fp.close()
  return output

def GenerateOutput(target_list, target_dicts, data):
  for build_file, build_file_dict in data.iteritems():
    if not build_file.endswith('.gyp'):
      continue

  root_makefile = open('Makefile', 'w')
  root_makefile.write(SHARED)

  for qualified_target in target_list:
    build_file, target = gyp.common.BuildFileAndTarget('',
                                                       qualified_target)[0:2]

    output_file = os.path.join(os.path.split(build_file)[0],
                               target + '.mk')

    spec = target_dicts[qualified_target]
    config = spec['configurations']['Debug']
    output = GenerateMakefile(output_file, build_file, spec, config)
    if output:
      libpaths[qualified_target] = output
    root_makefile.write('include ' + output_file + "\n")

  root_makefile.close()
