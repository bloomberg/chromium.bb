# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
from threading import Lock, Thread

import blame
from common import utils
import component_dictionary
import crash_utils
import git_repository_parser
import match_set
import svn_repository_parser


LINE_CHANGE_PRIORITY = 1
FILE_CHANGE_PRIORITY = 2
_THIS_DIR = os.path.abspath(os.path.dirname(__file__))
CONFIG = crash_utils.ParseURLsFromConfig(os.path.join(_THIS_DIR,
                                                      'config.ini'))


def GenerateMatchEntry(
    matches, revision_info, revision_number, file_path, function,
    component_path, component_name, crashed_line_numbers, stack_frame_indices,
    file_change_type, repository_parser):
  """Generates a match object and adds it to the match set.

  Args:
    matches: A matchset object, a map from CL to a match object.
    revision_info: The revision information, a map from fields (message,
                   changed files, etc) to its values.
    revision_number: The SVN revision number or git hash.
    file_path: The path of the file.
    function: The function that caused an crash.
    component_path: The path of the component this file is from.
    component_name: The name of the component the file is from.
    crashed_line_numbers: The list of the lines in the file that caused
                          the crash.
    stack_frame_indices: The list of positions of this file within a stack.
    file_change_type: Whether file is modified, added or deleted.
    repository_parser: The parser object to parse line diff.
  """
  # Check if this CL should be ignored.
  with matches.matches_lock:
    if revision_number in matches.cls_to_ignore:
      return

    # If this CL is not already identified as suspected, create a new entry.
    if revision_number not in matches.matches:
      match = match_set.Match(revision_info, component_name)
      message = revision_info['message']
      # TODO(jeun): Don't hold lock while issuing http request.
      match.ParseMessage(message, matches.codereview_api_url)

      # If this match is a revert, add to the set of CLs to be ignored.
      if match.is_revert:
        matches.cls_to_ignore.add(revision_number)

        # If this match has info on what CL it is reverted from, add that CL.
        if match.revert_of:
          matches.cls_to_ignore.add(match.revert_of)

        return

      matches.matches[revision_number] = match

    else:
      match = matches.matches[revision_number]

  (diff_url, changed_line_numbers, changed_line_contents) = (
      repository_parser.ParseLineDiff(
          file_path, component_path, file_change_type, revision_number))

  # Ignore this match if the component is not supported for svn.
  if not diff_url:
    return

  # Find the intersection between the lines that this file crashed on and
  # the changed lines.
  (line_number_intersection, stack_frame_index_intersection) = (
      crash_utils.Intersection(
          crashed_line_numbers, stack_frame_indices, changed_line_numbers))

  # Find the minimum distance between the changed lines and crashed lines.
  min_distance = crash_utils.FindMinLineDistance(crashed_line_numbers,
                                                 changed_line_numbers)

  # Check whether this CL changes the crashed lines or not.
  if line_number_intersection:
    priority = LINE_CHANGE_PRIORITY
  else:
    priority = FILE_CHANGE_PRIORITY

  # Add the parsed information to the object.
  with matches.matches_lock:
    match.crashed_line_numbers.append(line_number_intersection)

    file_name = file_path.split('/')[-1]
    match.changed_files.append(file_name)

    # Update the min distance only if it is less than the current one.
    if min_distance < match.min_distance:
      match.min_distance = min_distance

    # If this CL does not change the crashed line, all occurrence of this
    # file in the stack has the same priority.
    if not stack_frame_index_intersection:
      stack_frame_index_intersection = stack_frame_indices
    match.stack_frame_indices.append(stack_frame_index_intersection)
    match.changed_file_urls.append(diff_url)
    match.priorities.append(priority)
    match.function_list.append(function)


def FindMatch(revisions_info_map, file_to_revision_info, file_to_crash_info,
              component_path, component_name, repository_parser,
              codereview_api_url):
  """Finds a CL that modifies file in the stacktrace.

  Args:
    revisions_info_map: A dictionary mapping revision number to the CL
                        information.
    file_to_revision_info: A dictionary mapping file to the revision that
                           modifies it.
    file_to_crash_info: A dictionary mapping file to its occurrence in
                       stacktrace.
    component_path: The path of the component to search for.
    component_name: The name of the component to search for.
    repository_parser: The parser object to parse the line diff.
    codereview_api_url: A code review url to retrieve data from.

  Returns:
    Matches, a set of match objects.
  """
  matches = match_set.MatchSet(codereview_api_url)
  threads = []

  # Iterate through the crashed files in the stacktrace.
  for crashed_file_path in file_to_crash_info:
    # Ignore header file.
    if crashed_file_path.endswith('.h'):
      continue

    # If the file in the stacktrace is not changed in any commits, continue.
    for changed_file_path in file_to_revision_info:
      changed_file_name = changed_file_path.split('/')[-1]
      crashed_file_name = crashed_file_path.split('/')[-1]

      if changed_file_name != crashed_file_name:
        continue

      if not crash_utils.GuessIfSameSubPath(
          changed_file_path, crashed_file_path):
        continue

      crashed_line_numbers = file_to_crash_info.GetCrashedLineNumbers(
          crashed_file_path)
      stack_frame_nums = file_to_crash_info.GetCrashStackFrameIndices(
          crashed_file_path)
      functions = file_to_crash_info.GetCrashFunctions(crashed_file_path)

      # Iterate through the CLs that this file path is changed.
      for (cl, file_change_type) in file_to_revision_info[changed_file_path]:
        # If the file change is delete, ignore this CL.
        if file_change_type == 'D':
          continue

        revision = revisions_info_map[cl]

        match_thread = Thread(
            target=GenerateMatchEntry,
            args=[matches, revision, cl, changed_file_path, functions,
                  component_path, component_name, crashed_line_numbers,
                  stack_frame_nums, file_change_type,
                  repository_parser])
        threads.append(match_thread)
        match_thread.start()

  for match_thread in threads:
    match_thread.join()

  matches.RemoveRevertedCLs()

  return matches


def FindMatchForComponent(component_path, file_to_crash_info, changelog,
                          callstack_priority, results, results_lock):
  """Parses changelog and finds suspected CLs for a given component.

  Args:
    component_path: The path of component to look for the culprit CL.
    file_to_crash_info: A dictionary mapping file to its occurrence in
                        stackframe.
    changelog: The parsed changelog for this component.
    callstack_priority: The priority of this call stack, 0 if from crash stack,
                        1 if from freed, 2 if from previously allocated.
    results: A dictionary to store the result.
    results_lock: A lock that guards results.
  """
  (repository_parser, component_name, revisions, file_to_revision_map) = \
      changelog

  # Find match for this component.
  codereview_api_url = CONFIG['codereview']['review_url']
  component_result = FindMatch(
      revisions, file_to_revision_map, file_to_crash_info, component_path,
      component_name, repository_parser, codereview_api_url)
  matches = component_result.matches

  # For all the match results in a dictionary, add to the list so that it
  # can be sorted.
  with results_lock:
    for cl in matches:
      match = matches[cl]
      results.append((callstack_priority, cl, match))


def FindMatchForCallstack(
    callstack, components, component_to_changelog_map, results,
    results_lock):
  """Finds culprit cl for a stack within a stacktrace.

  For each components to look for, create new thread that computes the matches
  and join the results at the end.

  Args:
    callstack: A callstack in a stacktrace to find the result for.
    components: A set of components to look for.
    component_to_changelog_map: A map from component to its parsed changelog.
    results: A list to aggregrate results from all stacktraces.
    results_lock: A lock that guards results.
  """
  # Create component dictionary from the component and call stack.
  component_dict = component_dictionary.ComponentDictionary(callstack,
                                                            components)
  callstack_priority = callstack.priority

  # Iterate through all components and create new thread for each component.
  threads = []
  for component_path in component_dict:
    # If the component to consider in this callstack is not in the parsed list
    # of components, ignore this one.
    if component_path not in component_to_changelog_map:
      continue

    changelog = component_to_changelog_map[component_path]
    file_to_crash_info = component_dict.GetFileDict(component_path)
    t = Thread(
        target=FindMatchForComponent,
        args=[component_path, file_to_crash_info, changelog,
              callstack_priority, results, results_lock])
    threads.append(t)
    t.start()

  for t in threads:
    t.join()


def FindMatchForStacktrace(stacktrace, components,
                           component_to_regression_dict):
  """Finds the culprit CL for stacktrace.

  The passed stacktrace is either from release build stacktrace
  or debug build stacktrace.

  Args:
    stacktrace: A list of parsed stacks within a stacktrace.
    components: A set of components to look for.
    component_to_regression_dict: A dictionary mapping component path to
                                  its regression.

  Returns:
    A list of match results from all stacks.
  """
  # A list to aggregate results from all the callstacks in the stacktrace.
  results = []
  results_lock = Lock()

  # Setup parsers.
  svn_parser = svn_repository_parser.SVNParser(CONFIG['svn'])
  git_parser = git_repository_parser.GitParser(component_to_regression_dict,
                                               CONFIG['git'])

  # Create a cache of parsed revisions.
  component_to_changelog_map = {}
  for component_path in components:
    component_object = component_to_regression_dict[component_path]
    range_start = component_object['old_revision']
    range_end = component_object['new_revision']

    # If range start is 0, the range is too large and the crash has been
    # introduced the archived build, so ignore this case.
    if range_start == '0':
      continue

    component_name = component_to_regression_dict[component_path]['name']

    is_git = utils.IsGitHash(range_start)
    if is_git:
      repository_parser = git_parser
    else:
      repository_parser = svn_parser

    (revisions, file_to_revision_map) = repository_parser.ParseChangelog(
        component_path, range_start, range_end)

    # If the returned map from ParseChangeLog is empty, we don't need to look
    # further because either the parsing failed or the changelog is empty.
    if not (revisions and file_to_revision_map):
      continue

    component_to_changelog_map[component_path] = (repository_parser,
                                                  component_name,
                                                  revisions,
                                                  file_to_revision_map)

  # Create separate threads for each of the call stack in the stacktrace.
  threads = []
  for callstack in stacktrace.stack_list:
    t = Thread(
        target=FindMatchForCallstack,
        args=[callstack, components, component_to_changelog_map,
              results, results_lock])
    threads.append(t)
    t.start()

  for t in threads:
    t.join()

  return results


def SortMatchesFunction(match_with_stack_priority):
  """A function to sort the match triple.

  Currently, it sorts the list by:
  1) The highest priority file change in the CL (changing crashed line is
  higher priority than just changing the file).
  2) The callstack this match is computed (crash stack, freed, allocation).
  3) The minimum stack frame number of the changed file in the match.
  4) The number of files this CL changes (higher the better).
  5) The minimum distance between the lines that the CL changes and crashed
  lines.

  Args:
    match_with_stack_priority: A match object, with the CL it is from and what
                               callstack it is from.

  Returns:
    A sort key.
  """
  (stack_priority, _, match) = match_with_stack_priority

  return (min(match.priorities),
          stack_priority,
          crash_utils.FindMinStackFrameNumber(match.stack_frame_indices,
                                              match.priorities),
          -len(match.changed_files), match.min_distance)


def SortAndFilterMatches(matches, num_important_frames=5):
  """Filters the list of potential culprit CLs to remove noise.

  Args:
    matches: A list containing match results.
    num_important_frames: A number of frames on the top of the frame to Check
                          for when filtering the results. A match with a file
                          that is in top num_important_frames of the stacktrace
                          is regarded more probable then others.

  Returns:
    Filtered match results.
  """
  new_matches = []
  line_changed = False
  is_important_frame = False
  highest_priority_stack = crash_utils.INFINITY
  matches.sort(key=SortMatchesFunction)

  # Iterate through the matches to find out what results are significant.
  for stack_priority, cl, match in matches:
    # Check if the current match changes crashed line.
    is_line_change = (min(match.priorities) == LINE_CHANGE_PRIORITY)

    # Check which stack this match is from, and finds the highest priority
    # callstack up to this point.
    current_stack = stack_priority
    if current_stack < highest_priority_stack:
      highest_priority_stack = current_stack

    # Check if current match changes a file that occurs in crash state.
    flattened_stack_frame_indices = [frame for frame_indices in
                                     match.stack_frame_indices
                                     for frame in frame_indices]
    current_is_important = (
        min(flattened_stack_frame_indices) < num_important_frames)

    # This match and anything lower than this should be ignored if:
    # - Current match does not change crashed lines but there are matches
    # that do so.
    # - Current match is not in crash state but there are matches in it.
    # - There are other matches that came from higher priority stack.
    if (line_changed and not is_line_change) or (
        is_important_frame and not current_is_important) or (
            current_stack > highest_priority_stack):
      break

    # Update the variables.
    if is_line_change:
      line_changed = True
    if current_is_important:
      is_important_frame = True

    # Add current match to the filtered result.
    new_matches.append((stack_priority, cl, match))

  return new_matches


def GenerateReasonForMatches(matches):
  """Generates a reason that a match (CL) is a culprit cl.

  Args:
    matches: A list of match objects.
  """
  # Iterate through the matches in the list.
  for i, _, match in matches:
    reason = []

    # Zip the files in the match by the reason they are suspected
    # (how the file is modified).
    match_by_priority = zip(
        match.priorities, match.crashed_line_numbers, match.changed_files,
        match.changed_file_urls)

    # Sort the zipped changed files in the match by their priority so that the
    # changed lines comes first in the reason.
    match_by_priority.sort(
        key=lambda (priority, crashed_line_number, file_name, url): priority)

    # Iterate through the sorted match.
    for i in range(len(match_by_priority)):
      (priority, crashed_line_number, file_name, file_url) = \
          match_by_priority[i]

      # If the file in the match is a line change, append a explanation.
      if priority == LINE_CHANGE_PRIORITY:
        reason.append(
            'Line %s of file %s      which potentially caused the crash '
            'according to the stacktrace, is changed in this cl.\n' %
            (crash_utils.PrettifyList(crashed_line_number),
             crash_utils.PrettifyFiles([(file_name, file_url)])))

      else:
        # Get all the files that are not line change.
        rest_of_the_files = match_by_priority[i:]

        if len(rest_of_the_files) == 1:
          file_string = 'File %s      is changed in this cl.\n'
        else:
          file_string = 'Files %s      are changed in this cl.\n'

        # Create a list of file name and its url, and prettify the list.
        file_name_with_url = [(file_name, file_url)
                              for (_, _, file_name, file_url)
                              in rest_of_the_files]
        pretty_file_name_url = crash_utils.PrettifyFiles(file_name_with_url)

        # Add the reason, break because we took care of the rest of the files.
        reason.append(file_string % pretty_file_name_url)
        break

    # Set the reason as string.
    match.reason = ''.join(reason)


def CombineMatches(matches):
  """Combine possible duplicates in matches.

  Args:
    matches: A list of matches object, along with its callstack priority and
             CL it is from.
  Returns:
    A combined list of matches.
  """
  combined_matches = []

  for stack_index, cl, match in matches:
    found_match = None

    # Iterate through the list of combined matches.
    for _, cl_combined, match_combined in combined_matches:
      # Check for if current CL is already higher up in the result.
      if cl == cl_combined:
        found_match = match_combined
        break

    # If current match is not already in, add it to the list of matches.
    if not found_match:
      combined_matches.append((stack_index, cl, match))
      continue

    # Combine the reason if the current match is already in there.
    found_match.reason += match.reason

  return combined_matches


def FilterAndGenerateReasonForMatches(result):
  """A wrapper function.

  It generates reasons for the matches and returns string representation
  of filtered results.

  Args:
    result: A list of match objects.

  Returns:
    A string representation of filtered results.
  """
  new_result = SortAndFilterMatches(result)
  GenerateReasonForMatches(new_result)
  combined_matches = CombineMatches(new_result)
  return crash_utils.MatchListToResultList(combined_matches)


def ParseCrashComponents(main_stack):
  """Parses the crashing component.

  Crashing components is a component that top_n_frames of the stacktrace is
  from.

  Args:
    main_stack: Main stack from the stacktrace.
    top_n_frames: A number of frames to regard as crashing frame.

  Returns:
    A set of components.
  """
  components = set()

  for frame in main_stack.frame_list:
    components.add(frame.component_path)

  return components


def GenerateAndFilterBlameList(callstack, component_to_crash_revision_dict,
                               component_to_regression_dict):
  """A wrapper function.

  Finds blame information for stack and returns string representation.

  Args:
    callstack: A callstack to find the blame information.
    component_to_crash_revision_dict: A dictionary mapping component to its
                                      crash revision.
    component_to_regression_dict: A dictionary mapping component to its
                                  regression.

  Returns:
    A list of blame results.
  """
  # Setup parser objects to use for parsing blame information.
  svn_parser = svn_repository_parser.SVNParser(CONFIG['svn'])
  git_parser = git_repository_parser.GitParser(component_to_regression_dict,
                                               CONFIG['git'])
  parsers = {}
  parsers['svn'] = svn_parser
  parsers['git'] = git_parser

  # Create and generate the blame objects from the callstack.
  blame_list = blame.BlameList()
  blame_list.FindBlame(callstack, component_to_crash_revision_dict,
                       component_to_regression_dict,
                       parsers)

  blame_list.FilterAndSortBlameList()
  return crash_utils.BlameListToResultList(blame_list)


def FindItForCrash(stacktrace_list,
                   callstack,
                   component_to_regression_dict,
                   component_to_crash_revision_dict):
  """Finds the culprit CL from the list of stacktrace.

  Args:
    stacktrace_list: A list of stacktraces to look for, in the order of
                     decreasing significance.
    callstack: A callstack object to show blame information for, if there are
               no results for all stacktraces in the stacktrace_list.
    component_to_regression_dict: A parsed regression information as a
                                  result of parsing DEPS file.
    component_to_crash_revision_dict: A parsed crash revision information.

  Returns:
    A list of result objects, with the message how the result is created.
  """
  # If regression information is not available, return blame information.
  if not component_to_regression_dict:
    return_message = (
        'Regression information is not available. The result is '
        'the blame information.')
    result = GenerateAndFilterBlameList(callstack,
                                        component_to_crash_revision_dict,
                                        component_to_regression_dict)
    return (return_message, result)

  for stacktrace in stacktrace_list:
    # Check the next stacktrace if current one is empty.
    if not stacktrace.stack_list:
      continue

    # Get the crash stack for this stacktrace, and extract crashing components
    # from it.
    main_stack = stacktrace.GetCrashStack()
    components = ParseCrashComponents(main_stack)

    result_for_stacktrace = FindMatchForStacktrace(
        stacktrace, components, component_to_regression_dict)

    # If the result is empty, check the next stacktrace. Else, return the
    # filtered result.
    if not result_for_stacktrace:
      continue

    return_message = (
        'The result is a list of CLs that change the crashed files.')
    result = FilterAndGenerateReasonForMatches(result_for_stacktrace)
    return (return_message, result)

  # If no match is found, return the blame information for the input
  # callstack.
  return_message = (
      'There are no CLs that change the crashed files. The result is the '
      'blame information.')
  result = GenerateAndFilterBlameList(
      callstack, component_to_crash_revision_dict,
      component_to_regression_dict)
  return (return_message, result)

