#!/usr/bin/python

import optparse
import os.path
import re
import sys


def BuildFileAndTarget(build_file, target):
  # NOTE: If you just want to split up target into a build_file and target,
  # and you know that target already has a build_file that's been produced by
  # this function, pass '' for build_file.

  target_split = target.split(':', 1)
  if len(target_split) == 2:
    [build_file_rel, target] = target_split

    # If a relative path, build_file_rel is relative to the directory
    # containing build_file.  If build_file is not in the current directory,
    # build_file_rel is not a usable path as-is.  Resolve it by interpreting it
    # as relative to build_file.  If build_file_rel is absolute, it is usable
    # as a path regardless of the current directory, and os.path.join will
    # return it as-is.
    build_file = os.path.normpath(os.path.join(os.path.dirname(build_file),
                                               build_file_rel))

  return [build_file, target, build_file + ':' + target]


def QualifiedTarget(build_file, target):
  # "Qualified" means the file that a target was defined in and the target
  # name, separated by a colon.
  return BuildFileAndTarget(build_file, target)[2]


def ExceptionAppend(e, msg):
  if not e.args:
    e.args = [msg]
  elif len(e.args) == 1:
    e.args = [str(e.args[0]) + ' ' + msg]
  else:
    e.args = [str(e.args[0]) + ' ' + msg, e.args[1:]]


def LoadOneBuildFile(build_file_path):
  build_file = open(build_file_path)
  build_file_contents = build_file.read()
  build_file.close()

  build_file_data = None
  try:
    build_file_data = eval(build_file_contents)
  except SyntaxError, e:
    e.filename = build_file_path
    raise
  except Exception, e:
    ExceptionAppend(e, 'while reading ' + build_file_path)
    raise

  # TODO(mark): Apply "pre"/"early" variable expansion here.

  # Apply "pre"/"early" conditionals.
  ProcessConditionalsInDict(build_file_data)

  # Scan for includes and merge them in.
  try:
    LoadBuildFileIncludesIntoDict(build_file_data, build_file_path)
  except Exception, e:
    ExceptionAppend(e, 'while reading includes of ' + build_file_path)
    raise

  return build_file_data


def LoadBuildFileIncludesIntoDict(subdict, subdict_path):
  if 'includes' in subdict:
    # Unhook the includes list, it's no longer needed.
    includes_list = subdict['includes']
    del subdict['includes']

    # Replace it by merging in the included files.
    for include in includes_list:
      MergeDicts(subdict, LoadOneBuildFile(include), subdict_path, include)

  # Recurse into subdictionaries.
  for k, v in subdict.iteritems():
    if v.__class__ == dict:
      LoadBuildFileIncludesIntoDict(v, subdict_path)
    elif v.__class__ == list:
      LoadBuildFileIncludesIntoList(v, subdict_path)


# This presently only recurses into lists so that it can look for dicts.
# Should it allow includes within lists inline?  TODO(mark): Decide.
#   sources: [
#     "source1.cc",
#     { "includes": [ "some_included_file" ] },
#     "source2.cc"
#   ]
def LoadBuildFileIncludesIntoList(sublist, sublist_path):
  for item in sublist:
    if item.__class__ == dict:
      LoadBuildFileIncludesIntoDict(item, sublist_path)
    elif item.__class__ == list:
      LoadBuildFileIncludesIntoList(item, sublist_path)


# TODO(mark): I don't love this name.  It just means that it's going to load
# a build file that contains targets and is expected to provide a targets dict
# that contains the targets...
def LoadTargetBuildFile(build_file_path, data={}):
  if build_file_path in data:
    # Already loaded.
    return

  build_file_data = LoadOneBuildFile(build_file_path)
  data[build_file_path] = build_file_data

  # ...it's loaded and it should have EARLY references and conditionals
  # all resolved and includes merged in...at least it will eventually...

  # Look for dependencies.  This means that dependency resolution occurs
  # after "pre" conditionals and variable expansion, but before "post" -
  # in other words, you can't put a "dependencies" section inside a "post"
  # conditional within a target.

  if 'targets' in build_file_data:
    for target_dict in build_file_data['targets']:
      if 'dependencies' not in target_dict:
        continue
      for dependency in target_dict['dependencies']:
        other_build_file = BuildFileAndTarget(build_file_path, dependency)[0]
        LoadTargetBuildFile(other_build_file, data)

  return data


# TODO(mark): This needs a way to choose which conditions dict to look at.
# Right now, it's called "conditions" but that's just so that I don't need
# to edit the names in the input files.  The existing conditions are all
# "early" or "pre" conditions.  Support for "late"/"post"/"target" conditions
# needs to be added as well.
def ProcessConditionalsInDict(subdict):
  if 'conditions' in subdict:
    # Unhook the conditions list, it's no longer needed.
    conditions_dict = subdict['conditions']
    del subdict['conditions']

    # Evaluate conditions and merge in the dictionaries for the ones that pass.
    for condition in conditions_dict:
      [expression, settings_dict] = condition
      # TODO(mark): The expression is hard-coded.  That obviously has to
      # change.
      if expression == 'OS==mac':
        # OK to pass '', '' for the build files because everything comes from
        # the same build file and everything is already relative to the same
        # place.
        MergeDicts(subdict, settings_dict, '', '')

  # Recurse into subdictionaries.
  for k, v in subdict.iteritems():
    if v.__class__ == dict:
      ProcessConditionalsInDict(v)
    elif v.__class__ == list:
      ProcessConditionalsInList(v)


# TODO(mark): The same comment about list recursion and whether to allow
# inlines in lists at LoadBuildFileIncludesIntoList applies to this function.
def ProcessConditionalsInList(sublist):
  for item in sublist:
    if item.__class__ == dict:
      ProcessConditionalsInDict(item)
    elif item.__class__ == list:
      ProcessConditionalsInList(item)


class DependencyTreeNode(object):
  """

  Class variables:
     MARK_NONE: An object that has not yet been visited during flattening.
     MARK_PENDING: An object that has been visited, but whose dependencies
                   have not yet been visisted.
     MARK_DONE: An object that has been visited and whose dependencies have
                also been visited.
  Attributes:
     ref: A reference to an object that this DependencyTreeNode represents.
     dependencies: List of DependencyTreeNodes on which this one depends.
     dependents: List of DependencyTreeNodes that depend on this one.
     _mark: That's me.  Also, this is set to a MARK_* constant to the mark
            state of the object during flattening.
  """

  MARK_NONE = 0
  MARK_PENDING = 1
  MARK_DONE = 2

  class CircularException(Exception):
    pass

  def __init__(self, ref):
    self.ref = ref
    self.dependencies = []
    self.dependents = []
    self._mark = self.MARK_NONE

  # YOU MUST CALL THIS with flat_list set to [].  Leave the optional parameters
  # at their defaults.  TODO(mark): Provide a better public interface.
  def FlattenToList(self, flat_list, reset=True, check_dependencies=False):
    if reset:
      self._mark = self.MARK_NONE
      for dependent in self.dependents:
        dependent.FlattenToList(None, reset)
      if flat_list == None:
        return

    if self._mark == self.MARK_PENDING:
      # Oops.  Found a cycle.
      raise self.CircularException, 'Returned to ' + str(self.ref) + \
                                    ' while it was already being visited'

    if self._mark == self.MARK_DONE:
      # Already visisted.
      return

    # Visit all nodes upon which this one depends, or ensure that they have
    # already been visited.
    self._mark = self.MARK_PENDING

    # Don't check dependencies of the root object that the caller requested.
    if check_dependencies:
      for dependency in self.dependencies:
        dependency.FlattenToList(flat_list, False, True)

    # All of this node's dependency references are in the list.  Append this
    # node's reference.
    self._mark = self.MARK_DONE
    flat_list.append(self.ref)

    # Visit all nodes that depend on this one.  Don't try to visit a dependent
    # marked PENDING, that's the dependent that called this object.
    for dependent in self.dependents:
      if dependent._mark != self.MARK_PENDING:
        dependent.FlattenToList(flat_list, False, True)

    return flat_list


def BuildDependencyList(targets):
  # Create a DependencyTreeNode for each target.  Put it into a dict for easy
  # access.
  dependency_nodes = {}
  for target, spec in targets.iteritems():
    if not target in dependency_nodes:
      dependency_nodes[target] = DependencyTreeNode(target)

  # Set up the dependency links.  Targets that have no dependencies are treated
  # as dependent on root_node.
  root_node = DependencyTreeNode(None)
  for target, spec in targets.iteritems():
    target_node = dependency_nodes[target]
    if not 'dependencies' in spec or len(spec['dependencies']) == 0:
      target_node.dependencies = [root_node]
      root_node.dependents.append(target_node)
    else:
      for index in range(0, len(spec['dependencies'])):
        dependency = spec['dependencies'][index]
        target_build_file = BuildFileAndTarget('', target)[0]
        dependency = QualifiedTarget(target_build_file, dependency)
        # Store the qualified name of the target even if it wasn't originally
        # qualified in the dict.  Others will find this useful as well.
        spec['dependencies'][index] = dependency
        dependency_node = dependency_nodes[dependency]
        target_node.dependencies.append(dependency_node)
        dependency_node.dependents.append(target_node)

  # Take the root node out of the list because it doesn't correspond to a real
  # target.
  flat_list = root_node.FlattenToList([])[1:]

  # If there's anything left unvisited, there must be a self-contained circular
  # dependency.  If you need to figure out what's wrong, look for elements of
  # targets that are not in flat_list.  FlattenToList doesn't catch these
  # self-contained cycles because they're not reachable from root_node.
  if len(flat_list) != len(targets):
    raise DependencyTreeNode.CircularException, 'some targets not reachable'

  return [dependency_nodes, root_node, flat_list]


def MergeLists(to, fro, to_file, fro_file, is_paths=False):
  # TODO(mark): Support a way for the "fro" list to declare how it wants to
  # be merged into the "to" list.  Right now, "append" is always used, but
  # other possible policies include "prepend" and "replace".  Perhaps the
  # "fro" list can include a special first token, or perhaps the "fro" list
  # can have a sibling or something identifying the desired treatment.  Also,
  # "append" may not always be the most appropriate merge policy.  For
  # example, when merging file-wide .gyp settings into targets, it seems more
  # logical to prepend file-wide settings to target-specific ones, which are
  # thought of as "inheriting" file-wide setings.
  for item in fro:
    if isinstance(item, str) or isinstance(item, int):
      # The cheap and easy case.
      # TODO(mark): Expand variables here?  I waffle a bit more on this below,
      # in MergeDicts.
      if is_paths and to_file != fro_file:
        # If item is a relative path, it's relative to the build file dict that
        # it's coming from.  Fix it up to make it relative to the build file
        # dict that it's going into.
        # TODO(mark): We might want to exclude some things here even if
        # is_paths is true.
        # TODO(mark): The next line may be wrong for more complex cases, we
        # may need to do a better job of finding one build file's path relative
        # to another.  Good candidate for abstraction!
        to.append(os.path.normpath(os.path.join(os.path.dirname(to_file),
                                                os.path.dirname(fro_file),
                                                item)))
      else:
        to.append(item)
    elif isinstance(item, dict):
      # Insert a copy of the dictionary.
      to.append(item.copy())
    elif isinstance(item, list):
      # Insert a copy of the list.
      to.append(item[:])
    else:
      raise TypeError, \
          'Attempt to merge list item of unsupported type ' + \
          item.__class__.__name__


def MergeDicts(to, fro, to_file, fro_file):
  # I wanted to name the parameter "from" but it's a Python keyword...
  for k, v in fro.iteritems():
    # It would be nice to do "if not k in to: to[k] = v" but that wouldn't give
    # copy semantics.  Something else may want to merge from the |fro| dict
    # later, and having the same dict ref pointed to twice in the tree isn't
    # what anyone wants considering that the dicts may subsequently be
    # modified.
    if k in to and v.__class__ != to[k].__class__:
      raise TypeError, \
          'Attempt to merge dict value of type ' + v.__class__.__name__ + \
          ' into incompatible type ' + to[k].__class__.__name__ + \
          ' for key ' + k
    if isinstance(v, str) or isinstance(v, int):
      # Overwrite the existing value, if any.  Cheap and easy.
      # TODO(mark): Expand variables here?  We may want a way to make use
      # of the existing string value, if any, and variable expansion might
      # be the right solution.  On the other hand, it's possible that we
      # might want to do all expansions in a separate step completely
      # independent of merging.  These questions need answers.
      to[k] = v
    elif isinstance(v, dict):
      # Recurse, guaranteeing copies will be made of objects that require it.
      if not k in to:
        to[k] = {}
      MergeDicts(to[k], v, to_file, fro_file)
    elif isinstance(v, list):
      # Call MergeLists, which will make copies of objects that require it.
      if not k in to:
        to[k] = []
      is_paths = k in ['include_dirs', 'sources']
      MergeLists(to[k], v, to_file, fro_file, is_paths)
    else:
      raise TypeError, \
          'Attempt to merge dict value of unsupported type ' + \
          v.__class__.__name__ + ' for key ' + k


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
  usage = 'usage: %s [-f format] [build_file ...]'
  parser.set_usage(usage.replace('%s', '%prog'))
  parser.add_option('-f', '--format', dest='format',
                    help='Output format to generate')
  (options, build_files) = parser.parse_args(args)
  if not options.format:
    options.format = {'darwin': 'xcodeproj'}[sys.platform]
  if not build_files:
    build_files = FindBuildFiles()
  if not build_files:
    print >>sys.stderr, (usage + '\n\n%s: error: no build_file') % \
                        (my_name, my_name)
    return 1

  generator_name = 'gyp.generator.' + options.format
  generator = __import__(generator_name, fromlist=generator_name)

  # Load build files.  This loads every target-containing build file into
  # the |data| dictionary such that the keys to |data| are build file names,
  # and the values are the entire build file contents after "early" or "pre"
  # processing has been done and includes have been resolved.
  data = {}
  for build_file in build_files:
    LoadTargetBuildFile(build_file, data)

  # Build a dict to access each target's subdict by qualified name.
  targets = {}
  for build_file in data:
    if 'targets' in data[build_file]:
      for target in data[build_file]['targets']:
        target_name = QualifiedTarget(build_file, target['name'])
        targets[target_name] = target

  # BuildDependencyList will also fix up all dependency lists to contain only
  # qualified names.  That makes it much easier to see if a target is already
  # in a dependency list, because the name it will be listed by is known.
  # This is used below when the dependency lists are adjusted for static
  # libraries.  The only thing I don't like about this is that it seems like
  # BuildDependencyList shouldn't modify "targets".  I thought we looped over
  # "targets" too many times, though, and that seemed like a good place to do
  # this fix-up.  We may want to revisit where this is done.
  [dependency_nodes, root_node, flat_list] = BuildDependencyList(targets)

  # TODO(mark): Make all of this stuff generic.  WORK IN PROGRESS.  It's a
  # lot cleaner than it used to be, but there's still progress to be made.
  # The whole file above this point is in pretty good shape, everything
  # below this line is kind of a wasteland.

  # Look at each project's settings dict, and merge settings into targets.
  # TODO(mark): Figure out when we should do this step.  Seems like it should
  # happen earlier.  Also, the policy here should be for dict keys in the base
  # settings dict to NOT overwrite keys in the target, and for list items in
  # the base settings dict to be PREPENDED to target lists instead of
  # appended.
  for build_file_name, build_file_data in data.iteritems():
    if 'settings' in build_file_data:
      file_settings = build_file_data['settings']
      for target_dict in build_file_data['targets']:
        MergeDicts(target_dict, file_settings, build_file_name, build_file_name)

  # Now look for dependent_settings sections in dependencies, and merge
  # settings.
  for target in flat_list:
    target_dict = targets[target]
    if not 'dependencies' in target_dict:
      continue

    build_file = BuildFileAndTarget("", target)[0]
    for dependency in target_dict['dependencies']:
      [dep_build_file, dep_target_unq, dep_target_q] = \
          BuildFileAndTarget(build_file, dependency)
      dependency_dict = targets[dep_target_q]
      if not 'dependent_settings' in dependency_dict:
        continue

      dependent_settings = dependency_dict['dependent_settings']
      MergeDicts(target_dict, dependent_settings, build_file, dep_build_file)

  # TODO(mark): This logic is rough, but it works for base_unittests.
  # Set up computed dependencies.  For each non-static library target, look
  # at the entire dependency hierarchy and add any static libraries as computed
  # dependencies.  Static library targets have no computed dependencies.
  for target in flat_list:
    target_dict = targets[target]

    # If we've got a static library here...
    if target_dict['type'] == 'static_library':
      dependents = dependency_nodes[target].FlattenToList([])[1:]

      # Look at every target that depends on it, even indirectly...
      for dependent in dependents:
        [dependent_bf, dependent_unq, dependent_q] = \
            BuildFileAndTarget('', dependent)
        dependent_dict = targets[dependent_q]

        # If the dependent isn't a static library...
        if dependent_dict['type'] != 'static_library':

          # Make it depend on the static library if it doesn't already...
          if not 'dependencies' in dependent_dict:
            dependent_dict['dependencies'] = []
          if not target in dependent_dict['dependencies']:
            dependent_dict['dependencies'].append(target)

          # ...and make it link against the libraries that the static library
          # wants, if it doesn't already...
          if 'libraries' in target_dict:
            if not 'libraries' in dependent_dict:
              dependent_dict['libraries'] = []
            for library in target_dict['libraries']:
              if not library in dependent_dict['libraries']:
                dependent_dict['libraries'].append(library)

      # The static library doesn't need its dependencies or libraries any more.
      if 'dependencies' in target_dict:
        del target_dict['dependencies']
      if 'libraries' in target_dict:
        del target_dict['libraries']

  # Do source_patterns.
  # TODO(mark): This needs to be refactored real soon now.
  # I'm positive I wrote this comment before, but now it's gone.  Here's the
  # deal: this should be made more general, so that we can have arbitrary
  # *_patterns sections.  That probably means that it should be called
  # sources_patterns instead of source_patterns.  While we're thinking about
  # renaming it, maybe _patterns isn't the best name anyway.  Apparently I
  # called it source_rules in a meeting yesterday.  Also, the only action
  # supported here is "exclude" which seems to imply that there should be an
  # "include" but I'm having a hard time coming up with a good case for
  # that.  Someone suggested looking at the filesystem (which implies a glob
  # for include patterns rather than a RE).  I suppose we could do that, but
  # I don't really love it.
  # Also, I think we want to allow for a simpler _exclude list that does strict
  # string matching without the RE business.
  for target in flat_list:
    [build_file, target_unq] = BuildFileAndTarget('', target)[0:2]
    target_dict = targets[target]

    if 'source_patterns' in target_dict:
      for source_pattern in target_dict['source_patterns']:
        [action, pattern] = source_pattern
        pattern_re = re.compile(pattern)
        # Ugh, need to make a copy up front because we can't modify the list
        # while iterating through it.  This may need some rethinking.  That
        # makes it TODO(mark).
        new_sources = target_dict['sources'][:]
        for source in target_dict['sources']:
          if pattern_re.search(source):
            if action == 'exclude':
              new_sources.remove(source)
        target_dict['sources'] = new_sources

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
