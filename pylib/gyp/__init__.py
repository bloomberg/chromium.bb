#!/usr/bin/python

import optparse
import os.path
import re
import sys


# TODO(mark): variables_hack is a temporary hack to work with conditional
# sections since real expression parsing is not currently available.
# Additional variables are added to this list when a generator is imported
# in main.
variables_hack = []


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
    build_file_data = eval(build_file_contents, {'__builtins__': None}, None)
  except SyntaxError, e:
    e.filename = build_file_path
    raise
  except Exception, e:
    ExceptionAppend(e, 'while reading ' + build_file_path)
    raise

  # Apply "pre"/"early" variable expansions.
  ProcessVariableExpansionsInDict(build_file_data, False)

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


early_variable_re = re.compile('(<\((.*?)\))')
late_variable_re = re.compile('(>\((.*?)\))')

def ExpandVariables(input, is_late, variables):
  # Look for the pattern that gets expanded into variables
  if not is_late:
    variable_re = early_variable_re
  else:
    variable_re = late_variable_re
  matches = variable_re.findall(input)

  output = input
  if matches != None:
    # Reverse the list of matches so that replacements are done right-to-left.
    # That ensures that earlier re.sub calls won't mess up the string in a
    # way that causes later calls to find the earlier substituted text instead
    # of what's intended for replacement.
    matches.reverse()
    for match in matches:
      # match[0] is the substring to look for and match[1] is the name of
      # the variable.
      if not match[1] in variables:
        raise KeyError, 'Undefined variable ' + match[1] + ' in ' + input
      output = re.sub(re.escape(match[0]), variables[match[1]], output)
  return output


def ProcessVariableExpansionsInDict(subdict, is_late, variables=None):
  if variables == None:
    variables = {}

  # Any keys with plain string values in the current scope become automatic
  # variables.  The variable name is the key name with a "_" character
  # prepended.
  for k, v in subdict.iteritems():
    if isinstance(v, str) or isinstance(v, int):
      variables['_' + k] = v

  # Handle the associated variables subdict first.  Pass it the real variables
  # dict that will be used in this scope, not a copy.
  if 'variables' in subdict:
    ProcessVariableExpansionsInDict(subdict['variables'], is_late, variables)
    variables.update(subdict['variables'])

  for k, v in subdict.iteritems():
    if k == 'variables':
      # This was already done above.
      continue
    if isinstance(v, dict):
      # Make a copy of the dict so that subdicts can't influence parents.
      ProcessVariableExpansionsInDict(v, is_late, variables.copy())
    elif isinstance(v, list):
      # The list itself can't influence the variables dict, and it'll make
      # copies if it needs to pass the dict to something that can influence
      # it.
      ProcessVariableExpansionsInList(v, is_late, variables)
    elif isinstance(v, str):
      subdict[k] = ExpandVariables(v, is_late, variables)
    elif not isinstance(v, int):
      raise TypeError, 'Unknown type ' + v.__class__.__name__ + ' for ' + k


def ProcessVariableExpansionsInList(sublist, is_late, variables=None):
  index = 0
  while index < len(sublist):
    item = sublist[index]
    if isinstance(item, dict):
      # Make a copy of the variables dict so that it won't influence anything
      # outside of its own scope.
      ProcessVariableExpansionsInDict(item, is_late, variables.copy())
    elif isinstance(item, list):
      ProcessVariableExpansionsInList(item, is_late, variables)
    elif isinstance(item, str):
      sublist[index] = ExpandVariables(item, is_late, variables)
    elif not isinstance(item, int):
      raise TypeError, 'Unknown type ' + item.__class__.__name__ + \
                       ' at index ' + str(index)
      pass
    index = index + 1


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
      # TODO(mark): This is ever-so-slightly better than it was initially when
      # 'OS==mac' was hard-coded, but expression evaluation is needed.
      if expression in variables_hack:
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

  Attributes:
     ref: A reference to an object that this DependencyTreeNode represents.
     dependencies: List of DependencyTreeNodes on which this one depends.
     dependents: List of DependencyTreeNodes that depend on this one.
  """

  class CircularException(Exception):
    pass

  def __init__(self, ref):
    self.ref = ref
    self.dependencies = []
    self.dependents = []

  def FlattenToList(self):
    # flat_list is the sorted list of dependencies - actually, the list items
    # are the "ref" attributes of DependencyTreeNodes.  Every target will
    # appear in flat_list after all of its dependencies, and before all of its
    # dependents.
    flat_list = []

    # in_degree_zeros is the list of DependencyTreeNodes that have no
    # dependencies not in flat_list.  Initially, it is a copy of the children
    # of this node, because when the graph was built, nodes with no
    # dependencies were made implicit dependents of the root node.
    in_degree_zeros = self.dependents[:]

    while in_degree_zeros:
      # Nodes in in_degree_zeros have no dependencies not in flat_list, so they
      # can be appended to flat_list.  Take these nodes out of in_degree_zeros
      # as work progresses, so that the next node to process from the list can
      # always be accessed at a consistent position.
      node = in_degree_zeros.pop(0)
      flat_list.append(node.ref)

      # Look at dependents of the node just added to flat_list.  Some of them
      # may now belong in in_degree_zeros.
      for node_dependent in node.dependents:
        is_in_degree_zero = True
        for node_dependent_dependency in node_dependent.dependencies:
          if not node_dependent_dependency.ref in flat_list:
            # The dependent one or more dependencies not in flat_list.  There
            # will be more chances to add it to flat_list when examining
            # it again as a dependent of those other dependencies, provided
            # that there are no cycles.
            is_in_degree_zero = False
            break

        if is_in_degree_zero:
          # All of the dependent's dependencies are already in flat_list.  Add
          # it to in_degree_zeros where it will be processed in a future
          # iteration of the outer loop.
          in_degree_zeros.append(node_dependent)

    return flat_list

  def DeepDependents(self, dependents=None):
    if dependents == None:
      dependents = []

    for dependent in self.dependents:
      if dependent.ref not in dependents:
        # Put each dependent as well as its dependents into the list.
        dependents.append(dependent.ref)
        dependent.DeepDependents(dependents)

    return dependents


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
  flat_list = root_node.FlattenToList()

  # If there's anything left unvisited, there must be a circular dependency
  # (cycle).  If you need to figure out what's wrong, look for elements of
  # targets that are not in flat_list.
  if len(flat_list) != len(targets):
    raise DependencyTreeNode.CircularException, \
        'Some targets not reachable, cycle in dependency graph detected'

  return [dependency_nodes, flat_list]


def RelativePath(path, relative_to):
  # Assuming both |path| and |relative_to| are relative to the current
  # directory, returns a relative path that identifies path relative to
  # relative_to.

  if os.path.isabs(path) != os.path.isabs(relative_to):
    # If one of the paths is absolute, both need to be absolute.
    path = os.path.abspath(path)
    relative_to = os.path.abspath(relative_to)
  else:
    # If both paths are relative, make sure they're normalized.
    path = os.path.normpath(path)
    relative_to = os.path.normpath(relative_to)

  # Split the paths into components.  As a special case, if either path is
  # the current directory, use an empty list as a split-up path.  This must be
  # done because the code that follows is unprepared to deal with "." meaning
  # "current directory" and it will instead assume that it's a subdirectory,
  # which is wrong.  It's possible to wind up with "." when it's passed to this
  # function, for example, by taking the dirname of a relative path in the
  # current directory.
  if path == os.path.curdir:
    path_split = []
  else:
    path_split = path.split(os.path.sep)

  if relative_to == os.path.curdir:
    relative_to_split = []
  else:
    relative_to_split = relative_to.split(os.path.sep)

  # Determine how much of the prefix the two paths share.
  prefix_len = len(os.path.commonprefix([path_split, relative_to_split]))

  # Put enough ".." components to back up out of relative_to to the common
  # prefix, and then append the part of path_split after the common prefix.
  relative_split = [os.path.pardir] * (len(relative_to_split) - prefix_len) + \
                   path_split[prefix_len:]

  # Turn it back into a string and we're done.
  return os.path.join(*relative_split)


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
        path = os.path.normpath(os.path.join(
            RelativePath(os.path.dirname(fro_file), os.path.dirname(to_file)),
            item))
        to.append(path)
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
      is_paths = k in ['include_dirs', 'sources', 'xcode_framework_dirs']
      MergeLists(to[k], v, to_file, fro_file, is_paths)
    else:
      raise TypeError, \
          'Attempt to merge dict value of unsupported type ' + \
          v.__class__.__name__ + ' for key ' + k


def ProcessRules(name, the_dict):
  """Process regular expression and exclusion-based rules on lists.

  An exclusion list is in a dict key named with a trailing "!", like
  "sources!".  Every item in such a list is removed from the associated
  main list, which in this example, would be "sources".  Removed items are
  placed into a "sources_excluded" list in the dict.

  Regular expression (regex) rules are contained in dict keys named with a
  trailing "/", such as "sources/" to operate on the "sources" list.  Regex
  rules in a dict take the form:
    'sources/': [ ['exclude', '_(linux|mac|win)\\.cc$'] ],
                  ['include', '_mac\\.cc$'] ],
  The first rule says to exclude all files ending in _linux.cc, _mac.cc, and
  _win.cc.  The second rule then includes all files ending in _mac.cc that
  are now or were once in the "sources" list.  Items matching an "exclude"
  rule are subject to the same processing as would occur if they were listed
  by name in an exclusion list (ending in "!").  Items matching an "include"
  rule are brought back into the main list if previously excluded by an
  exclusion list or exclusion regex rule, and are protected from future removal
  by such exclusion lists and rules.
  """

  # Look through the dictionary for any lists whose keys end in '!' or '/'.
  # These are lists that will be treated as exclude lists and regular
  # expression-based exclude/include lists.  Collect the lists that are
  # needed first, looking for the lists that they operate on, and assemble
  # then into |lists|.  This is done in a separate loop up front, because
  # the _included and _excluded keys need to be added to the_dict, and that
  # can't be done while iterating through it.

  lists = []
  del_lists = []
  for key, value in the_dict.iteritems():
    operation = key[-1]
    if operation != '!' and operation != '/':
      continue

    if not isinstance(value, list):
      raise ValueError, name + ' key ' + key + ' must be list, not ' + \
                        value.__class__.__name__

    list_key = key[:-1]
    if list_key not in the_dict:
      # This happens when there's a list like "sources!" but no corresponding
      # "sources" list.  Since there's nothing for it to operate on, queue up
      # the "sources!" list for deletion now.
      del_lists.append(key)
      continue

    if not isinstance(the_dict[list_key], list):
      raise ValueError, name + ' key ' + list_key + \
                        ' must be list, not ' + \
                        value.__class__.__name__ + ' when applying ' + \
                        {'!': 'exclusion', '/': 'regex'}[operation]

    if not list_key in lists:
      lists.append(list_key)

  # Delete the lists that are known to be unneeded at this point.
  for del_list in del_lists:
    del the_dict[del_list]

  for list_key in lists:
    # Initialize the _excluded and _included lists now, so that the code that
    # needs to use them can perform list operations without needing to do its
    # own lazy initialization.  Lists that are unneeded will be deleted at
    # the end.
    excluded_key = list_key + '_excluded'
    included_key = list_key + '_included'
    for k in [excluded_key, included_key]:
      if k in the_dict:
        raise KeyError, name + ' key ' + k + ' must not be present prior ' + \
                        ' to applying exclusion/regex rules for ' + list_key
      the_dict[k] = []

    # Note that exclude_key ("sources!") is different from excluded_key
    # ("sources_excluded").  Since exclude_key is just a very temporary
    # variable used on the next few lines, this isn't a huge problem, but
    # be careful!
    exclude_key = list_key + '!'
    if exclude_key in the_dict:
      for exclude_item in the_dict[exclude_key]:
        if exclude_item in the_dict[included_key]:
          # The exclude_item was already preserved and is "golden", don't touch
          # it.
          continue

        # The exclude_item may appear in the list more than once, so loop to
        # remove it.  That's "while exclude_item in", not "for exclude_item
        # in."  Crucial difference.
        removed = False
        while exclude_item in the_dict[list_key]:
          removed = True
          the_dict[list_key].remove(exclude_item)

        # If anything was removed, add it to the _excluded list.
        if removed:
          if not exclude_item in the_dict[excluded_key]:
            the_dict[excluded_key].append(exclude_item)

      # The "whatever!" list is no longer needed, dump it.
      del the_dict[exclude_key]

    regex_key = list_key + '/'
    if regex_key in the_dict:
      for regex_item in the_dict[regex_key]:
        [action, pattern] = regex_item
        pattern_re = re.compile(pattern)

        # Instead of writing "for list_item in the_dict[list_key]", write a
        # while loop.  Iteration with a for loop won't work, because code that
        # follows manipulates the_dict[list_key].  Be careful with that "index"
        # variable.
        index = 0
        while index < len(the_dict[list_key]):
          list_item = the_dict[list_key][index]
          if pattern_re.search(list_item):
            # Regular expression match.

            if action == 'exclude':
              if list_item in the_dict[included_key]:
                # regex_item says to remove list_item from the list, but
                # something else already said to include it, so leave it
                # alone and proceed to the next item in the list.
                index = index + 1
                continue

              del the_dict[list_key][index]

              # Add it to the excluded list if it's not already there.
              if not list_item in the_dict[excluded_key]:
                the_dict[excluded_key].append(list_item)

              # continue without incrementing |index|.  The next object to
              # look at, if any, moved into the index of the object that was
              # just removed.
              continue

            elif action == 'include':
              # Here's a list_item that's in list and needs to stay there.
              # Add it to the golden list of happy items to keep, and nothing
              # will be able to exclude it in the future.
              if not list_item in the_dict[included_key]:
                the_dict[included_key].append(list_item)

            else:
              # This is an action that doesn't make any sense.
              raise ValueError, 'Unrecognized action ' + action + ' in ' + \
                                name + ' key ' + key

          # Advance to the next list item.
          index = index + 1

        if action == 'include':
          # Items matching an include pattern may have already been excluded.
          # Resurrect any that are found.  The while loop is needed again
          # because the excluded list will be manipulated.
          index = 0
          while index < len(the_dict[excluded_key]):
            excluded_item = the_dict[excluded_key][index]
            if pattern_re.search(excluded_item):
              # Yup, this is one.  Take it out of the excluded list and put
              # it back into the main list AND the golden included list, so
              # that nothing else can touch it.  Unfortunately, the best that
              # can be done at this point is an append, since there's no way
              # to know where in the list it came from.  TODO(mark): There
              # are possible solutions to this problem, like tracking
              # include/exclude status in a parallel list and only doing the
              # deletions after processing all of the rules.
              del the_dict[excluded_key][index]
              the_dict[list_key].append(excluded_item)
              if not excluded_item in the_dict[included_key]:
                the_dict[included_key].append(excluded_item)
            else:
              # Only move to the next index if there was no match.  If there
              # was a match, the item was deleted, and the next item to look
              # at is at the same index as the item just examined.
              index = index + 1

      # The "whatever/" list is no longer needed, dump it.
      del the_dict[regex_key]

    # Dump the "included" list, which is never needed since evertyhing in it
    # is already in the main list.  Dump the "excluded" list if it's empty.
    del the_dict[included_key]
    if len(the_dict[excluded_key]) == 0:
      del the_dict[excluded_key]


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
    options.format = {'darwin': 'xcodeproj',
                      'win32':  'msvs',
                      'cygwin': 'msvs'}[sys.platform]
  if not build_files:
    build_files = FindBuildFiles()
  if not build_files:
    print >>sys.stderr, (usage + '\n\n%s: error: no build_file') % \
                        (my_name, my_name)
    return 1

  generator_name = 'gyp.generator.' + options.format
  # These parameters are passed in order (as opposed to by key)
  # because ActivePython cannot handle key parameters to __import__.
  generator = __import__(generator_name, globals(), locals(), generator_name)
  variables_hack.extend(generator.variables_hack)

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
  [dependency_nodes, flat_list] = BuildDependencyList(targets)

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

    build_file = BuildFileAndTarget('', target)[0]
    for dependency in target_dict['dependencies']:
      # The name is already relative, so use ''.
      [dep_build_file, dep_target_unq, dep_target_q] = \
          BuildFileAndTarget('', dependency)
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
      dependents = dependency_nodes[target].DeepDependents()
      # TODO(mark): Probably want dependents to be sorted in the order that
      # they appear in flat_list.

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

  # Apply "post"/"late"/"target" variable expansions.
  for target in flat_list:
    [build_file, target_unq] = BuildFileAndTarget('', target)[0:2]
    target_dict = targets[target]
    ProcessVariableExpansionsInDict(target_dict, True)

  # TODO(mark): Apply "post"/"late"/"target" conditionals.

  # Apply exclude (!) and regex (/) rules.
  for target in flat_list:
    [build_file, target_unq] = BuildFileAndTarget('', target)[0:2]
    target_dict = targets[target]
    ProcessRules(target, target_dict)

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
