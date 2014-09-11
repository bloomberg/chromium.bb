# Copyright (c) 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import atexit
import cgi
import ConfigParser
import json
import os
import Queue
import threading
import time

from common import utils
from result import Result


INFINITY = float('inf')

MAX_THREAD_NUMBER = 10
TASK_QUEUE = None


def SignalWorkerThreads():
  global TASK_QUEUE
  if not TASK_QUEUE:
    return

  for i in range(MAX_THREAD_NUMBER):
    TASK_QUEUE.put(None)

  # Give worker threads a chance to exit.
  # Workaround the harmless bug in python 2.7 below.
  time.sleep(1)


atexit.register(SignalWorkerThreads)


def Worker():
  global TASK_QUEUE
  while True:
    try:
      task = TASK_QUEUE.get()
      if not task:
        return
    except TypeError:
      # According to http://bugs.python.org/issue14623, this is a harmless bug
      # in python 2.7 which won't be fixed.
      # The exception is raised on daemon threads when python interpreter is
      # shutting down.
      return

    function, args, kwargs, result_semaphore = task
    try:
      function(*args, **kwargs)
    except:
      pass
    finally:
      # Signal one task is done in case of exception.
      result_semaphore.release()


def RunTasks(tasks):
  """Run given tasks. Not thread-safe: no concurrent calls of this function.

  Return after all tasks were completed. A task is a dict as below:
    {
      'function': the function to call,
      'args': the positional argument to pass to the function,
      'kwargs': the key-value arguments to pass to the function,
    }
  """
  if not tasks:
    return

  global TASK_QUEUE
  if not TASK_QUEUE:
    TASK_QUEUE = Queue.Queue()
    for index in range(MAX_THREAD_NUMBER):
      thread = threading.Thread(target=Worker, name='worker_%s' % index)
      # Set as daemon, so no join is needed.
      thread.daemon = True
      thread.start()

  result_semaphore = threading.Semaphore(0)
  # Push task to task queue for execution.
  for task in tasks:
    TASK_QUEUE.put(
        (task['function'], task.get('args', []),
         task.get('kwargs', {}), result_semaphore))

  # Wait until all tasks to be executed.
  for _ in tasks:
    result_semaphore.acquire()


def GetRepositoryType(revision_number):
  """Returns the repository type of this revision number.

  Args:
    revision_number: A revision number or git hash.

  Returns:
    'git' or 'svn', depending on the revision_number.
  """
  if utils.IsGitHash(revision_number):
    return 'git'
  else:
    return 'svn'


def ParseURLsFromConfig(file_name):
  """Parses URLS from the config file.

  The file should be in python config format, where svn section is in the
  format "svn:component_path".
  Each of the section for svn should contain changelog_url, revision_url,
  diff_url and blame_url.

  Args:
    file_name: The name of the file that contains URL information.

  Returns:
    A dictionary that maps repository type to list of URLs. For svn, it maps
    key 'svn' to another dictionary, which maps component path to the URLs
    as explained above. For git, it maps to the URLs as explained above.
  """
  config = ConfigParser.ConfigParser()

  # Get the absolute path of the config file, and read the file. If it fails,
  # return none.
  config_file_path = os.path.join(os.path.abspath(os.path.dirname(__file__)),
                                  file_name)
  config.read(config_file_path)
  if not config:
    return None

  # Iterate through the config file, check for sections.
  config_dict = {}
  for section in config.sections():
    # These two do not need another layer of dictionary, so add it and go
    # to next section.
    if ':' not in section:
      for option in config.options(section):
        if section not in config_dict:
          config_dict[section] = {}

        url = config.get(section, option)
        config_dict[section][option] = url

      continue

    # Get repository type and component name from the section name.
    repository_type_and_component = section.split(':')
    repository_type = repository_type_and_component[0]
    component_path = repository_type_and_component[1]

    # Add 'svn' as the key, if it is not already there.
    if repository_type not in config_dict:
      config_dict[repository_type] = {}
    url_map_for_repository = config_dict[repository_type]

    # Add the path to the 'svn', if it is not already there.
    if component_path not in url_map_for_repository:
      url_map_for_repository[component_path] = {}
    type_to_url = url_map_for_repository[component_path]

    # Add all URLs to this map.
    for option in config.options(section):
      url = config.get(section, option)
      type_to_url[option] = url

  return config_dict


def NormalizePath(path, parsed_deps):
  """Normalizes the path.

  Args:
    path: A string representing a path.
    parsed_deps: A map from component path to its component name, repository,
                 etc.

  Returns:
    A tuple containing a component this path is in (e.g blink, skia, etc)
    and a path in that component's repository. Returns None if the component
    repository is not supported, i.e from googlecode.
  """
  # First normalize the path by retreiving the normalized path.
  normalized_path = os.path.normpath(path).replace('\\', '/')

  # Iterate through all component paths in the parsed DEPS, in the decreasing
  # order of the length of the file path.
  for component_path in sorted(parsed_deps,
                               key=(lambda path: -len(path))):
    # new_component_path is the component path with 'src/' removed.
    new_component_path = component_path
    if new_component_path.startswith('src/') and new_component_path != 'src/':
      new_component_path = new_component_path[len('src/'):]

    # We need to consider when the lowercased component path is in the path,
    # because syzyasan build returns lowercased file path.
    lower_component_path = new_component_path.lower()

    # If this path is the part of file path, this file must be from this
    # component.
    if new_component_path in normalized_path or \
        lower_component_path in normalized_path:

      # Case when the retreived path is in lowercase.
      if lower_component_path in normalized_path:
        current_component_path = lower_component_path
      else:
        current_component_path = new_component_path

      # Normalize the path by stripping everything off the component's relative
      # path.
      normalized_path = normalized_path.split(current_component_path, 1)[1]
      lower_normalized_path = normalized_path.lower()

      # Add 'src/' or 'Source/' at the front of the normalized path, depending
      # on what prefix the component path uses. For example, blink uses
      # 'Source' but chromium uses 'src/', and blink component path is
      # 'src/third_party/WebKit/Source', so add 'Source/' in front of the
      # normalized path.
      if not (lower_normalized_path.startswith('src/') or
              lower_normalized_path.startswith('source/')):

        if (lower_component_path.endswith('src/') or
            lower_component_path.endswith('source/')):
          normalized_path = (current_component_path.split('/')[-2] + '/' +
                             normalized_path)

        else:
          normalized_path = 'src/' + normalized_path

      component_name = parsed_deps[component_path]['name']

      return (component_path, component_name, normalized_path)

  # If the path does not match any component, default to chromium.
  return ('src/', 'chromium', normalized_path)


def SplitRange(regression):
  """Splits a range as retrieved from clusterfuzz.

  Args:
    regression: A string in format 'r1234:r5678'.

  Returns:
    A list containing two numbers represented in string, for example
    ['1234','5678'].
  """
  if not regression:
    return None

  revisions = regression.split(':')

  # If regression information is not available, return none.
  if len(revisions) != 2:
    return None

  range_start = revisions[0]
  range_end = revisions[1]

  # Strip 'r' off the range start/end. Not using lstrip to avoid the case when
  # the range is in git hash and it starts with 'r'.
  if range_start.startswith('r'):
    range_start = range_start[1:]

  if range_end.startswith('r'):
    range_end = range_end[1:]

  return [range_start, range_end]


def LoadJSON(json_string):
  """Loads json object from string, or None.

  Args:
    json_string: A string to get object from.

  Returns:
    JSON object if the string represents a JSON object, None otherwise.
  """
  try:
    data = json.loads(json_string)
  except ValueError:
    data = None

  return data


def GetDataFromURL(url):
  """Retrieves raw data from URL, tries 10 times.

  Args:
    url: URL to get data from.
    retries: Number of times to retry connection.

  Returns:
    None if the data retrieval fails, or the raw data.
  """
  status_code, data = utils.GetHttpClient().Get(url, retries=10)
  if status_code == 200:
    return data
  else:
    # Return None if it fails to read data.
    return None


def FindMinLineDistance(crashed_line_list, changed_line_numbers,
                        line_range=3):
  """Calculates how far the changed line is from one of the crashes.

  Finds the minimum distance between the lines that the file crashed on
  and the lines that the file changed. For example, if the file crashed on
  line 200 and the CL changes line 203,204 and 205, the function returns 3.

  Args:
    crashed_line_list: A list of lines that the file crashed on.
    changed_line_numbers: A list of lines that the file changed.
    line_range: Number of lines to look back for.

  Returns:
    The minimum distance. If either of the input lists is empty,
    it returns inf.

  """
  min_distance = INFINITY
  crashed_line = -1
  changed_line = -1

  crashed_line_numbers = set()
  for crashed_line_range in crashed_line_list:
    for crashed_line in crashed_line_range:
      for line in range(crashed_line - line_range, crashed_line + 1):
        crashed_line_numbers.add(line)

  for line in crashed_line_numbers:
    for distance in changed_line_numbers:
      # Find the current distance and update the min if current distance is
      # less than current min.
      current_distance = abs(line - distance)
      if current_distance < min_distance:
        min_distance = current_distance
        crashed_line = line
        changed_line = distance

  return (min_distance, crashed_line, changed_line)


def GuessIfSameSubPath(path1, path2):
  """Guesses if two paths represent same path.

  Compares the name of the folders in the path (by split('/')), and checks
  if they match either more than 3 or min of path lengths.

  Args:
    path1: First path.
    path2: Second path to compare.

  Returns:
    True if it they are thought to be a same path, False otherwise.
  """
  path1 = path1.split('/')
  path2 = path2.split('/')

  intersection = set(path1).intersection(set(path2))
  return len(intersection) >= (min(3, min(len(path1), len(path2))))


def FindMinStackFrameNumber(stack_frame_indices, priorities):
  """Finds the minimum stack number, from the list of stack numbers.

  Args:
    stack_frame_indices: A list of lists containing stack position.
    priorities: A list of of priority for each file.

  Returns:
    Inf if stack_frame_indices is empty, minimum stack number otherwise.
  """
  # Get the indexes of the highest priority (or low priority number).
  highest_priority = min(priorities)
  highest_priority_indices = []
  for i in range(len(priorities)):
    if priorities[i] == highest_priority:
      highest_priority_indices.append(i)

  # Gather the list of stack frame numbers for the files that change the
  # crash lines.
  flattened = []
  for i in highest_priority_indices:
    flattened += stack_frame_indices[i]

  # If no stack frame information is available, return inf. Else, return min.
  if not flattened:
    return INFINITY
  else:
    return min(flattened)


def AddHyperlink(text, link):
  """Returns a string with HTML link tag.

  Args:
    text: A string to add link.
    link: A link to add to the string.

  Returns:
    A string with hyperlink added.
  """
  sanitized_link = cgi.escape(link, quote=True)
  sanitized_text = cgi.escape(str(text))
  return '<a href="%s">%s</a>' % (sanitized_link, sanitized_text)


def PrettifyList(items):
  """Returns a string representation of a list.

  It adds comma in between the elements and removes the brackets.
  Args:
    items: A list to prettify.
  Returns:
    A string representation of the list.
  """
  return ', '.join(map(str, items))


def PrettifyFrameInfo(frame_indices, functions):
  """Return a string to represent the frames with functions."""
  frames = []
  for frame_index, function in zip(frame_indices, functions):
    frames.append('frame #%s, "%s"' % (frame_index, function.split('(')[0]))
  return '; '.join(frames)


def PrettifyFiles(file_list):
  """Returns a string representation of a list of file names.

  Args:
    file_list: A list of tuple, (file_name, file_url).
  Returns:
    A string representation of file names with their urls.
  """
  ret = ['\n']
  for file_name, file_url in file_list:
    ret.append('      %s\n' % AddHyperlink(file_name, file_url))
  return ''.join(ret)


def Intersection(crashed_line_list, stack_frame_index, changed_line_numbers,
                 function, line_range=3):
  """Finds the overlap betwee changed lines and crashed lines.

  Finds the intersection of the lines that caused the crash and
  lines that the file changes. The intersection looks within 3 lines
  of the line that caused the crash.

  Args:
    crashed_line_list: A list of lines that the file crashed on.
    stack_frame_index: A list of positions in stack for each of the lines.
    changed_line_numbers: A list of lines that the file changed.
    function: A list of functions that the file crashed on.
    line_range: Number of lines to look backwards from crashed lines.

  Returns:
    line_number_intersection: Intersection between crashed_line_list and
                       changed_line_numbers.
    stack_frame_index_intersection: Stack number for each of the intersections.
  """
  line_number_intersection = []
  stack_frame_index_intersection = []
  function_intersection = []

  # Iterate through the crashed lines, and its occurence in stack.
  for (lines, stack_frame_index, function_name) in zip(
      crashed_line_list, stack_frame_index, function):
    # Also check previous 'line_range' lines. Create a set of all changed lines
    # and lines within 3 lines range before the crashed line.
    line_minus_n = set()
    for line in lines:
      for line_in_range in range(line - line_range, line + 1):
        line_minus_n.add(line_in_range)

    for changed_line in changed_line_numbers:
      # If a CL does not change crahsed line, check next line.
      if changed_line not in line_minus_n:
        continue

      intersected_line = set()
      # If the changed line is exactly the crashed line, add that line.
      for line in lines:
        if line in changed_line_numbers:
          intersected_line.add(line)

        # If the changed line is in 3 lines of the crashed line, add the line.
        else:
          intersected_line.add(changed_line)

      # Avoid adding the same line twice.
      if intersected_line not in line_number_intersection:
        line_number_intersection.append(list(intersected_line))
        stack_frame_index_intersection.append(stack_frame_index)
        function_intersection.append(function_name)
      break

  return (line_number_intersection, stack_frame_index_intersection,
          function_intersection)


def MatchListToResultList(matches):
  """Convert list of matches to the list of result objects.

  Args:
    matches: A list of match objects along with its stack priority and revision
             number/git hash
  Returns:
    A list of result object.

  """
  result_list = []

  for _, cl, match in matches:
    suspected_cl = cl
    revision_url = match.revision_url
    component_name = match.component_name
    author = match.author
    reason = match.reason
    review_url = match.review_url
    reviewers = match.reviewers
    # For matches, line content do not exist.
    line_content = None
    message = match.message

    result = Result(suspected_cl, revision_url, component_name, author, reason,
                    review_url, reviewers, line_content, message)
    result_list.append(result)

  return result_list


def BlameListToResultList(blame_list):
  """Convert blame list to the list of result objects.

  Args:
    blame_list: A list of blame objects.

  Returns:
    A list of result objects.
  """
  result_list = []

  for blame in blame_list:
    suspected_cl = blame.revision
    revision_url = blame.url
    component_name = blame.component_name
    author = blame.author
    reason = (
        'The CL last changed line %s of file %s, which is stack frame %d.' %
        (blame.line_number, blame.file, blame.stack_frame_index))
    # Blame object does not have review url and reviewers.
    review_url = None
    reviewers = None
    line_content = blame.line_content
    message = blame.message

    result = Result(suspected_cl, revision_url, component_name, author, reason,
                    review_url, reviewers, line_content, message)
    result_list.append(result)

  return result_list
