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
  'PRODUCT_DIR': '$(builddir)',
  'RULE_INPUT_ROOT': '%(INPUT_ROOT)s',  # This gets expanded by Python.
  'RULE_INPUT_PATH': '$<',

  # These appear unused -- ???
  'RULE_INPUT_EXT': 'XXXEXT$(suffix $^)',
  'RULE_INPUT_NAME': 'XXXNAME$(notdir $(basename $^)0',
}

# Header of toplevel Makefile.
# This should go into the build tree, but it's easier to keep it here for now.
SHARED_HEADER = ("""\
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

# Specify BUILDTYPE=Release on the command line for a release build.
BUILDTYPE ?= Debug

# Directory all our build output goes into.
# Note that this must be two directories beneath src/ for unit tests to pass,
# as they reach into the src/ directory for data with relative paths.
builddir ?= """ + os.getcwd() + """/out/$(BUILDTYPE)

# Object output directory.
obj := $(builddir)/obj

# We build up a list of all targets so we can slurp in the generated
# dependency rule Makefiles in one pass.
all_targets :=

# C++ apps need to be linked with g++.  Not sure what's appropriate.
LD := $(CXX)
RANLIB ?= ranlib

# Flags to make gcc output dependency info.  Note that you need to be
# careful here to use the flags that ccache and distcc can understand.
# We write to a temporary dep file first and then rename at the end
# so we can't end up with a broken dep file.
depfile = $@.d
DEPFLAGS = -MMD -MF $(depfile).tmp

# We have to fixup the deps output in a few ways.
# First, the file output should to mention the proper .o file.
# ccache or distcc lose the path to the target, so we convert a rule of
# the form:
#   foobar.o: DEP1 DEP2
# into
#   path/to/foobar.o: DEP1 DEP2
# Additionally, we want to make missing files not cause us to needlessly
# rebuild.  We want to rewrite
#   foobar.o: DEP1 DEP2 \
#               DEP3
# to
#   DEP1 DEP2:
#   DEP3:
# so if the files are missing, they're just considered phony rules.
# We have to do some pretty insane escaping to get those backslashes
# and dollar signs past Python, make, the shell, and sed at the same time."""
r"""
define fixup_dep
sed -i -e "s|^$(notdir $@)|$@|" $(depfile).tmp
sed -e "s|^[^:]*: *||" -e "s| *\\\\$$||" -e 's|^ *||' \
    -e "/./s|$$|:|" $(depfile).tmp >> $(depfile).tmp
mv $(depfile).tmp $(depfile)
endef
"""
"""
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
cmd_link = $(LD) $(LDFLAGS) -o $@ -Wl,--start-group $^ -Wl,--end-group $(LIBS)

# Shared-object link (for generating .so).
# TODO: perhaps this can share with the LINK command above?
quiet_cmd_solink = SOLINK $@
cmd_solink = $(LD) -shared $(LDFLAGS) -o $@ -Wl,--start-group $^ -Wl,--end-group $(LIBS)

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
	@$(fixup_dep)

$(obj)/%.o: %.s
	$(call do_cmd,cc)

$(obj)/%.o: %.cpp
	$(call do_cmd,cxx)
	@$(fixup_dep)
$(obj)/%.o: %.cc
	$(call do_cmd,cxx)
	@$(fixup_dep)
$(obj)/%.o: %.cxx
	$(call do_cmd,cxx)
	@$(fixup_dep)

# Try building from generated source, too.
$(obj)/%.o: $(obj)/%.cc
	$(call do_cmd,cxx)
	@$(fixup_dep)
$(obj)/%.o: $(obj)/%.cpp
	$(call do_cmd,cxx)
	@$(fixup_dep)
""")

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


def Objectify(path):
  """Convert a path to its output directory form."""
  if '$(' in path:
    return path
  return '$(obj)/' + path


# Map from qualified target to path to output.
target_outputs = {}
# Map from qualified target to a list of all linker dependencies,
# transitively expanded.
# Used in building shared-library-based executables.
target_link_deps = {}


class MakefileWriter:
  def Write(self, qualified_target, output_filename, root, spec, configs):
    print 'Generating %s' % output_filename
    self.fp = open(output_filename, 'w')

    self.fp.write(header)

    # Paths in gyp files are relative to the .gyp file, but we need
    # paths relative to the source root for the master makefile.  Grab
    # the path of the .gyp file as the base to relativize against.
    self.path = gyp.common.RelativePath(os.path.split(output_filename)[0], root)
    self.target = spec['target_name']
    self.type = spec['type']

    deps, link_deps = self.ComputeDeps(spec)

    # Some of the generation below can add extra output, sources, or
    # link dependencies.  All of the out params of the functions that
    # follow use names like extra_foo.
    extra_outputs = []
    extra_sources = []
    extra_link_deps = []

    self.output = self.ComputeOutput(spec)

    # Actions must come first, since they can generate more OBJs for use below.
    if 'actions' in spec:
      self.WriteActions(spec['actions'], extra_sources, extra_outputs)

    # Rules must be early like actions.
    if 'rules' in spec:
      self.WriteRules(spec['rules'], extra_sources, extra_outputs)

    if 'copies' in spec:
      self.WriteCopies(spec['copies'], extra_outputs)

    if 'sources' in spec or extra_sources:
      self.WriteSources(configs, deps, spec.get('sources', []) + extra_sources,
                        extra_outputs, extra_link_deps)

    self.WriteTarget(spec, configs, deps,
                     extra_link_deps + link_deps, extra_outputs)

    # Update global list of target outputs, used in dependency tracking.
    target_outputs[qualified_target] = self.output

    # Update global list of link dependencies.
    if self.type == 'static_library':
      target_link_deps[qualified_target] = [self.output]
    elif self.type == 'shared_library':
      # Anyone that uses us transitively depend on all of our link
      # dependencies.
      target_link_deps[qualified_target] = [self.output] + link_deps

    self.fp.close()


  def WriteActions(self, actions, extra_sources, extra_outputs):
    for action in actions:
      name = self.target + '_' + action['action_name']
      self.WriteLn('### Rules for action "%s":' % action['action_name'])
      inputs = action['inputs']
      outputs = action['outputs']

      # Build up a list of outputs.
      # Collect the output dirs we'll need.
      #
      # HACK: to make file_version_info always rebuild, the gyp file
      # uses a '.bogus' filename.  That could be improved but for now
      # we work around it here.
      dirs = set()
      for i, out in enumerate(outputs):
        dirs.add(os.path.split(out)[0])
        if out.endswith('.bogus'):
          outputs[i] = out[:-len('.bogus')] + '.h'
      if action.get('process_outputs_as_sources', False):
        extra_sources += outputs

      # Write the actual command.
      command = gyp.common.EncodePOSIXShellList(action['action'])
      if 'message' in action:
        self.WriteLn('quiet_cmd_%s = ACTION %s $@' % (name, action['message']))
      else:
        self.WriteLn('quiet_cmd_%s = ACTION %s $@' % (name, name))
      if len(dirs) > 0:
        command = 'mkdir -p %s' % ' '.join(dirs) + '; ' + command
      self.WriteLn('cmd_%s = cd %s; %s' % (name, self.path, command))
      self.WriteLn()
      self.WriteMakeRule(map(self.Absolutify, outputs),
                         map(self.Absolutify, inputs),
                         actions=['$(call do_cmd,%s)' % name])

      # Stuff the outputs in a variable so we can refer to them later.
      outputs_variable = 'action_%s_outputs' % name
      self.WriteLn('%s := %s' % (outputs_variable, ' '.join(outputs)))
      extra_outputs.append('$(%s)' % outputs_variable)

    self.WriteLn()


  def WriteRules(self, rules, extra_sources, extra_outputs):
    for rule in rules:
      name = self.target + '_' + rule['rule_name']
      self.WriteLn('### Generated for rule %s:' % name)

      all_outputs = []

      dirs = set()
      for rule_source in rule['rule_sources']:
        rule_source_basename = os.path.basename(rule_source)
        (rule_source_root, rule_source_ext) = \
            os.path.splitext(rule_source_basename)

        outputs = map(lambda out: out % { 'INPUT_ROOT': rule_source_root },
                      rule['outputs'])
        for out in outputs:
          dirs.add(os.path.split(out)[0])
          if rule.get('process_outputs_as_sources', False):
            extra_sources.append(out)
        all_outputs += outputs
        inputs = map(self.Absolutify, [rule_source] + rule.get('inputs', []))
        actions = ['$(call do_cmd,%s)' % name]

        if name == 'resources_grit':
          # HACK: This is ugly.  Grit intentionally doesn't touch the
          # timestamp of its output file when the file doesn't change,
          # which is fine in hash-based dependency systems like scons
          # and forge, but not kosher in the make world.  After some
          # discussion, hacking around it here seems like the least
          # amount of pain.
          actions += ['@touch --no-create $@']

        self.WriteMakeRule(outputs, inputs, actions)

      self.WriteLn()

      outputs_variable = 'rule_%s_outputs' % name
      self.WriteList(all_outputs, outputs_variable)
      extra_outputs.append('$(%s)' % outputs_variable)

      mkdirs = ''
      if len(dirs) > 0:
        mkdirs = 'mkdir -p %s; ' % ' '.join(dirs)
      self.WriteLn("cmd_%(name)s = %(mkdirs)s%(action)s" % {
        'mkdirs': mkdirs,
        'name': name,
        'action': gyp.common.EncodePOSIXShellList(map(self.FixupArgPath, rule['action']))
      })
      self.WriteLn('quiet_cmd_%(name)s = RULE %(name)s $@' % {
        'name': name,
      })
      self.WriteLn()
    self.WriteLn()


  def WriteCopies(self, copies, extra_outputs):
    self.WriteLn('### Generated for copy rule.')

    variable = self.target + '_copies'
    outputs = []
    for copy in copies:
      for path in copy['files']:
        path = self.Absolutify(path)
        filename = os.path.split(path)[1]
        output = os.path.join(copy['destination'], filename)
        self.WriteMakeRule([output], [path],
                           actions = ['$(call do_cmd,copy)'])
        outputs.append(output)
    self.WriteLn('%s = %s' % (variable, ' '.join(outputs)))
    extra_outputs.append('$(%s)' % variable)
    self.WriteLn()


  def WriteSources(self, configs, deps, sources,
                   extra_outputs, extra_link_deps):
    # Write configuration-specific variables for CFLAGS, etc.
    for configname in sorted(configs.keys()):
      config = configs[configname]
      self.WriteList(config.get('defines'), 'DEFS_%s' % configname, prefix='-D')
      self.WriteList(config.get('cflags'), 'CFLAGS_%s' % configname)
      includes = config.get('include_dirs')
      if includes:
        includes = map(self.Absolutify, includes)
      self.WriteList(includes, 'INCS_%s' % configname, prefix='-I')

    sources = filter(Compilable, sources)
    objs = map(Objectify, map(self.Absolutify, map(Target, sources)))
    self.WriteList(objs, 'OBJS')

    self.WriteLn('# Add to the list of files we specially track '
                 'dependencies for.')
    self.WriteLn('all_targets += $(OBJS)')
    self.WriteLn()

    # Make sure our dependencies are built first.
    if deps:
      self.WriteMakeRule(['$(OBJS)'], deps,
                         comment = 'Make sure our dependencies are built '
                                   'before any of us.',
                         order_only = True)

    # Make sure the actions and rules run first.
    # If they generate any extra headers etc., the per-.o file dep tracking
    # will catch the proper rebuilds, so order only is still ok here.
    if extra_outputs:
      self.WriteMakeRule(['$(OBJS)'], extra_outputs,
                         comment = 'Make sure our actions/rules run '
                                   'before any of us.',
                         order_only = True)

    if objs:
      extra_link_deps.append('$(OBJS)')
      self.WriteLn("""\
# CFLAGS et al overrides must be target-local.
# See "Target-specific Variable Values" in the GNU Make manual.""")
      self.WriteLn("$(OBJS): CFLAGS := $(CFLAGS_$(BUILDTYPE)) "
                   "$(DEFS_$(BUILDTYPE)) $(INCS_$(BUILDTYPE))")
      self.WriteLn("$(OBJS): CXXFLAGS := $(CFLAGS_$(BUILDTYPE)) "
                   "$(DEFS_$(BUILDTYPE)) $(INCS_$(BUILDTYPE))")

    self.WriteLn()


  def ComputeOutput(self, spec):
    output = None
    target = spec['target_name']
    if self.type == 'static_library':
      target = 'lib%s.a' % target
    elif self.type in ('loadable_module', 'shared_library'):
      target = 'lib%s.so' % target
    elif self.type == 'none':
      target = '%s.stamp' % target
    elif self.type == 'settings':
      return None
    elif self.type == 'executable':
      target = spec.get('product_name', target)
    else:
      print "ERROR: What output file should be generated?", "typ", self.type, "target", target
    output = os.path.join('$(obj)', self.path, target)
    return output


  def ComputeDeps(self, spec):
    deps = set()
    link_deps = set()
    if 'dependencies' in spec:
      deps.update([target_outputs[dep] for dep in spec['dependencies']
                   if target_outputs[dep]])
      for dep in spec['dependencies']:
        if dep in target_link_deps:
          link_deps.update(target_link_deps[dep])
      deps.update(link_deps)
      # TODO: It seems we need to transitively link in libraries (e.g. -lfoo)?
      # This hack makes it work:
      # link_deps.extend(spec.get('libraries', []))
    return (list(deps), list(link_deps))


  def WriteTarget(self, spec, configs, deps, link_deps, extra_outputs):
    self.WriteLn('### Rules for final target.')

    if extra_outputs:
      self.WriteMakeRule([self.output], extra_outputs,
                         comment = 'Build our special outputs first.',
                         order_only = True)

    if self.type not in ('settings', 'none'):
      for configname in sorted(configs.keys()):
        config = configs[configname]
        self.WriteList(config.get('ldflags'), 'LDFLAGS_%s' % configname)
      self.WriteList(spec.get('libraries'), 'LIBS')
      self.WriteLn('%s: LDFLAGS := $(LDFLAGS_$(BUILDTYPE)) '
                   '$(LIBS)' % self.output)

    if self.type == 'executable':
      self.WriteMakeRule([self.output], link_deps,
                         actions = ['$(call do_cmd,link)'])
      filename = os.path.split(self.output)[1]
      binpath = '$(builddir)/' + filename
      self.WriteMakeRule([binpath], [self.output],
                         actions = ['$(call do_cmd,copy)'],
                         comment = 'Copy this to the binary output path.')

      self.WriteMakeRule([filename], [binpath],
                         comment = 'Short alias for building this executable.')
      self.WriteMakeRule(['all'], [binpath],
                         comment = 'Add executable to "all" target.')
    elif self.type == 'static_library':
      self.WriteMakeRule([self.output], link_deps,
                         actions = [
                             '$(call do_cmd,ar)',
                             '$(call do_cmd,ranlib)',
                             ])
    elif self.type in ('loadable_module', 'shared_library'):
      self.WriteMakeRule([self.output], link_deps,
                         actions = ['$(call do_cmd,solink)'])
    elif self.type == 'none':
      # Write a stamp line.
      self.WriteMakeRule([self.output], deps,
                         actions = ['$(call do_cmd,touch)'])
    elif self.type == 'settings':
      # Only used for passing flags around.
      pass
    else:
      print "WARNING: no output for", self.type, target


  def WriteList(self, list, variable=None, prefix=''):
    self.fp.write(variable + " := ")
    if list:
      list = [QuoteIfNecessary(prefix + l) for l in list]
      self.fp.write(" \\\n\t".join(list))
    self.fp.write("\n\n")


  def WriteMakeRule(self, outputs, inputs, actions=None, comment=None,
                    order_only=False):
    if comment:
      self.WriteLn('# ' + comment)
    order_insert = '| ' if order_only else ''
    self.WriteLn('%s: %s%s' % (outputs[0], order_insert, ' '.join(inputs)))
    if actions:
      for action in actions:
        self.WriteLn('\t%s' % action)
    if len(outputs) > 1:
      # If we have more than one output, a rule like
      #   foo bar: baz
      # that for *each* output we must run the action, potentially
      # in parallel.  That is not what we're trying to write -- what
      # we want is that we run the action once and it generates all
      # the files.
      # http://www.gnu.org/software/hello/manual/automake/Multiple-Outputs.html
      # discusses this problem and has this solution:
      # 1) Write the naive rule that would produce parallel runs of
      # the action.
      # 2) Make the outputs seralized on each other, so we won't start
      # a a parallel run until the first run finishes, at which point
      # we'll have generated all the outputs and we're done.
      self.WriteLn('%s: %s' % (' '.join(outputs[1:]), outputs[0]))
    self.WriteLn()


  def WriteLn(self, text=''):
    self.fp.write(text + '\n')


  def Absolutify(self, path):
    """Convert a subdirectory-relative path into a base-relative path.
    Skips over paths that contain variables."""
    if '$(' in path:
      return path
    return os.path.normpath(os.path.join(self.path, path))


  def FixupArgPath(self, arg):
    if '/' in arg or '.h.' in arg:
      return self.Absolutify(arg)
    return arg


def GenerateOutput(target_list, target_dicts, data, params):
  options = params['options']
  root_makefile = open('Makefile', 'w')
  root_makefile.write(SHARED_HEADER)

  for qualified_target in target_list:
    build_file, target = gyp.common.BuildFileAndTarget('', qualified_target)[:2]
    output_file = os.path.join(os.path.split(build_file)[0], target + '.mk')

    spec = target_dicts[qualified_target]
    configs = spec['configurations']

    writer = MakefileWriter()
    writer.Write(qualified_target, output_file, options.depth, spec, configs)
    root_makefile.write('include ' + output_file + "\n")

  root_makefile.write(SHARED_FOOTER)

  root_makefile.close()
