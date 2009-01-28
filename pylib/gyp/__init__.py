#!/usr/bin/python

import demjson
import optparse
import os.path
import re
import sys
import simplejson


def BuildFileAndTarget(build_file, target):
  # NOTE: If you just want to split up target into a build_file and target,
  # and you know that target already has a build_file that's been produced by
  # this function, pass "" for build_file.

  target_split = target.split(":", 1)
  if len(target_split) == 2:
    [build_file_rel, target] = target_split

    # If a relative path, build_file_rel is relative to the directory
    # containing build_file.  If build_file is not in the current directory,
    # build_file_rel is not a usable path as-is.  Resolve it by interpreting it
    # as relative to build_file.  If build_file_rel is absolute, it is usable
    # as a path regardless of the current directory, and os.path.join will
    # return it as-is.
    build_file = os.path.join(os.path.dirname(build_file), build_file_rel)

  return [build_file, target, build_file + ":" + target]


def QualifiedTarget(build_file, target):
  return BuildFileAndTarget(build_file, target)[2]


def ExceptionAppend(e, msg):
  if not e.args:
    e.args = [msg]
  else:
    if len(e.args) == 1:
      e.args = [e.args[0] + " " + msg]
    else:
      e.args = [e.args[0] + " " + msg, e.args[1:]]


def ReadBuildFile(build_file_path, data={}):
  build_file = open(build_file_path)
  try:
    # simplejson
    #build_file_data = simplejson.load(build_file)

    # demjson
    build_file_contents = build_file.read()
    json = demjson.JSON()
    json.prevent("trailing_comma_in_literal")
    json.prevent("undefined_values")
    build_file_data = json.decode(build_file_contents)

    data[build_file_path] = build_file_data
  except Exception, e:
    ExceptionAppend(e, "while reading " + build_file_path)
    raise
  finally:
    build_file.close()

  # Look for references to other build files that may need to be read.
  if "targets" in build_file_data:
    for target_name, target_dict in build_file_data["targets"].iteritems():
      if "dependencies" in target_dict:
        for dependency in target_dict["dependencies"]:
          other_build_file = BuildFileAndTarget(build_file_path, dependency)[0]
          if not other_build_file in data:
            ReadBuildFile(other_build_file, data)

  return data


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
      raise self.CircularException, "Returned to " + str(self.ref) + \
                                    " while it was already being visited"

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
    if not "dependencies" in spec or len(spec["dependencies"]) == 0:
      target_node.dependencies = [root_node]
      root_node.dependents.append(target_node)
    else:
      for dependency in spec["dependencies"]:
        target_build_file = BuildFileAndTarget("", target)[0]
        dependency = QualifiedTarget(target_build_file, dependency)
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
    raise DependencyTreeNode.CircularException, "some targets not reachable"

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
        to.append(os.path.join(os.path.dirname(to_file),
                               os.path.dirname(fro_file),
                               item))
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
          "Attempt to merge list item of unsupported type " + \
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
          "Attempt to merge dict value of type " + v.__class__.__name__ + \
          " into incompatible type " + to[k].__class__.__name__ + \
          " for key " + k
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
      is_paths = k in ["include_dirs", "sources"]
      MergeLists(to[k], v, to_file, fro_file, is_paths)
    else:
      raise TypeError, \
          "Attempt to merge dict value of unsupported type " + \
          v.__class__.__name__ + " for key " + k


def main(args):
  my_name = os.path.basename(sys.argv[0])

  parser = optparse.OptionParser()
  usage = "usage: %s -f format build_file [...]"
  parser.set_usage(usage.replace("%s", "%prog"))
  parser.add_option("-f", "--format", dest="format",
                    help="Output format to generate (required)")
  (options, build_files) = parser.parse_args(args)
  if not options.format:
    print >>sys.stderr, (usage + "\n\n%s: error: format is required") % \
                        (my_name, my_name)
    sys.exit(1)
  if not build_files:
    print >>sys.stderr, (usage + "\n\n%s: error: build_file is required") % \
                        (my_name, my_name)
    sys.exit(1)

  generator_name = "gyp.generator." + options.format
  generator = __import__(generator_name, fromlist=generator_name)

  data = {}
  for build_file in build_files:
    ReadBuildFile(build_file, data)

  targets = {}
  for build_file_name, build_file_data in data.iteritems():
    for target in build_file_data["targets"]:
      qualified_target = QualifiedTarget(build_file_name, target)
      targets[qualified_target] = build_file_data["targets"][target]

  [dependency_nodes, root_node, flat_list] = BuildDependencyList(targets)

  # TODO(mark): Make all of this stuff generic.  WORK IN PROGRESS.

  # Look at each project's settings dict, and merge settings into targets.
  for build_file_name, build_file_data in data.iteritems():
    if "settings" in build_file_data:
      file_settings = build_file_data["settings"]
      for target, target_dict in build_file_data["targets"].iteritems():
        MergeDicts(target_dict, file_settings, build_file_name, build_file_name)

  # TODO(mark): This needs to be refactored real soon now.
  # Do conditions and source_patterns
  for target in flat_list:
    [build_file, target_unq] = BuildFileAndTarget("", target)[0:2]
    target_dict = data[build_file]["targets"][target_unq]

    if "conditions" in target_dict:
      # TODO(mark): Do we want to sever the conditions dict from target_dict?
      for item in target_dict["conditions"]:
        [condition, settings] = item
        if condition == "OS==mac":
          MergeDicts(target_dict, settings, build_file, build_file)

    if "source_patterns" in target_dict:
      for source_pattern in target_dict["source_patterns"]:
        [action, pattern] = source_pattern
        pattern_re = re.compile(pattern)
        # Ugh, need to make a copy up front because we can't modify the list
        # while iterating through it.  This may need some rethinking.  That
        # makes it TODO(mark).
        new_sources = target_dict["sources"][:]
        for source in target_dict["sources"]:
          if pattern_re.search(source):
            if action == "exclude":
              new_sources.remove(source)
        target_dict["sources"] = new_sources

  # Now look for dependent_settings sections in dependencies, and merge
  # settings.
  for target in flat_list:
    [build_file, target_unq] = BuildFileAndTarget("", target)[0:2]
    target_dict = data[build_file]["targets"][target_unq]
    if not "dependencies" in target_dict:
      continue

    for dependency in target_dict["dependencies"]:
      [dep_build_file, dep_target_unq] = \
          BuildFileAndTarget(build_file, dependency)[0:2]
      dependency_dict = data[dep_build_file]["targets"][dep_target_unq]
      if not "dependent_settings" in dependency_dict:
        continue

      dependent_settings = dependency_dict["dependent_settings"]
      MergeDicts(target_dict, dependent_settings, build_file, dep_build_file)

  # TODO(mark): This logic is rough, but it works for base_unittests.
  # Set up computed dependencies.  For each non-static library target, look
  # at the entire dependency hierarchy and add any static libraries as computed
  # dependencies.  Static library targets have no computed dependencies.
  for target in flat_list:
    [build_file, target_unq] = BuildFileAndTarget("", target)[0:2]
    target_dict = data[build_file]["targets"][target_unq]
    if target_dict["type"] == "static_library":
      dependents = dependency_nodes[target].FlattenToList([])[1:]
      for dependent in dependents:
        [dependent_bf, dependent_unq] = BuildFileAndTarget("", dependent)[0:2]
        dependent_dict = data[dependent_bf]["targets"][dependent_unq]
        if dependent_dict["type"] != "static_library":
          if not "computed_dependencies" in dependent_dict:
            dependent_dict["computed_dependencies"] = []
          dependent_dict["computed_dependencies"].append(target)
          if "libraries" in target_dict:
            if not "computed_libraries" in dependent_dict:
              dependent_dict["computed_libraries"] = []
            dependent_dict["computed_libraries"].extend(target_dict["libraries"])
  for target in flat_list:
    [build_file, target_unq] = BuildFileAndTarget("", target)[0:2]
    target_dict = data[build_file]["targets"][target_unq]
    if target_dict["type"] != "static_library":
      if "dependencies" in target_dict:
        if not "computed_dependencies" in target_dict:
          target_dict["computed_dependencies"] = []
        for dependency in target_dict["dependencies"]:
          dependency_qual = BuildFileAndTarget(build_file, dependency)[2]
          if not dependency_qual in target_dict["computed_dependencies"]:
            target_dict["computed_dependencies"].append(dependency_qual)
      if "libraries" in target_dict:
        if not "libraries" in target_dict:
          target_dict["libraries"] = []
        for library in target_dict["libraries"]:
          if not library in target_dict["libraries"]:
            target_dict["libraries"].append(library)

  generator.GenerateOutput(flat_list, data)


if __name__ == "__main__":
  main(sys.argv[1:])
