#!/usr/bin/python

# Notes:
#
# This is all roughly based on the Makefile system used by the Linux
# kernel, but is a non-recursive make -- we put the entire dependency
# graph in front of make and let it figure it out.
#
# The code below generates a separate .mk file for each target, but
# all are sourced by the top-level Makefile.  This means that all
# variables in .mk-files clobber one another.  Be careful to use :=
# where appropriate for immediate evaluation, and similarly to watch
# that you're not relying on a variable value to last beween different
# .mk files.
#
# TODOs:
#
# Global settings and utility functions are currently stuffed in the
# toplevel Makefile.  It may make sense to generate some .mk files on
# the side to keep the the files readable.
#
# Currently doesn't include build command lines as part of the deps
# of a target.  Editing a build file won't rebuild all affected files.
# See TODO near "if_changed".
#
# Should add a rule that regens the Makefiles from the gyp files.
#
# The generation is one enormous function.  Now that it works, it could
# be refactored into smaller functions.

import gyp
import gyp.common
import os.path

# Debugging-related imports -- remove me once we're solid.
import code
import pprint

generator_default_variables = {
  'EXECUTABLE_PREFIX': '',
  'EXECUTABLE_SUFFIX': '',
  'OS': 'linux',
  'INTERMEDIATE_DIR': '$(obj)/gen',
  'SHARED_INTERMEDIATE_DIR': '$(obj)/gen',
  'PRODUCT_DIR': '$(obj)/bin',
  'RULE_INPUT_ROOT': '%(INPUT_ROOT)s',  # This gets expanded by Python.
  'RULE_INPUT_PATH': '$<',

  # These appear unused -- ???
  'RULE_INPUT_EXT': 'XXXEXT$(suffix $^)',
  'RULE_INPUT_NAME': 'XXXNAME$(notdir $(basename $^)0',
}

# Header of toplevel Makefile.
# This should go into the build tree, but it's easier to keep it here for now.
SHARED_HEADER = """\
# We borrow heavily from the kernel build setup, though we are simpler since
# we don't have Kconfig tweaking settings on us.

# The implicit make rules have it looking for RCS files, among other things.
# We instead explicitly write all the rules we care about.
# It's even quicker (saves ~200ms) to pass -r on the command line.
MAKEFLAGS=-r

# The V=1 flag on command line makes us verbosely print command lines.
ifdef V
  quiet=
else
  quiet=quiet_
endif

# We build up a list of all targets so we can slurp in the generated
# dependency rule Makefiles in one pass.
all_targets :=

# C++ apps need to be linked with g++.  Not sure what's appropriate.
LD := $(CXX)
RANLIB ?= ranlib

# This is a hack; we should use the settings from the .gyp files.
PACKAGES := gtk+-2.0 nss
CFLAGS := $(CFLAGS) -m32 `pkg-config --cflags $(PACKAGES)`
LDFLAGS := $(CFLAGS) `pkg-config --libs $(PACKAGES)` -lrt

# Flags to make gcc output dependency info.  Note that you need to be
# careful here to use the flags that ccache and distcc can understand.
# We write to a temporary dep file first and then rename at the end
# so we can't end up with a broken dep file.
depfile = $@.d
DEPFLAGS = -MMD -MF $(depfile).tmp

# We have to fixup the dep file output to mention the proper .o file.
# ccache or distcc lose the path to the target, so we convert a rule of
# the form:
#   foobar.o: DEP1 DEP2
# into
#   path/to/foobar.o: DEP1 DEP2
fixup_dep = @sed -e "s|^$(notdir $@)|$@|" $(depfile).tmp > $(depfile); \
             rm -f $(depfile).tmp

# Build output directory.  TODO: make this configurable.  Maybe name it
# builddir for consistency with other make files.
obj = out

# Command definitions:
# - cmd_foo is the actual command to run;
# - quiet_cmd_foo is the brief-output summary of the command.

quiet_cmd_cc = CC $@
cmd_cc = $(CC) $(CFLAGS) $(DEPFLAGS) -c -o $@ $<

quiet_cmd_cxx = CXX $@
cmd_cxx = $(CXX) $(CXXFLAGS) $(DEPFLAGS) -c -o $@ $<

quiet_cmd_ar = AR $@
cmd_ar = $(AR) rc $@ $(filter %.o,$^)

quiet_cmd_ranlib = RANLIB $@
cmd_ranlib = $(RANLIB) $@

quiet_cmd_touch = TOUCH $@
cmd_touch = touch $@

quiet_cmd_copy = COPY $@
cmd_copy = ln -f $< $@

# Due to circular dependencies between libraries :(, we wrap the
# special "figure out circular dependencies" flags around the entire
# input list during linking.
quiet_cmd_link = LINK $@
cmd_link = $(LD) $(LDFLAGS) -o $@ -Wl,--start-group $(filter %.a %.o,$^) -Wl,--end-group $(LIBS)

# Shared-object link (for generating .so).
# TODO: perhaps this can share with the LINK command above?
quiet_cmd_solink = SOLINK $@
cmd_solink = $(LD) -shared $(LDFLAGS) -o $@ -Wl,--start-group $(filter %.a %.o,$^) -Wl,--end-group $(LIBS)

# do_cmd: run a command via the above cmd_foo names.
# TODO: This should also set up dependencies of the target on the
# command line used to generate the target (so we rebuild on CFLAGS
# change etc.).  See if_changed in the kernel source
# scripts/Kbuild.include.
do_cmd = @\
echo '  $($(quiet)cmd_$(1))'; \
mkdir -p $(dir $@); \
$(cmd_$(1))

# Suffix rules, putting all outputs into $(obj).
$(obj)/%.o: %.c
	$(call do_cmd,cc)
	$(fixup_dep)

$(obj)/%.o: %.s
	$(call do_cmd,cc)

$(obj)/%.o: %.cpp
	$(call do_cmd,cxx)
	$(fixup_dep)
$(obj)/%.o: %.cc
	$(call do_cmd,cxx)
	$(fixup_dep)
$(obj)/%.o: %.cxx
	$(call do_cmd,cxx)
	$(fixup_dep)

# Try building from generated source, too.
$(obj)/%.o: $(obj)/%.cc
	$(call do_cmd,cxx)
	$(fixup_dep)
$(obj)/%.o: $(obj)/%.cpp
	$(call do_cmd,cxx)
	$(fixup_dep)

"""

SHARED_FOOTER = """\
# Add in dependency-tracking rules.  $(all_targets) is all targets in
# our tree.  First, only consider targets that already have been
# built, as unbuilt targets will be built regardless of dependency
# info:
all_targets := $(wildcard $(sort $(all_targets)))
# Of those, only consider the ones with .d (dependency) info:
d_files := $(wildcard $(foreach f,$(all_targets),$(f).d))
ifneq ($(d_files),)
  include $(d_files)
endif
"""

header = """\
# This file is generated by gyp; do not edit.

"""

footer = """\
"""


def Compilable(filename):
  """Return true if the file is compilable (should be in OBJS)."""
  return any(filename.endswith(e) for e in ['.c', '.cc', '.cpp', '.cxx', '.s'])


def Target(filename):
  """Translate a compilable filename to its .o target."""
  return os.path.splitext(filename)[0] + '.o'


def QuoteIfNecessary(string):
  if '"' in string:
    string = '"' + string.replace('"', '\\"') + '"'
  return string


def WriteList(fp, list, variable=None,
                        prefix=''):
  fp.write(variable + " := ")
  if list:
    fp.write(" \\\n\t".join([QuoteIfNecessary(prefix + l) for l in list]))
  fp.write("\n\n")


# Map from qualified target to path to output.
target_outputs = {}


def Absolutify(dir, path):
  """Convert a subdirectory-relative path into a base-relative path.
  Skips over paths that contain variables."""
  if '$(' in path:
    return path
  return os.path.normpath(os.path.join(dir, path))


def AbsolutifyL(dir, paths):
  """Absolutify lifted to lists."""
  return [Absolutify(dir, p) for p in paths]


def FixupArgPath(dir, arg):
  if '/' in arg or '.h.' in arg:
    return Absolutify(dir, arg)
  return arg


def FixupArgPathL(dir, args):
  return [FixupArgPath(dir, arg) for arg in args]


def Objectify(path):
  """Convert a path to its output directory form."""
  if '$(' in path:
    return path
  return '$(obj)/' + path


def GenerateMakefile(output_filename, build_file, root, spec, config):
  print 'Generating %s' % output_filename

  fp = open(output_filename, 'w')
  fp.write(header)

  # Paths in gyp files are relative to the .gyp file, but we need
  # paths relative to the source root for the master makefile.  Grab
  # the path of the .gyp file as the base to relativize against.
  dir = gyp.common.RelativePath(os.path.split(output_filename)[0], root)
  target = spec['target_name']

  special_outputs = []
  extra_sources = []

  # Actions must come first, since they can generate more OBJs for use below.
  if 'actions' in spec:
    for action in spec['actions']:
      name = target + '_' + action['action_name']
      fp.write('# Rules for action "%s":\n' % action['action_name'])
      inputs = AbsolutifyL(dir, action['inputs'])
      outputs = AbsolutifyL(dir, action['outputs'])

      # Build up a list of outputs.
      # Collect the output dirs we'll need.
      dirs = set()
      for out in outputs:
        dirs.add(os.path.split(out)[0])
      if action.get('process_outputs_as_sources', False):
        extra_sources += outputs

      # Stuff the outputs in a variable so we can refer to them later.
      outputs_variable = 'action_%s_outputs' % name
      fp.write('%s := %s\n' % (outputs_variable, ' '.join(outputs)))
      special_outputs.append('$(%s)' % outputs_variable)

      # Write the actual command.
      command = gyp.common.EncodePOSIXShellList(FixupArgPathL(dir, action['action']))
      if 'message' in action:
        fp.write('quiet_cmd_%s = ACTION %s $@\n' % (name, action['message']))
      else:
        fp.write('quiet_cmd_%s = ACTION %s $@\n' % (name, name))
      if len(dirs) > 0:
        command = 'mkdir -p %s' % ' '.join(dirs) + '; ' + command
      fp.write('cmd_%s = %s\n' % (name, command))

      # We want to run the action once to generate all the files.
      # We make the first output run the action, and the other outputs
      # depend on the first.
      fp.write('%s: %s\n' % (outputs[0], ' '.join(inputs)))
      fp.write("\t$(call do_cmd,%s)\n" % name)
      if len(outputs) > 1:
        fp.write('%s: %s\n' % (' '.join(outputs[1:]), outputs[0]))
      fp.write('\n')
    fp.write('\n')

  # Rules must be early like actions.
  if 'rules' in spec:
    for rule in spec['rules']:
      name = target + '_' + rule['rule_name']
      fp.write('# Generated for rule %s:\n' % name)

      all_outputs = []

      dirs = set()
      for rule_source in rule['rule_sources']:
        rule_source_basename = os.path.basename(rule_source)
        (rule_source_root, rule_source_ext) = \
            os.path.splitext(rule_source_basename)

        outputs = map(lambda out: out % { 'INPUT_ROOT': rule_source_root }, rule['outputs'])
        for out in outputs:
          dirs.add(os.path.split(out)[0])
          if rule.get('process_outputs_as_sources', False):
            extra_sources.append(out)
        all_outputs += outputs
        output = outputs[0]
        inputs = ' '.join(AbsolutifyL(dir, [rule_source] + rule.get('inputs', [])))
        fp.write("""\
%(output)s: %(inputs)s
\t$(call do_cmd,%(name)s)\n""" % locals())
        if len(outputs) > 0:
          fp.write('%s: %s\n' % (' '.join(outputs[1:]), outputs[0]))

      fp.write('\n')

      outputs_variable = 'rule_%s_outputs' % name
      WriteList(fp, all_outputs, outputs_variable)
      special_outputs.append('$(%s)' % outputs_variable)

      mkdirs = ''
      if len(dirs) > 0:
        mkdirs = 'mkdir -p %s; ' % ' '.join(dirs)
      fp.write("cmd_%(name)s = %(mkdirs)s%(action)s\n" % {
        'mkdirs': mkdirs,
        'name': name,
        'action': gyp.common.EncodePOSIXShellList(FixupArgPathL(dir, rule['action']))
      })
      fp.write('quiet_cmd_%(name)s = RULE %(name)s $@\n' % {
        'name': name,
      })
      fp.write('\n')
    fp.write('\n')

  if 'copies' in spec:
    fp.write('# Generated for copy rule.\n')

    variable = target + '_copies'
    outputs = []
    for copy in spec['copies']:
      for path in copy['files']:
        filename = os.path.split(path)[1]
        output = os.path.join(copy['destination'], filename)
        fp.write('%s: %s\n\t$(call do_cmd,copy)\n' % (output, path))
        outputs.append(output)
    fp.write('%s = %s\n' % (variable, ' '.join(outputs)))
    special_outputs.append('$(%s)' % variable)
    fp.write('\n')

  WriteList(fp, config.get('defines'), 'DEFS', prefix='-D')
  WriteList(fp, config.get('cflags'), 'local_CFLAGS')
  includes = config.get('include_dirs')
  if includes:
    includes = AbsolutifyL(dir, includes)
    includes = sorted(list(set(includes)))  # Uniquify.
  WriteList(fp, includes, 'INCS', prefix='-I')
  WriteList(fp, spec.get('libraries'), 'LIBS')

  objs = None
  if 'sources' in spec:
    sources = spec['sources']
    if extra_sources:
      sources += extra_sources
    sources = filter(Compilable, sources)
    objs = map(Objectify, AbsolutifyL(dir, map(Target, sources)))
  WriteList(fp, objs, 'OBJS')
  # Make sure the actions and rules run first.
  fp.write("targets += $(OBJS)\n")

  output = None
  typ = spec.get('type')
  if typ == 'static_library':
    target = 'lib%s.a' % target
  elif typ == 'loadable_module':
    target = 'lib%s.so' % target
  elif typ == 'none':
    target = '%s.stamp' % target
  elif typ == 'settings':
    pass  # We'll fix up at end.
  elif typ == 'executable':
    target = spec.get('product_name', target)
  else:
    print "ERROR: What output file should be generated?", "typ", typ, "target", target
  fp.write('\n')
  output = os.path.join('$(obj)', dir, target)

  if objs:
    fp.write("""\
# CFLAGS et al overrides must be target-local.
# See "Target-specific Variable Values" in the GNU Make manual.
%(output)s: CFLAGS := $(CFLAGS) $(local_CFLAGS) $(DEFS) $(INCS)
%(output)s: CXXFLAGS := $(CFLAGS) $(local_CFLAGS) $(DEFS) $(INCS)
%(output)s: LIBS := $(LIBS)
""" % locals())

  deps = []
  if 'dependencies' in spec:
    deps = [target_outputs[dep] for dep in spec['dependencies']
            if target_outputs[dep]]
  if special_outputs:
    deps += special_outputs

  if deps:
    fp.write('# Make sure our dependencies are built before any of us.\n')
    fp.write('$(OBJS): ' + ' '.join(deps) + '\n')

  # Now write the actual build rule.
  fp.write('\n# Rules for final target.\n')
  deps = ' '.join(deps)
  if typ == 'executable':
    binpath = '$(obj)/bin/' + target
    fp.write("""\
%(output)s: $(OBJS) %(deps)s
\t$(call do_cmd,link)
%(binpath)s: %(output)s
\t$(call do_cmd,copy)

# Also provide a short alias for building this executable; e.g., "make %(target)s".
%(target)s: %(binpath)s""" % locals())
  elif typ == 'static_library':
    fp.write("""\
%(output)s: $(OBJS) %(deps)s
\t$(call do_cmd,ar)
\t$(call do_cmd,ranlib)""" % locals())
  elif typ == 'loadable_module':
    fp.write("""\
%(output)s: $(OBJS) %(deps)s
\t$(call do_cmd,solink)""" % locals())
  elif typ == 'none':
    # Write a stamp line.
    fp.write("%(output)s: $(OBJS) %(deps)s\n" % locals())
    fp.write("\t$(call do_cmd,touch)\n")
  else:
    #if typ not in ('executable', 'application', 'none', 'static_library'):
    if typ != 'settings':
      print "WARNING: no output for", typ, target

  fp.write('\n')
  fp.write(footer)
  fp.write('\n')

  fp.close()
  if typ == 'settings':
    # Settings have no output.
    return None
  return output

def GenerateOutput(target_list, target_dicts, data, params):
  options = params['options']
  root_makefile = open('Makefile', 'w')
  root_makefile.write(SHARED_HEADER)

  for qualified_target in target_list:
    build_file, target = gyp.common.BuildFileAndTarget('', qualified_target)[:2]
    output_file = os.path.join(os.path.split(build_file)[0], target + '.mk')

    spec = target_dicts[qualified_target]
    configs = spec['configurations']
    config = None
    if 'Debug' in configs:
      config = configs['Debug']
    elif 'Default' in configs:
      config = configs['Default']

    output = GenerateMakefile(output_file, build_file, options.depth,
                              spec, config)
    target_outputs[qualified_target] = output
    root_makefile.write('include ' + output_file + "\n")

  root_makefile.write(SHARED_FOOTER)

  root_makefile.close()
