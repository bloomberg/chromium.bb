#!/usr/bin/python

import copy
import gyp.common
import optparse
import os.path
import re
import shlex
import subprocess


# A list of types that are treated as linkable.
linkable_types = ['executable', 'shared_library']

# A list of sections that contain pathnames.  You should probably call
# IsPathSection instead, which has other checks.
path_sections = [
  'include_dirs',
  'inputs',
  'sources',
  'msvs_props',
  'outputs',
  'xcode_framework_dirs',
]


def IsPathSection(section):
  if section in path_sections or \
     section.endswith('_dir') or section.endswith('_dirs') or \
     section.endswith('_path') or section.endswith('_paths'):
    return True
  return False


def ExceptionAppend(e, msg):
  if not e.args:
    e.args = [msg]
  elif len(e.args) == 1:
    e.args = [str(e.args[0]) + ' ' + msg]
  else:
    e.args = [str(e.args[0]) + ' ' + msg, e.args[1:]]


def LoadOneBuildFile(build_file_path, data, variables, includes, is_target):
  if build_file_path in data:
    return data[build_file_path]

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

  data[build_file_path] = build_file_data

  # Scan for includes and merge them in.
  try:
    if is_target:
      LoadBuildFileIncludesIntoDict(build_file_data, build_file_path, data,
                                    variables, includes)
    else:
      LoadBuildFileIncludesIntoDict(build_file_data, build_file_path, data,
                                    variables)
  except Exception, e:
    ExceptionAppend(e, 'while reading includes of ' + build_file_path)
    raise

  return build_file_data


def LoadBuildFileIncludesIntoDict(subdict, subdict_path, data,
                                  variables, includes=None):
  includes_list = []
  if includes != None:
    includes_list.extend(includes)
  if 'includes' in subdict:
    for include in subdict['includes']:
      # "include" is specified relative to subdict_path, so compute the real
      # path to include by appending the provided "include" to the directory
      # in which subdict_path resides.
      relative_include = \
          os.path.normpath(os.path.join(os.path.dirname(subdict_path), include))
      includes_list.append(relative_include)
    # Unhook the includes list, it's no longer needed.
    del subdict['includes']

  # Merge in the included files.
  for include in includes_list:
    MergeDicts(subdict, LoadOneBuildFile(include, data, variables, None, False),
               subdict_path, include)

  # Recurse into subdictionaries.
  for k, v in subdict.iteritems():
    if v.__class__ == dict:
      LoadBuildFileIncludesIntoDict(v, subdict_path, data, variables, None)
    elif v.__class__ == list:
      LoadBuildFileIncludesIntoList(v, subdict_path, data, variables)


# This recurses into lists so that it can look for dicts.
def LoadBuildFileIncludesIntoList(sublist, sublist_path, data, variables):
  for item in sublist:
    if item.__class__ == dict:
      LoadBuildFileIncludesIntoDict(item, sublist_path, data, variables, None)
    elif item.__class__ == list:
      LoadBuildFileIncludesIntoList(item, sublist_path, data, variables)


# TODO(mark): I don't love this name.  It just means that it's going to load
# a build file that contains targets and is expected to provide a targets dict
# that contains the targets...
def LoadTargetBuildFile(build_file_path, data, variables, includes):
  if build_file_path in data:
    # Already loaded.
    return

  build_file_data = LoadOneBuildFile(build_file_path, data, variables,
                                     includes, True)

  # Apply "pre"/"early" variable expansions and condition evaluations.
  ProcessVariablesAndConditionsInDict(build_file_data, False, variables)

  # Look at each project's target_defaults dict, and merge settings into
  # targets.
  if 'target_defaults' in build_file_data:
    index = 0
    while index < len(build_file_data['targets']):
      # This procedure needs to give the impression that target_defaults is
      # used as defaults, and the individual targets inherit from that.
      # The individual targets need to be merged into the defaults.  Make
      # a deep copy of the defaults for each target, merge the target dict
      # as found in the input file into that copy, and then hook up the
      # copy with the target-specific data merged into it as the replacement
      # target dict.
      old_target_dict = build_file_data['targets'][index]
      new_target_dict = copy.deepcopy(build_file_data['target_defaults'])
      MergeDicts(new_target_dict, old_target_dict,
                 build_file_path, build_file_path)
      build_file_data['targets'][index] = new_target_dict
      index = index + 1

  # Look for dependencies.  This means that dependency resolution occurs
  # after "pre" conditionals and variable expansion, but before "post" -
  # in other words, you can't put a "dependencies" section inside a "post"
  # conditional within a target.

  if 'targets' in build_file_data:
    for target_dict in build_file_data['targets']:
      if 'dependencies' not in target_dict:
        continue
      for dependency in target_dict['dependencies']:
        other_build_file = \
            gyp.common.BuildFileAndTarget(build_file_path, dependency)[0]
        LoadTargetBuildFile(other_build_file, data, variables, includes)

  return data


early_variable_re = re.compile('((<@?)\((.*?)\))')
late_variable_re = re.compile('(([>!]@?)\((.*?)\))')

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
      # match[0] is the substring to look for, match[1] is the character code
      # for the replacement type (< > ! <@ >@ !@), and match[2] is the name of
      # the variable (< >) or command to run (!).

      # expand_char is the first character of the replacement type: < > !
      expand_char = match[1][0]

      # expand_to_list is true if an @ variant was being used.  In that case,
      # the expansion should result in a list.  This can only happen if the
      # only expansion present is the list expansion, hence the input ==
      # match[0] check.
      # Also note that the caller to be expecting a list in return, and not
      # all callers do because not all are working in list context.
      if len(match[1]) == 2 and input == match[0]:
        expand_to_list = True
      else:
        expand_to_list = False

      if expand_char == '!':
        p = subprocess.Popen(match[2], shell=True,
                             stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        (p_stdout, p_stderr) = p.communicate()

        if p.wait() != 0:
          # Simulate check_call behavior by reusing its exception.
          raise subprocess.CalledProcessError(p.returncode, match[2])
        if p_stderr:
          raise Exception, match[2] + ' produced stderr:\n' + p_stderr

        replacement = p_stdout.rstrip()
      else:
        if not match[2] in variables:
          raise KeyError, 'Undefined variable ' + match[2] + ' in ' + input
        replacement = variables[match[2]]

      if isinstance(replacement, list):
        for item in replacement:
          if not isinstance(item, str) and not isinstance(item, int):
            raise TypeError, 'Variable ' + match[2] + ' must expand to a ' + \
                             ' string or list of strings; list contains a ' + \
                             item.__class__.__name__
        # Run through the list and handle variable expansions in it.  Since
        # the list is guaranteed not to contain dicts, this won't do anything
        # with conditions sections.
        #
        # TODO(mark): I think this should be made more general: any time an
        # expansion is done, if there are more expandable tokens left in the
        # output, additional expansion phases should be done.  It should not
        # be effective solely for lists.
        ProcessVariablesAndConditionsInList(replacement, is_late, variables)
      elif not isinstance(replacement, str) and \
           not isinstance(replacement, int):
            raise TypeError, 'Variable ' + match[2] + ' must expand to a ' + \
                             ' string or list of strings; found a ' + \
                             replacement.__class__.__name__

      if expand_to_list:
        # Expanding in list context.  It's guaranteed that there's only one
        # replacement to do in |input| and that it's this replacement.  See
        # above.
        if isinstance(replacement, list):
          # If it's already a list, make a copy.
          output = replacement[:]
        else:
          # Split it the same way sh would split arguments.
          output = shlex.split(replacement)
      else:
        # Expanding in string context.
        if isinstance(replacement, list):
          # When expanding a list into string context, turn the list items
          # into a string in a way that will work with a subprocess call.
          #
          # TODO(mark): list2cmdline isn't completely correct.  This should
          # call a generator-provided function that observes the proper
          # list-to-argument quoting rules on a specific platform.  Even
          # working solely in a POSIX environment, list2cmdline is not ideal.
          # It doesn't, but should, escape the | character.  It doesn't quote
          # list items, so if a list item contains a variable expansion, the
          # shell will expand the variable to multiple arguments, where
          # code that comes through here will most likely be expecting a
          # single argument.
          output = re.sub(re.escape(match[0]),
                                    subprocess.list2cmdline(replacement),
                                    output)
        else:
          # Expanding a string into string context is easy, just replace it.
          output = re.sub(re.escape(match[0]), replacement, output)

  return output


def ProcessConditionsInDict(the_dict, is_late, variables):
  # If the_dict has a "conditions" key (is_late == False) or a
  # "target_conditons" key (is_late == True), its value is treated as a list.
  # Each item in the list consists of cond_expr, a string expression evaluated
  # as the condition, and true_dict, a dict that will be merged into the_dict
  # if cond_expr evaluates to true.  Optionally, a third item, false_dict, may
  # be present.  false_dict is merged into the_dict if cond_expr evaluates to
  # false.  This function will recurse into true_dict or false_dict as
  # appropriate before merging it into the_dict, allowing for nested
  # conditions.

  if not is_late:
    conditions_key = 'conditions'
  else:
    conditions_key = 'target_conditions'

  if not conditions_key in the_dict:
    return

  conditions_list = the_dict[conditions_key]
  # Unhook the conditions list, it's no longer needed.
  del the_dict[conditions_key]

  for condition in conditions_list:
    if not isinstance(condition, list):
      raise TypeError, conditions_key + ' must be a list'
    if len(condition) != 2 and len(condition) != 3:
      # It's possible that condition[0] won't work in which case this won't
      # attempt will raise its own IndexError.  That's probably fine.
      raise IndexError, conditions_key + ' ' + condition[0] + \
                        ' must be length 2 or 3, not ' + len(condition)

    [cond_expr, true_dict] = condition[0:2]
    false_dict = None
    if len(condition) == 3:
      false_dict = condition[2]

    # TODO(mark): Catch exceptions here to re-raise after providing a little
    # more error context.  The name of the file being processed and the
    # condition in quesiton might be nice.
    if eval(cond_expr, {'__builtins__': None}, variables):
      merge_dict = true_dict
    else:
      merge_dict = false_dict

    if merge_dict != None:
      # Recurse to pick up nested conditions.
      ProcessConditionsInDict(merge_dict, is_late, variables)

      # For now, it's OK to pass '', '' for the build files because everything
      # comes from the same build file and everything is already relative to
      # the same place.  If the path to the build file being processed were to
      # be available, which might be nice for error reporting, it should be
      # passed in these two arguments.
      MergeDicts(the_dict, merge_dict, '', '')


def LoadAutomaticVariablesFromDict(variables, the_dict):
  # Any keys with plain string values in the_dict become automatic variables.
  # The variable name is the key name with a "_" character prepended.
  for key, value in the_dict.iteritems():
    if isinstance(value, str) or isinstance(value, int) or \
       isinstance(value, list):
      variables['_' + key] = value


def LoadVariablesFromVariablesDict(variables, the_dict):
  # Any keys in the_dict's "variables" dict, if it has one, becomes a
  # variable.  The variable name is the key name in the "variables" dict.
  if 'variables' in the_dict:
    variables.update(the_dict['variables'])


def ProcessVariablesAndConditionsInDict(the_dict, is_late, variables):
  """Handle all variable and command expansion and conditional evaluation.

  This function is the public entry point for all variable expansions and
  conditional evaluations.
  """

  # Save a copy of the variables dict before loading automatics or the
  # variables dict.  After performing steps that may result in either of
  # these changing, the variables can be reloaded from the copy.
  variables_copy = variables.copy()
  LoadAutomaticVariablesFromDict(variables, the_dict)

  if 'variables' in the_dict:
    # Handle the associated variables dict first, so that any variable
    # references within can be resolved prior to using them as variables.
    # Pass a copy of the variables dict to avoid having it be tainted.
    # Otherwise, it would have extra automatics added for everything that
    # should just be an ordinary variable in this scope.
    ProcessVariablesAndConditionsInDict(the_dict['variables'], is_late,
                                        variables.copy())

  LoadVariablesFromVariablesDict(variables, the_dict)

  for key, value in the_dict.iteritems():
    # Skip "variables", which was already processed if present.
    if key != 'variables' and isinstance(value, str):
      expanded = ExpandVariables(value, is_late, variables)
      if not isinstance(expanded, str):
        raise ValueError, \
              'Variable expansion in this context permits strings only, ' + \
              'found ' + expanded.__class__.__name__ + ' for ' + key
      the_dict[key] = expanded

  # Variable expansion may have resulted in changes to automatics.  Reload.
  # TODO(mark): Optimization: only reload if no changes were made.
  variables = variables.copy()
  LoadAutomaticVariablesFromDict(variables, the_dict)
  LoadVariablesFromVariablesDict(variables, the_dict)

  # Process conditions in this dict.  This is done after variable expansion
  # so that conditions may take advantage of expanded variables.  For example,
  # if the_dict contains:
  #   {'type':       '<(library_type)',
  #    'conditions': [['_type=="static_library"', { ... }]]}, 
  # _type, as used in the condition, will only be set to the value of
  # library_type if variable expansion is performed before condition
  # processing.  However, condition processing should occur prior to recursion
  # so that variables (both automatic and "variables" dict type) may be
  # adjusted by conditions sections, merged into the_dict, and have the
  # intended impact on contained dicts.
  #
  # This arrangement means that a "conditions" section containing a "variables"
  # section will only have those variables effective in subdicts, not in
  # the_dict.  The workaround is to put a "conditions" section within a
  # "variables" section.  For example:
  #   {'conditions': [['os=="mac"', {'variables': {'define': 'IS_MAC'}}]],
  #    'defines':    ['<(define)'],
  #    'my_subdict': {'defines': ['<(define)']}},
  # will not result in "IS_MAC" being appended to the "defines" list in the
  # current scope but would result in it being appended to the "defines" list
  # within "my_subdict".  By comparison:
  #   {'variables': {'conditions': [['os=="mac"', {'define': 'IS_MAC'}]]},
  #    'defines':    ['<(define)'],
  #    'my_subdict': {'defines': ['<(define)']}},
  # will append "IS_MAC" to both "defines" lists.

  ProcessConditionsInDict(the_dict, is_late, variables)

  # Conditional processing may have resulted in changes to automatics or the
  # variables dict.  Reload.
  # TODO(mark): Optimization: only reload if no changes were made.
  # ProcessConditonsInDict could return a value indicating whether changes
  # were made.
  variables = variables.copy()
  LoadAutomaticVariablesFromDict(variables, the_dict)
  LoadVariablesFromVariablesDict(variables, the_dict)

  # Recurse into child dicts, or process child lists which may result in
  # further recursion into descendant dicts.
  for key, value in the_dict.iteritems():
    # Skip "variables" and string values, which were already processed if
    # present.
    if key == 'variables' or isinstance(value, str):
      continue
    if isinstance(value, dict):
      # Pass a copy of the variables dict so that subdicts can't influence
      # parents.
      ProcessVariablesAndConditionsInDict(value, is_late, variables.copy())
    elif isinstance(value, list):
      # The list itself can't influence the variables dict, and
      # ProcessVariablesAndConditionsInList will make copies of the variables
      # dict if it needs to pass it to something that can influence it.  No
      # copy is necessary here.
      ProcessVariablesAndConditionsInList(value, is_late, variables)
    elif not isinstance(value, int):
      raise TypeError, 'Unknown type ' + value.__class__.__name__ + \
                       ' for ' + key


def ProcessVariablesAndConditionsInList(the_list, is_late, variables):
  # Iterate using an index so that new values can be assigned into the_list.
  index = 0
  while index < len(the_list):
    item = the_list[index]
    if isinstance(item, dict):
      # Make a copy of the variables dict so that it won't influence anything
      # outside of its own scope.
      ProcessVariablesAndConditionsInDict(item, is_late, variables.copy())
    elif isinstance(item, list):
      ProcessVariablesAndConditionsInList(item, is_late, variables)
    elif isinstance(item, str):
      expanded = ExpandVariables(item, is_late, variables)
      if isinstance(expanded, str):
        the_list[index] = expanded
      elif isinstance(expanded, list):
        del the_list[index]
        for expanded_item in expanded:
          the_list.insert(index, expanded_item)
          index = index + 1

        # index now identifies the next item to examine.  Continue right now
        # without falling into the index increment below.
        continue
      else:
        raise ValueError, \
              'Variable expansion in this context permits strings and ' + \
              'lists only, found ' + expanded.__class__.__name__ + ' at ' + \
              index
    elif not isinstance(item, int):
      raise TypeError, 'Unknown type ' + item.__class__.__name__ + \
                       ' at index ' + index
    index = index + 1


class DependencyGraphNode(object):
  """

  Attributes:
    ref: A reference to an object that this DependencyGraphNode represents.
    dependencies: List of DependencyGraphNodes on which this one depends.
    dependents: List of DependencyGraphNodes that depend on this one.
  """

  class CircularException(Exception):
    pass

  def __init__(self, ref):
    self.ref = ref
    self.dependencies = []
    self.dependents = []

  def FlattenToList(self):
    # flat_list is the sorted list of dependencies - actually, the list items
    # are the "ref" attributes of DependencyGraphNodes.  Every target will
    # appear in flat_list after all of its dependencies, and before all of its
    # dependents.
    flat_list = []

    # in_degree_zeros is the list of DependencyGraphNodes that have no
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

  def DirectDependencies(self, dependencies=None):
    """Returns a list of just direct dependencies."""
    if dependencies == None:
      dependencies = []

    for dependency in self.dependencies:
      # Check for None, corresponding to the root node.
      if dependency.ref != None and dependency.ref not in dependencies:
        dependencies.append(dependency.ref)

    return dependencies

  def _AddImportedDependencies(self, targets, dependencies=None):
    """Given a list of direct dependencies, adds indirect dependencies that
    other dependencies have declared to export their settings.

    This method does not operate on self.  Rather, it operates on the list
    of dependencies in the |dependencies| argument.  For each dependency in
    that list, if any declares that it exports the settings of one of its
    own dependencies, those dependencies whose settings are "passed through"
    are added to the list.  As new items are added to the list, they too will
    be processed, so it is possible to import settings through multiple levels
    of dependencies.

    This method is not terribly useful on its own, it depends on being
    "primed" with a list of direct dependencies such as one provided by
    DirectDependencies.  DirectAndImportedDependencies is intended to be the
    public entry point.
    """

    if dependencies == None:
      dependencies = []

    index = 0
    while index < len(dependencies):
      dependency = dependencies[index]
      dependency_dict = targets[dependency]
      for imported_dependency in \
          dependency_dict.get('export_dependent_settings', []):
        # Add any dependencies whose settings should be imported to the list
        # if not already present.  Newly-added items will be checked for
        # their own imports when the list iteration reaches them.
        # Rather than simply appending new items, insert them after the
        # dependency that exported them.  This is done to more closely match
        # the depth-first method used by DeepDependencies.
        add_index = 1
        if imported_dependency not in dependencies:
          dependencies.insert(index + add_index, imported_dependency)
          add_index = add_index + 1
      index = index + 1

    return dependencies

  def DirectAndImportedDependencies(self, targets, dependencies=None):
    """Returns a list of a target's direct dependencies and all indirect
    dependencies that a dependency has advertised settings should be exported
    through the dependency for.
    """

    dependencies = self.DirectDependencies(dependencies)
    return self._AddImportedDependencies(targets, dependencies)

  def DeepDependencies(self, dependencies=None):
    """Returns a list of all of a target's dependencies, recursively."""
    if dependencies == None:
      dependencies = []

    for dependency in self.dependencies:
      # Check for None, corresponding to the root node.
      if dependency.ref != None and dependency.ref not in dependencies:
        dependencies.append(dependency.ref)
        dependency.DeepDependencies(dependencies)

    return dependencies

  def LinkDependencies(self, targets, dependencies=None, initial=True):
    """Returns a list of dependency targets that are linked into this target.

    This function has a split personality, depending on the setting of
    |initial|.  Outside calers should always leave |initial| at its default
    setting.

    When adding a target to the list of dependencies, this function will
    recurse into itself with |initial| set to False, to collect depenedencies
    that are linked into the linkable target for which the list is being built.
    """
    if dependencies == None:
      dependencies = []

    # Check for None, corresponding to the root node.
    if self.ref == None:
      return dependencies

    # It's kind of sucky that |targets| has to be passed into this function,
    # but that's presently the easiest way to access the target dicts so that
    # this function can find target types.

    target_type = targets[self.ref]['type']
    is_linkable = target_type in linkable_types

    if initial and not is_linkable:
      # If this is the first target being examined and it's not linkable,
      # return an empty list of link dependencies, because the link
      # dependencies are intended to apply to the target itself (initial is
      # True) and this target won't be linked.
      return dependencies

    # Executables are already fully and finally linked.  Nothing else can be
    # a link dependency of an executable, there can only be dependencies in
    # the sense that a dependent target might run an executable.
    if not initial and target_type == 'executable':
      return dependencies

    # The target is linkable, add it to the list of link dependencies.
    if self.ref not in dependencies:
      if target_type != 'none':
        # Special case: "none" type targets don't produce any linkable products
        # and shouldn't be exposed as link dependencies, although dependencies
        # of "none" type targets may still be link dependencies.
        dependencies.append(self.ref)
      if initial or not is_linkable:
        # If this is a subsequent target and it's linkable, don't look any
        # further for linkable dependencies, as they'll already be linked into
        # this target linkable.  Always look at dependencies of the initial
        # target, and always look at dependencies of non-linkables.
        for dependency in self.dependencies:
          dependency.LinkDependencies(targets, dependencies, False)

    return dependencies


def BuildDependencyList(targets):
  other_dependency_lists = ['export_dependent_settings', 'hard_dependencies']

  # Create a DependencyGraphNode for each target.  Put it into a dict for easy
  # access.
  dependency_nodes = {}
  for target, spec in targets.iteritems():
    if not target in dependency_nodes:
      dependency_nodes[target] = DependencyGraphNode(target)

  # Set up the dependency links.  Targets that have no dependencies are treated
  # as dependent on root_node.
  root_node = DependencyGraphNode(None)
  for target, spec in targets.iteritems():
    target_node = dependency_nodes[target]
    target_build_file = gyp.common.BuildFileAndTarget('', target)[0]
    if not 'dependencies' in spec or len(spec['dependencies']) == 0:
      target_node.dependencies = [root_node]
      root_node.dependents.append(target_node)
      # If there are no dependencies, there can't be anything in the other
      # lists that need to contain things that must already be in dependencies.
      for other in other_dependency_lists:
        assert len(spec.get(other, [])) == 0
    else:
      dependencies = spec['dependencies']
      for index in xrange(0, len(dependencies)):
        dependency = dependencies[index]
        dependency = gyp.common.QualifiedTarget(target_build_file, dependency)
        # Store the qualified name of the target even if it wasn't originally
        # qualified in the dict.  Others will find this useful as well.
        dependencies[index] = dependency
        dependency_node = dependency_nodes[dependency]
        target_node.dependencies.append(dependency_node)
        dependency_node.dependents.append(target_node)

      # Rewrite other dependency lists using qualified names everywhere,
      # too.  Make sure that anything showing up in one of these other lists
      # is actually a dependency.
      for other in other_dependency_lists:
        other_dependencies = spec.get(other, [])
        for index in xrange(0, len(other_dependencies)):
          dependency = other_dependencies[index]
          dependency = gyp.common.QualifiedTarget(target_build_file, dependency)
          assert dependency in dependencies
          other_dependencies[index] = dependency

  # Take the root node out of the list because it doesn't correspond to a real
  # target.
  flat_list = root_node.FlattenToList()

  # If there's anything left unvisited, there must be a circular dependency
  # (cycle).  If you need to figure out what's wrong, look for elements of
  # targets that are not in flat_list.
  if len(flat_list) != len(targets):
    raise DependencyGraphNode.CircularException, \
        'Some targets not reachable, cycle in dependency graph detected'

  return [dependency_nodes, flat_list]


def DoDependentSettings(key, flat_list, targets, dependency_nodes):
  # key should be one of all_dependent_settings, direct_dependent_settings,
  # or link_settings.

  for target in flat_list:
    target_dict = targets[target]
    build_file = gyp.common.BuildFileAndTarget('', target)[0]

    if key == 'all_dependent_settings':
      dependencies = dependency_nodes[target].DeepDependencies()
    elif key == 'direct_dependent_settings':
      dependencies = \
          dependency_nodes[target].DirectAndImportedDependencies(targets)
    elif key == 'link_settings':
      dependencies = dependency_nodes[target].LinkDependencies(targets)
    else:
      raise KeyError, "DoDependentSettings doesn't know how to determine " + \
                      'dependencies for ' + key

    for dependency in dependencies:
      dependency_dict = targets[dependency]
      if not key in dependency_dict:
        continue
      dependency_build_file = gyp.common.BuildFileAndTarget('', dependency)[0]
      MergeDicts(target_dict, dependency_dict[key],
                 build_file, dependency_build_file)


def AdjustStaticLibraryDependencies(flat_list, targets, dependency_nodes):
  # Recompute target "dependencies" properties.  For each static library
  # target, remove "dependencies" entries referring to other static libraries,
  # unless the relationship is also listed in "hard_dependencies".  For each
  # linkable target, add a "dependencies" entry referring to all of the
  # target's computed list of link dependencies (including static libraries
  # if no such entry is already present.
  for target in flat_list:
    target_dict = targets[target]
    target_type = target_dict['type']

    if target_type == 'static_library':
      if not 'dependencies' in target_dict:
        continue

      hard_dependencies = target_dict.get('hard_dependencies', [])
      index = 0
      while index < len(target_dict['dependencies']):
        dependency = target_dict['dependencies'][index]
        dependency_dict = targets[dependency]
        if dependency_dict['type'] == 'static_library' and \
           dependency not in hard_dependencies:
          # A static library should not depend on another static library unless
          # the dependency relationship is "hard," which should only be done
          # when a dependent relies on some side effect other than just the
          # build product, like a rule or action output.  Take the dependency
          # out of the list, and don't increment index because the next
          # dependency to analyze will shift into the index formerly occupied
          # by the one being removed.
          del target_dict['dependencies'][index]
        else:
          index = index + 1

      # If the dependencies list is empty, it's not needed, so unhook it.
      if len(target_dict['dependencies']) == 0:
        del target_dict['dependencies']

    elif target_type in linkable_types:
      # Get a list of dependency targets that should be linked into this
      # target.  Add them to the dependencies list if they're not already
      # present.

      link_dependencies = dependency_nodes[target].LinkDependencies(targets)
      for dependency in link_dependencies:
        if dependency == target:
          continue
        if not 'dependencies' in target_dict:
          target_dict['dependencies'] = []
        if not dependency in target_dict['dependencies']:
          target_dict['dependencies'].append(dependency)


def MakePathRelative(to_file, fro_file, item):
  # If item is a relative path, it's relative to the build file dict that it's
  # coming from.  Fix it up to make it relative to the build file dict that
  # it's going into.
  # Exception: any |item| that begins with these special characters is
  # returned without modification.
  #   /  Used when a path is already absolute (shortcut optimization;
  #      such paths would be returned as absolute anyway)
  #   $  Used for build environment variables
  #   -  Used for some build environment flags (such as -lapr-1 in a
  #      "libraries" section)
  #   <  Used for our own variables (see ExpandVariables)
  #   >  Used for our own variables (see ExpandVariables)
  #   !  Used for command evaluation (see ExpandVariables)
  # Not using startswith here, because its not present before py2.5.
  if to_file == fro_file or item[0] in ('/', '$', '-', '<', '>', '!'):
    return item
  else:
    return os.path.normpath(os.path.join(
        gyp.common.RelativePath(os.path.dirname(fro_file),
                                os.path.dirname(to_file)),
                                item))


def MergeLists(to, fro, to_file, fro_file, is_paths=False, append=True):
  prepend_index = 0

  for item in fro:
    if isinstance(item, str) or isinstance(item, int):
      # The cheap and easy case.
      if is_paths:
        to_item = MakePathRelative(to_file, fro_file, item)
      else:
        to_item = item
    elif isinstance(item, dict):
      # Make a copy of the dictionary, continuing to look for paths to fix.
      # The other intelligent aspects of merge processing won't apply because
      # item is being merged into an empty dict.
      to_item = {}
      MergeDicts(to_item, item, to_file, fro_file)
    elif isinstance(item, list):
      # Recurse, making a copy of the list.  If the list contains any
      # descendant dicts, path fixing will occur.  Note that here, custom
      # values for is_paths and append are dropped; those are only to be
      # applied to |to| and |fro|, not sublists of |fro|.  append shouldn't
      # matter anyway because the new |to_item| list is empty.
      to_item = []
      MergeLists(to_item, item, to_file, fro_file)
    else:
      raise TypeError, \
          'Attempt to merge list item of unsupported type ' + \
          item.__class__.__name__

    if append:
      to.append(to_item)
    else:
      # Don't just insert everything at index 0.  That would prepend the new
      # items to the list in reverse order, which would be an unwelcome
      # surprise.
      to.insert(prepend_index, to_item)
      prepend_index = prepend_index + 1


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
      is_path = IsPathSection(k)
      if is_path:
        to[k] = MakePathRelative(to_file, fro_file, v)
      else:
        to[k] = v
    elif isinstance(v, dict):
      # Recurse, guaranteeing copies will be made of objects that require it.
      if not k in to:
        to[k] = {}
      MergeDicts(to[k], v, to_file, fro_file)
    elif isinstance(v, list):
      # Lists in dicts can be merged with different policies, depending on
      # how the key in the "from" dict (k, the from-key) is written.
      #
      # If the from-key has          ...the to-list will have this action
      # this character appended:...     applied when receiving the from-list:
      #                           =  replace
      #                           +  prepend
      #                           ?  set, only if to-list does not yet exist
      #                      (none)  append
      #
      # This logic is list-specific, but since it relies on the associated
      # dict key, it's checked in this dict-oriented function.
      ext = k[-1]
      append = True
      if ext == '=':
        list_base = k[:-1]
        lists_incompatible = [list_base, list_base + '?']
        to[list_base] = []
      elif ext == '+':
        list_base = k[:-1]
        lists_incompatible = [list_base + '=', list_base + '?']
        append = False
      elif ext == '?':
        list_base = k[:-1]
        lists_incompatible = [list_base, list_base + '=', list_base + '+']
      else:
        list_base = k
        lists_incompatible = [list_base + '=', list_base + '?']

      # Some combinations of merge policies appearing together are meaningless.
      # It's stupid to replace and append simultaneously, for example.  Append
      # and prepend are the only policies that can coexist.
      for list_incompatible in lists_incompatible:
        if list_incompatible in fro:
          raise KeyError, 'Incompatible list policies ' + k + ' and ' + \
                          list_incompatible

      if list_base in to:
        if ext == '?':
          # If the key ends in "?", the list will only be merged if it doesn't
          # already exist.
          continue
        if not isinstance(to[list_base], list):
          # This may not have been checked above if merging in a list with an
          # extension character.
          raise TypeError, \
              'Attempt to merge dict value of type ' + v.__class__.__name__ + \
              ' into incompatible type ' + to[list_base].__class__.__name__ + \
              ' for key ' + list_base + '(' + k + ')'
      else:
        to[list_base] = []

      # Call MergeLists, which will make copies of objects that require it.
      # MergeLists can recurse back into MergeDicts, although this will be
      # to make copies of dicts (with paths fixed), there will be no
      # subsequent dict "merging" once entering a list because lists are
      # always replaced, appended to, or prepended to.
      is_paths = IsPathSection(list_base)
      MergeLists(to[list_base], v, to_file, fro_file, is_paths, append)
    else:
      raise TypeError, \
          'Attempt to merge dict value of unsupported type ' + \
          v.__class__.__name__ + ' for key ' + k


def SetUpConfigurations(target, target_dict):
  # non_configuraiton_keys is a list of key names that belong in the target
  # itself and should not be propagated into its configurations.
  non_configuration_keys = [
    'actions',
    'configurations',
    'default_configuration',
    'dependencies',
    'libraries',
    'rules',
    'sources',
    'target_name',
    'type',
  ]
  # key_suffixes is a list of key suffixes that might appear on key names.
  # These suffixes are handled in conditional evaluations (for =, +, and ?)
  # and rules/exclude processing (for ! and /).  Keys with these suffixes
  # should be treated the same as keys without.
  key_suffixes = ['=', '+', '?', '!', '/']

  build_file = gyp.common.BuildFileAndTarget('', target)[0]

  # Provide a single configuration by default if none exists.
  # TODO(mark): Signal an error if default_configurations exists but
  # configurations does not.
  if not 'configurations' in target_dict:
    target_dict['configurations'] = [{'configuration_name': 'Default'}]
  if not 'default_configuration' in target_dict:
    target_dict['default_configuration'] = \
        sorted(target_dict['configurations'].keys())[0]

  index = 0
  for configuration in target_dict['configurations'].keys():
    # Configurations inherit (most) settings from the enclosing target scope.
    # Get the inheritance relationship right by making a copy of the target
    # dict.
    old_configuration_dict = target_dict['configurations'][configuration]
    new_configuration_dict = copy.deepcopy(target_dict)

    # Take out the bits that don't belong in a "configurations" section.
    # Since configuration setup is done before conditional, exclude, and rules
    # processing, be careful with handling of the suffix characters used in
    # those phases.
    delete_keys = []
    for key in new_configuration_dict:
      key_ext = key[-1:]
      if key_ext in key_suffixes:
        key_base = key[:-1]
      else:
        key_base = key
      if key_base in non_configuration_keys:
        delete_keys.append(key)

    for key in delete_keys:
      del new_configuration_dict[key]

    # Merge the supplied configuration dict into the new dict based on the
    # target dict, and put it back into the target dict as a configuration
    # dict.
    MergeDicts(new_configuration_dict, old_configuration_dict,
               build_file, build_file)
    target_dict['configurations'][configuration] = new_configuration_dict

  # Now that all of the target's configurations have been built, go through
  # the target dict's keys and remove everything that's been moved into a
  # "configurations" section.
  delete_keys = []
  for key in target_dict:
    key_ext = key[-1:]
    if key_ext in key_suffixes:
      key_base = key[:-1]
    else:
      key_base = key
    if not key_base in non_configuration_keys:
      delete_keys.append(key)
  for key in delete_keys:
    del target_dict[key]


def ProcessRulesInDict(name, the_dict):
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

  # Look through the dictionary for any lists whose keys end in "!" or "/".
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
    the_list = the_dict[list_key]

    # Initialize the list_actions list, which is parallel to the_list.  Each
    # item in list_actions identifies whether the corresponding item in
    # the_list should be excluded, unconditionally preserved (included), or
    # whether no exclusion or inclusion has been applied.  Items for which
    # no exclusion or inclusion has been applied (yet) have value -1, items
    # excluded have value 0, and items included have value 1.  An include can
    # overwrite a previous exclude, and no subsequent excludes can exclude
    # an include.  All items in list_actions are initialized to -1 because
    # no excludes or includes have been processed yet.
    list_actions = list((-1,) * len(the_list))

    exclude_key = list_key + '!'
    if exclude_key in the_dict:
      for exclude_item in the_dict[exclude_key]:
        for index in xrange(0, len(the_list)):
          if exclude_item == the_list[index] and list_actions[index] != 1:
            # This item matches the exclude_item, and it was not previously
            # preserved by an "include", so set its action to 0 (exclude).
            list_actions[index] = 0

      # The "whatever!" list is no longer needed, dump it.
      del the_dict[exclude_key]

    regex_key = list_key + '/'
    if regex_key in the_dict:
      for regex_item in the_dict[regex_key]:
        [action, pattern] = regex_item
        pattern_re = re.compile(pattern)

        for index in xrange(0, len(the_list)):
          list_item = the_list[index]
          if pattern_re.search(list_item):
            # Regular expression match.

            if action == 'exclude':
              if list_actions[index] != 1:
                # This item matches an exclude regex, and it was not previously
                # preserved by an "include", so set its value to 0 (exclude).
                list_actions[index] = 0
            elif action == 'include':
              # This item matches an include regex, so set its value to 1
              # (include) unconditionally.  Includes are intended to override
              # excludes whether they're processed before or after the exclude.
              list_actions[index] = 1
            else:
              # This is an action that doesn't make any sense.
              raise ValueError, 'Unrecognized action ' + action + ' in ' + \
                                name + ' key ' + key

    # Add excluded items to the excluded list.
    #
    # Note that exclude_key ("sources!") is different from excluded_key
    # ("sources_excluded").  The exclude_key list is input and it was already
    # processed and deleted; the excluded_key list is output and it's about
    # to be created.
    excluded_key = list_key + '_excluded'
    if excluded_key in the_dict:
      raise KeyError, \
          name + ' key ' + excluded_key + ' must not be present prior ' + \
          ' to applying exclusion/regex rules for ' + list_key

    excluded_list = []

    # Go backwards through the list_actions list so that as items are deleted,
    # the indices of items that haven't been seen yet don't shift.  That means
    # that things need to be prepended to excluded_list to maintain them in the
    # same order that they existed in the_list.
    for index in xrange(len(list_actions) - 1, -1, -1):
      if list_actions[index] == 0:
        # Dump anything with action 0 (exclude).  Keep anything with action 1
        # (include) or -1 (no include or exclude seen for the item).
        excluded_list.insert(0, the_list[index])
        del the_list[index]

    # If anything was excluded, put the excluded list into the_dict at
    # excluded_key.
    if len(excluded_list) > 0:
      the_dict[excluded_key] = excluded_list

  # Now recurse into subdicts and lists that may contain dicts.
  for key, value in the_dict.iteritems():
    if isinstance(value, dict):
      ProcessRulesInDict(key, value)
    elif isinstance(value, list):
      ProcessRulesInList(key, value)


def ProcessRulesInList(name, the_list):
  for item in the_list:
    if isinstance(item, dict):
      ProcessRulesInDict(name, item)
    elif isinstance(item, list):
      ProcessRulesInList(name, item)


def Load(build_files, variables, includes):
  # Load build files.  This loads every target-containing build file into
  # the |data| dictionary such that the keys to |data| are build file names,
  # and the values are the entire build file contents after "early" or "pre"
  # processing has been done and includes have been resolved.
  data = {}
  for build_file in build_files:
    LoadTargetBuildFile(build_file, data, variables, includes)

  # Build a dict to access each target's subdict by qualified name.
  targets = {}
  for build_file in data:
    if 'targets' in data[build_file]:
      for target in data[build_file]['targets']:
        target_name = gyp.common.QualifiedTarget(build_file,
                                                 target['target_name'])
        if target_name in targets:
          raise KeyError, 'Duplicate target definitions for ' + target_name
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

  # Handle dependent settings of various types.
  for settings_type in ['all_dependent_settings',
                        'direct_dependent_settings',
                        'link_settings']:
    DoDependentSettings(settings_type, flat_list, targets, dependency_nodes)

    # Take out the dependent settings now that they've been published to all
    # of the targets that require them.
    for target in flat_list:
      if settings_type in targets[target]:
        del targets[target][settings_type]

  # Make sure static libraries don't declare dependencies on other static
  # libraries, but that linkables depend on all unlinked static libraries
  # that they need so that their link steps will be correct.
  AdjustStaticLibraryDependencies(flat_list, targets, dependency_nodes)

  # Move everything that can go into a "configurations" section into one.
  for target in flat_list:
    target_dict = targets[target]
    SetUpConfigurations(target, target_dict)

  # Apply "post"/"late"/"target" variable expansions and condition evaluations.
  for target in flat_list:
    target_dict = targets[target]
    ProcessVariablesAndConditionsInDict(target_dict, True, variables)

  # Apply exclude (!) and regex (/) rules.
  for target in flat_list:
    target_dict = targets[target]
    ProcessRulesInDict(target, target_dict)

  # TODO(mark): Return |data| for now because the generator needs a list of
  # build files that came in.  In the future, maybe it should just accept
  # a list, and not the whole data dict.
  return [flat_list, targets, data]
