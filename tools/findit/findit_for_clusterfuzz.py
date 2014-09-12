# Copyright (c) 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import chromium_deps
from common import utils
import crash_utils
import findit_for_crash as findit
import stacktrace


def SplitStacktrace(stacktrace_string):
  """Preprocesses stacktrace string into two parts, release and debug.

  Args:
    stacktrace_string: A string representation of stacktrace,
                       in clusterfuzz format.

  Returns:
    A tuple of list of strings, release build stacktrace and
    debug build stacktrace.
  """
  # Make sure we only parse release/debug build stacktrace, and ignore
  # unsymbolised stacktrace.
  in_release_or_debug_stacktrace = False
  release_build_stacktrace_lines = None
  debug_build_stacktrace_lines = None
  current_stacktrace_lines = []

  # Iterate through all lines in stacktrace.
  for line in stacktrace_string.splitlines():
    line = line.strip()

    # If the line starts with +, it signifies the start of new stacktrace.
    if line.startswith('+-') and line.endswith('-+'):
      if 'Release Build Stacktrace' in line:
        in_release_or_debug_stacktrace = True
        current_stacktrace_lines = []
        release_build_stacktrace_lines = current_stacktrace_lines

      elif 'Debug Build Stacktrace' in line:
        in_release_or_debug_stacktrace = True
        current_stacktrace_lines = []
        debug_build_stacktrace_lines = current_stacktrace_lines

      # If the stacktrace is neither release/debug build stacktrace, ignore
      # all lines after it until we encounter release/debug build stacktrace.
      else:
        in_release_or_debug_stacktrace = False

    # This case, it must be that the line is an actual stack frame, so add to
    # the current stacktrace.
    elif in_release_or_debug_stacktrace:
      current_stacktrace_lines.append(line)

  return (release_build_stacktrace_lines, debug_build_stacktrace_lines)


def FindCulpritCLs(stacktrace_string,
                   build_type,
                   chrome_regression=None,
                   component_regression=None,
                   chrome_crash_revision=None,
                   component_crash_revision=None,
                   crashing_component_path=None,
                   crashing_component_name=None,
                   crashing_component_repo_url=None):
  """Returns the result, a list of result.Result objects and message.

  If either or both of component_regression and component_crash_revision is not
  None, is is assumed that crashing_component_path and
  crashing_component_repo_url are not None.

  Args:
    stacktrace_string: A string representing stacktrace.
    build_type: The type of the job.
    chrome_regression: A string, chrome regression from clusterfuzz, in format
                       '123456:123457'
    component_regression: A string, component regression in the same format.
    chrome_crash_revision: A crash revision of chrome, in string.
    component_crash_revision: A crash revision of the component,
                              if component build.
    crashing_component_path: A relative path of the crashing component, as in
                             DEPS file. For example, it would be 'src/v8' for
                             v8 and 'src/third_party/WebKit' for blink.
    crashing_component_name: A name of the crashing component, such as v8.
    crashing_component_repo_url: The URL of the crashing component's repo, as
                                 shown in DEPS file. For example,
                                 'https://chromium.googlesource.com/skia.git'
                                 for skia.

  Returns:
    A list of result objects, along with the short description on where the
    result is from.
  """
  build_type = build_type.lower()
  component_to_crash_revision_dict = {}
  component_to_regression_dict = {}

  # If chrome regression is available, parse DEPS file.
  chrome_regression = crash_utils.SplitRange(chrome_regression)
  if chrome_regression:
    chrome_regression_start = chrome_regression[0]
    chrome_regression_end = chrome_regression[1]

    # Do not parse regression information for crashes introduced before the
    # first archived build.
    if chrome_regression_start != '0':
      component_to_regression_dict = chromium_deps.GetChromiumComponentRange(
          chrome_regression_start, chrome_regression_end)
      if not component_to_regression_dict:
        return (('Failed to get component regression ranges for chromium '
                 'regression range %s:%s'
                 % (chrome_regression_start, chrome_regression_end)), [])

  # Parse crash revision.
  if chrome_crash_revision:
    component_to_crash_revision_dict = chromium_deps.GetChromiumComponents(
        chrome_crash_revision)
    if not component_to_crash_revision_dict:
      return (('Failed to get component dependencies for chromium revision "%s"'
                % chrome_crash_revision), [])

  # Check if component regression information is available.
  component_regression = crash_utils.SplitRange(component_regression)
  if component_regression:
    component_regression_start = component_regression[0]
    component_regression_end = component_regression[1]

    # If this component already has an entry in parsed DEPS file, overwrite
    # regression range and url.
    if crashing_component_path in component_to_regression_dict:
      component_regression_info = \
          component_to_regression_dict[crashing_component_path]
      component_regression_info['old_revision'] = component_regression_start
      component_regression_info['new_revision'] = component_regression_end
      component_regression_info['repository'] = crashing_component_repo_url

    # if this component does not have an entry, add the entry to the parsed
    # DEPS file.
    else:
      repository_type = crash_utils.GetRepositoryType(
          component_regression_start)
      component_regression_info = {
          'path': crashing_component_path,
          'rolled': True,
          'name': crashing_component_name,
          'old_revision': component_regression_start,
          'new_revision': component_regression_end,
          'repository': crashing_component_repo_url,
          'repository_type': repository_type
      }
      component_to_regression_dict[crashing_component_path] = \
          component_regression_info

  # If component crash revision is available, add it to the parsed crash
  # revisions.
  if component_crash_revision:

    # If this component has already a crash revision info, overwrite it.
    if crashing_component_path in component_to_crash_revision_dict:
      component_crash_revision_info = \
          component_to_crash_revision_dict[crashing_component_path]
      component_crash_revision_info['revision'] = component_crash_revision
      component_crash_revision_info['repository'] = crashing_component_repo_url

    # If not, add it to the parsed DEPS.
    else:
      if utils.IsGitHash(component_crash_revision):
        repository_type = 'git'
      else:
        repository_type = 'svn'
      component_crash_revision_info = {
          'path': crashing_component_path,
          'name': crashing_component_name,
          'repository': crashing_component_repo_url,
          'repository_type': repository_type,
          'revision': component_crash_revision
      }
      component_to_crash_revision_dict[crashing_component_path] = \
          component_crash_revision_info

  # Parsed DEPS is used to normalize the stacktrace. Since parsed regression
  # and parsed crash state essentially contain same information, use either.
  if component_to_regression_dict:
    parsed_deps = component_to_regression_dict
  elif component_to_crash_revision_dict:
    parsed_deps = component_to_crash_revision_dict
  else:
    return (('Identifying culprit CL requires at lease one of regression '
             'information or crash revision'), [])

  # Split stacktrace into release build/debug build and parse them.
  (release_build_stacktrace, debug_build_stacktrace) = SplitStacktrace(
      stacktrace_string)
  if not (release_build_stacktrace or debug_build_stacktrace):
    parsed_release_build_stacktrace = stacktrace.Stacktrace(
        stacktrace_string.splitlines(), build_type, parsed_deps)
  else:
    parsed_release_build_stacktrace = stacktrace.Stacktrace(
        release_build_stacktrace, build_type, parsed_deps)

  parsed_debug_build_stacktrace = stacktrace.Stacktrace(
      debug_build_stacktrace, build_type, parsed_deps)

  # Get a highest priority callstack (main_stack) from stacktrace, with release
  # build stacktrace in higher priority than debug build stacktace. This stack
  # is the callstack to find blame information for.
  if parsed_release_build_stacktrace.stack_list:
    main_stack = parsed_release_build_stacktrace.GetCrashStack()
  elif parsed_debug_build_stacktrace.stack_list:
    main_stack = parsed_debug_build_stacktrace.GetCrashStack()
  else:
    if 'mac_' in build_type:
      return ('No line information available in stacktrace.', [])

    return ('Findit failed to find any stack trace. Is it in a new format?', [])

  # Run the algorithm on the parsed stacktrace, and return the result.
  stacktrace_list = [parsed_release_build_stacktrace,
                     parsed_debug_build_stacktrace]
  return findit.FindItForCrash(
      stacktrace_list, main_stack, component_to_regression_dict,
      component_to_crash_revision_dict)
