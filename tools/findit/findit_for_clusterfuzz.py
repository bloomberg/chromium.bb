# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging

import chromium_deps
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
    if line.startswith('+'):
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
                   crashing_component=None):
  """Returns the result, a list of result.Result objects and message.

  Args:
    stacktrace_string: A string representing stacktrace.
    build_type: The type of the job.
    chrome_regression: A string, chrome regression from clusterfuzz, in format
                       '123456:123457'
    component_regression: A string, component regression in the same format.
    chrome_crash_revision: A crash revision of chrome, in string.
    component_crash_revision: A crash revision of the component,
                              if component build.
    crashing_component: Yet to be decided.

  Returns:
    A list of result objects, along with the short description on where the
    result is from.
  """
  build_type = build_type.lower()
  if 'syzyasan' in build_type:
    return ('This build type is currently not supported.', [])

  logging.basicConfig(filename='errors.log', level=logging.WARNING,
                      filemode='w')

  component_to_crash_revision_dict = {}
  component_to_regression_dict = {}

  # TODO(jeun): Come up with a good way to connect crashing component name to
  # its path.
  if component_regression or component_crash_revision:
    return ('Component builds are not supported yet.', [])

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

  # Parse crash revision.
  if chrome_crash_revision:
    component_to_crash_revision_dict = chromium_deps.GetChromiumComponents(
        chrome_crash_revision)

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
    return ('Stacktrace is malformed.', [])

  # Run the algorithm on the parsed stacktrace, and return the result.
  stacktrace_list = [parsed_release_build_stacktrace,
                     parsed_debug_build_stacktrace]
  return findit.FindItForCrash(
      stacktrace_list, main_stack, component_to_regression_dict,
      component_to_crash_revision_dict)
