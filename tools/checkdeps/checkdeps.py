#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Makes sure that files include headers from allowed directories.

Checks DEPS files in the source tree for rules, and applies those rules to
"#include" commands in source files. Any source file including something not
permitted by the DEPS files will fail.

The format of the deps file:

First you have the normal module-level deps. These are the ones used by
gclient. An example would be:

  deps = {
    "base":"http://foo.bar/trunk/base"
  }

DEPS files not in the top-level of a module won't need this. Then you
have any additional include rules. You can add (using "+") or subtract
(using "-") from the previously specified rules (including
module-level deps). You can also specify a path that is allowed for
now but that we intend to remove, using "!"; this is treated the same
as "+" when check_deps is run by our bots, but a presubmit step will
show a warning if you add a new include of a file that is only allowed
by "!".

Note that for .java files, there is currently no difference between
"+" and "!", even in the presubmit step.

  include_rules = {
    # Code should be able to use base (it's specified in the module-level
    # deps above), but nothing in "base/evil" because it's evil.
    "-base/evil",

    # But this one subdirectory of evil is OK.
    "+base/evil/not",

    # And it can include files from this other directory even though there is
    # no deps rule for it.
    "+tools/crime_fighter",

    # This dependency is allowed for now but work is ongoing to remove it,
    # so you shouldn't add further dependencies on it.
    "!base/evil/ok_for_now.h",
  }

DEPS files may be placed anywhere in the tree. Each one applies to all
subdirectories, where there may be more DEPS files that provide additions or
subtractions for their own sub-trees.

There is an implicit rule for the current directory (where the DEPS file lives)
and all of its subdirectories. This prevents you from having to explicitly
allow the current directory everywhere.  This implicit rule is applied first,
so you can modify or remove it using the normal include rules.

The rules are processed in order. This means you can explicitly allow a higher
directory and then take away permissions from sub-parts, or the reverse.

Note that all directory separators must be slashes (Unix-style) and not
backslashes. All directories should be relative to the source root and use
only lowercase.
"""

import os
import optparse
import subprocess
import sys
import copy

import cpp_checker
import java_checker
from rules import Rule, Rules


# Variable name used in the DEPS file to add or subtract include files from
# the module-level deps.
INCLUDE_RULES_VAR_NAME = "include_rules"

# Optionally present in the DEPS file to list subdirectories which should not
# be checked. This allows us to skip third party code, for example.
SKIP_SUBDIRS_VAR_NAME = "skip_child_includes"


def NormalizePath(path):
  """Returns a path normalized to how we write DEPS rules and compare paths.
  """
  return path.lower().replace('\\', '/')


class DepsChecker(object):
  """Parses include_rules from DEPS files and can verify files in the
  source tree against them.
  """

  def __init__(self, base_directory=None, verbose=False, being_tested=False):
    """Creates a new DepsChecker.

    Args:
      base_directory: OS-compatible path to root of checkout, e.g. C:\chr\src.
      verbose: Set to true for debug output.
      being_tested: Set to true to ignore the DEPS file at tools/checkdeps/DEPS.
    """
    self.base_directory = base_directory
    if not base_directory:
      self.base_directory = os.path.abspath(
        os.path.join(os.path.abspath(os.path.dirname(__file__)), '..', '..'))

    self.verbose = verbose

    self.git_source_directories = set()
    self._AddGitSourceDirectories()

    self._under_test = being_tested

  def _ApplyRules(self, existing_rules, includes, cur_dir):
    """Applies the given include rules, returning the new rules.

    Args:
      existing_rules: A set of existing rules that will be combined.
      include: The list of rules from the "include_rules" section of DEPS.
      cur_dir: The current directory, normalized path. We will create an
               implicit rule that allows inclusion from this directory.

    Returns: A new set of rules combining the existing_rules with the other
             arguments.
    """
    rules = copy.copy(existing_rules)

    # First apply the implicit "allow" rule for the current directory.
    if cur_dir.startswith(
          NormalizePath(os.path.normpath(self.base_directory))):
      relative_dir = cur_dir[len(self.base_directory) + 1:]

      source = relative_dir
      if len(source) == 0:
        source = "top level"  # Make the help string a little more meaningful.
      rules.AddRule("+" + relative_dir, "Default rule for " + source)
    else:
      raise Exception("Internal error: base directory is not at the beginning" +
                      " for\n  %s and base dir\n  %s" %
                      (cur_dir, self.base_directory))

    # Last, apply the additional explicit rules.
    for (_, rule_str) in enumerate(includes):
      if not relative_dir:
        rule_description = "the top level include_rules"
      else:
        rule_description = relative_dir + "'s include_rules"
      rules.AddRule(rule_str, rule_description)

    return rules

  def _ApplyDirectoryRules(self, existing_rules, dir_name):
    """Combines rules from the existing rules and the new directory.

    Any directory can contain a DEPS file. Toplevel DEPS files can contain
    module dependencies which are used by gclient. We use these, along with
    additional include rules and implicit rules for the given directory, to
    come up with a combined set of rules to apply for the directory.

    Args:
      existing_rules: The rules for the parent directory. We'll add-on to these.
      dir_name: The directory name that the deps file may live in (if
                it exists).  This will also be used to generate the
                implicit rules.  This is a non-normalized path.

    Returns: A tuple containing: (1) the combined set of rules to apply to the
             sub-tree, and (2) a list of all subdirectories that should NOT be
             checked, as specified in the DEPS file (if any).
    """
    norm_dir_name = NormalizePath(dir_name)

    # Check for a .svn directory in this directory or check this directory is
    # contained in git source direcotries. This will tell us if it's a source
    # directory and should be checked.
    if not (os.path.exists(os.path.join(dir_name, ".svn")) or
            (norm_dir_name in self.git_source_directories)):
      return (None, [])

    # Check the DEPS file in this directory.
    if self.verbose:
      print "Applying rules from", dir_name
    def FromImpl(_unused, _unused2):
      pass  # NOP function so "From" doesn't fail.

    def FileImpl(_unused):
      pass  # NOP function so "File" doesn't fail.

    class _VarImpl:
      def __init__(self, local_scope):
        self._local_scope = local_scope

      def Lookup(self, var_name):
        """Implements the Var syntax."""
        if var_name in self._local_scope.get("vars", {}):
          return self._local_scope["vars"][var_name]
        raise Exception("Var is not defined: %s" % var_name)

    local_scope = {}
    global_scope = {
        "File": FileImpl,
        "From": FromImpl,
        "Var": _VarImpl(local_scope).Lookup,
        }
    deps_file = os.path.join(dir_name, "DEPS")

    # The second conditional here is to disregard the
    # tools/checkdeps/DEPS file while running tests.  This DEPS file
    # has a skip_child_includes for 'testdata' which is necessary for
    # running production tests, since there are intentional DEPS
    # violations under the testdata directory.  On the other hand when
    # running tests, we absolutely need to verify the contents of that
    # directory to trigger those intended violations and see that they
    # are handled correctly.
    if os.path.isfile(deps_file) and (
        not self._under_test or not os.path.split(dir_name)[1] == 'checkdeps'):
      execfile(deps_file, global_scope, local_scope)
    elif self.verbose:
      print "  No deps file found in", dir_name

    # Even if a DEPS file does not exist we still invoke ApplyRules
    # to apply the implicit "allow" rule for the current directory
    include_rules = local_scope.get(INCLUDE_RULES_VAR_NAME, [])
    skip_subdirs = local_scope.get(SKIP_SUBDIRS_VAR_NAME, [])

    return (self._ApplyRules(existing_rules, include_rules, norm_dir_name),
            skip_subdirs)

  def CheckDirectory(self, start_dir):
    """Checks all relevant source files in the specified directory and
    its subdirectories for compliance with DEPS rules throughout the
    tree (starting at |self.base_directory|).  |start_dir| must be a
    subdirectory of |self.base_directory|.

    Returns an empty array on success.  On failure, the array contains
    strings that can be printed as human-readable error messages.
    """
    # TODO(joi): Make this work for start_dir != base_dir (I have a
    # subsequent change in flight to do this).
    base_rules = Rules()
    java = java_checker.JavaChecker(self.base_directory, self.verbose)
    cpp = cpp_checker.CppChecker(self.verbose)
    checkers = dict(
        (extension, checker)
        for checker in [java, cpp] for extension in checker.EXTENSIONS)
    return self._CheckDirectoryImpl(base_rules, checkers, start_dir)

  def _CheckDirectoryImpl(self, parent_rules, checkers, dir_name):
    (rules, skip_subdirs) = self._ApplyDirectoryRules(parent_rules, dir_name)
    if rules == None:
      return []

    # Collect a list of all files and directories to check.
    files_to_check = []
    dirs_to_check = []
    results = []
    contents = os.listdir(dir_name)
    for cur in contents:
      if cur in skip_subdirs:
        continue  # Don't check children that DEPS has asked us to skip.
      full_name = os.path.join(dir_name, cur)
      if os.path.isdir(full_name):
        dirs_to_check.append(full_name)
      elif os.path.splitext(full_name)[1] in checkers:
        files_to_check.append(full_name)

    # First check all files in this directory.
    for cur in files_to_check:
      checker = checkers[os.path.splitext(cur)[1]]
      file_status = checker.CheckFile(rules, cur)
      if file_status:
        results.append("ERROR in " + cur + "\n" + file_status)

    # Next recurse into the subdirectories.
    for cur in dirs_to_check:
      results.extend(self._CheckDirectoryImpl(rules, checkers, cur))
    return results

  def CheckAddedCppIncludes(self, added_includes):
    """This is used from PRESUBMIT.py to check new #include statements added in
    the change being presubmit checked.

    Args:
      added_includes: ((file_path, (include_line, include_line, ...), ...)

    Return:
      A list of tuples, (bad_file_path, rule_type, rule_description)
      where rule_type is one of Rule.DISALLOW or Rule.TEMP_ALLOW and
      rule_description is human-readable. Empty if no problems.
    """
    # Map of normalized directory paths to rules to use for those
    # directories, or None for directories that should be skipped.
    directory_rules = {}

    def ApplyDirectoryRulesAndSkipSubdirs(parent_rules, dir_path):
      rules_tuple = self._ApplyDirectoryRules(parent_rules, dir_path)
      directory_rules[NormalizePath(dir_path)] = rules_tuple[0]
      for subdir in rules_tuple[1]:
        # We skip this one case for running tests.
        directory_rules[NormalizePath(
            os.path.normpath(os.path.join(dir_path, subdir)))] = None

    ApplyDirectoryRulesAndSkipSubdirs(Rules(), self.base_directory)

    def GetDirectoryRules(dir_path):
      """Returns a Rules object to use for the given directory, or None
      if the given directory should be skipped.
      """
      norm_dir_path = NormalizePath(dir_path)

      if not dir_path.startswith(
          NormalizePath(os.path.normpath(self.base_directory))):
        dir_path = os.path.join(self.base_directory, dir_path)
        norm_dir_path = NormalizePath(dir_path)

      parent_dir = os.path.dirname(dir_path)
      parent_rules = None
      if not norm_dir_path in directory_rules:
        parent_rules = GetDirectoryRules(parent_dir)

      # We need to check for an entry for our dir_path again, in case we
      # are at a path e.g. A/B/C where A/B/DEPS specifies the C
      # subdirectory to be skipped; in this case, the invocation to
      # GetDirectoryRules(parent_dir) has already filled in an entry for
      # A/B/C.
      if not norm_dir_path in directory_rules:
        if not parent_rules:
          # If the parent directory should be skipped, then the current
          # directory should also be skipped.
          directory_rules[norm_dir_path] = None
        else:
          ApplyDirectoryRulesAndSkipSubdirs(parent_rules, dir_path)
      return directory_rules[norm_dir_path]

    cpp = cpp_checker.CppChecker(self.verbose)

    problems = []
    for file_path, include_lines in added_includes:
      # TODO(joi): Make this cover Java as well.
      if not os.path.splitext(file_path)[1] in cpp.EXTENSIONS:
        pass
      rules_for_file = GetDirectoryRules(os.path.dirname(file_path))
      if rules_for_file:
        for line in include_lines:
          is_include, line_status, rule_type = cpp.CheckLine(
              rules_for_file, line, True)
          if rule_type != Rule.ALLOW:
            problems.append((file_path, rule_type, line_status))
    return problems

  def _AddGitSourceDirectories(self):
    """Adds any directories containing sources managed by git to
    self.git_source_directories.
    """
    if not os.path.exists(os.path.join(self.base_directory, ".git")):
      return

    popen_out = os.popen("cd %s && git ls-files --full-name ." %
                         subprocess.list2cmdline([self.base_directory]))
    for line in popen_out.readlines():
      dir_name = os.path.join(self.base_directory, os.path.dirname(line))
      # Add the directory as well as all the parent directories. Use
      # forward slashes and lower case to normalize paths.
      while dir_name != self.base_directory:
        self.git_source_directories.add(NormalizePath(dir_name))
        dir_name = os.path.dirname(dir_name)
    self.git_source_directories.add(NormalizePath(self.base_directory))


def PrintUsage():
  print """Usage: python checkdeps.py [--root <root>] [tocheck]

  --root   Specifies the repository root. This defaults to "../../.." relative
           to the script file. This will be correct given the normal location
           of the script in "<root>/tools/checkdeps".

  tocheck  Specifies the directory, relative to root, to check. This defaults
           to "." so it checks everything. Only one level deep is currently
           supported, so you can say "chrome" but not "chrome/browser".

Examples:
  python checkdeps.py
  python checkdeps.py --root c:\\source chrome"""


def main():
  option_parser = optparse.OptionParser()
  option_parser.add_option("", "--root", default="", dest="base_directory",
                           help='Specifies the repository root. This defaults '
                           'to "../../.." relative to the script file, which '
                           'will normally be the repository root.')
  option_parser.add_option("-v", "--verbose", action="store_true",
                           default=False, help="Print debug logging")
  options, args = option_parser.parse_args()

  deps_checker = DepsChecker(options.base_directory, options.verbose)

  # Figure out which directory we have to check.
  start_dir = deps_checker.base_directory
  if len(args) == 1:
    # Directory specified. Start here. It's supposed to be relative to the
    # base directory.
    start_dir = os.path.abspath(
        os.path.join(deps_checker.base_directory, args[0]))
  elif len(args) >= 2:
    # More than one argument, we don't handle this.
    PrintUsage()
    return 1

  print "Using base directory:", deps_checker.base_directory
  print "Checking:", start_dir

  results = deps_checker.CheckDirectory(start_dir)
  if results:
    for result in results:
      print result
    print "\nFAILED\n"
    return 1
  print "\nSUCCESS\n"
  return 0


if '__main__' == __name__:
  sys.exit(main())
