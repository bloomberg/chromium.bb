# Copyright (c) 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import cgi
import ConfigParser
import json
import logging
import os
import time
import urllib2

from result import Result


INFINITY = float('inf')


def ParseURLsFromConfig(file_name):
  """Parses URLS from the config file.

  The file should be in python config format, where svn section is in the
  format "svn:component_path", except for git URLs and codereview URL.
  Each of the section for svn should contain changelog_url, revision_url,
  diff_url and blame_url.

  Args:
    file_name: The name of the file that contains URL information.

  Returns:
    A dictionary that maps repository type to list of URLs. For svn, it maps
    key 'svn' to another dictionary, which maps component path to the URLs
    as explained above. For git, it maps to the URLs as explained above.
    Codereview maps to codereview API url.
  """
  config = ConfigParser.ConfigParser()

  # Get the absolute path of the config file, and read the file. If it fails,
  # return none.
  config_file_path = os.path.join(os.path.abspath(os.path.dirname(__file__)),
                                  file_name)
  config.read(config_file_path)
  if not config:
    logging.error('Config file with URLs does not exist.')
    return None

  # Iterate through the config file, check for sections.
  repository_type_to_url_map = {}
  for section in config.sections():
    # These two do not need another layer of dictionary, so add it and go
    # to next section.
    if section == 'git' or section == 'codereview':
      for option in config.options(section):
        if section not in repository_type_to_url_map:
          repository_type_to_url_map[section] = {}

        url = config.get(section, option)
        repository_type_to_url_map[section][option] = url

      continue

    # Get repository type and component name from the section name.
    repository_type_and_component = section.split(':')
    repository_type = repository_type_and_component[0]
    component_path = repository_type_and_component[1]

    # Add 'svn' as the key, if it is not already there.
    if repository_type not in repository_type_to_url_map:
      repository_type_to_url_map[repository_type] = {}
    url_map_for_repository = repository_type_to_url_map[repository_type]

    # Add the path to the 'svn', if it is not already there.
    if component_path not in url_map_for_repository:
      url_map_for_repository[component_path] = {}
    type_to_url = url_map_for_repository[component_path]

    # Add all URLs to this map.
    for option in config.options(section):
      url = config.get(section, option)
      type_to_url[option] = url

  return repository_type_to_url_map


def NormalizePathLinux(path, parsed_deps):
  """Normalizes linux path.

  Args:
    path: A string representing a path.
    parsed_deps: A map from component path to its component name, repository,
                 etc.

  Returns:
    A tuple containing a component this path is in (e.g blink, skia, etc)
    and a path in that component's repository.
  """
  # First normalize the path by retreiving the absolute path.
  normalized_path = os.path.abspath(path)

  # Iterate through all component paths in the parsed DEPS, in the decreasing
  # order of the length of the file path.
  for component_path in sorted(parsed_deps,
                               key=(lambda path: -len(path))):
    # New_path is the component path with 'src/' removed.
    new_path = component_path
    if new_path.startswith('src/') and new_path != 'src/':
      new_path = new_path[len('src/'):]

    # If this path is the part of file path, this file must be from this
    # component.
    if new_path in normalized_path:

      # Currently does not support googlecode.
      if 'googlecode' in parsed_deps[component_path]['repository']:
        return (None, '', '')

      # Normalize the path by stripping everything off the component's relative
      # path.
      normalized_path = normalized_path.split(new_path,1)[1]

      # Add 'src/' or 'Source/' at the front of the normalized path, depending
      # on what prefix the component path uses. For example, blink uses
      # 'Source' but chromium uses 'src/', and blink component path is
      # 'src/third_party/WebKit/Source', so add 'Source/' in front of the
      # normalized path.
      if not normalized_path.startswith('src/') or \
          normalized_path.startswith('Source/'):

        if (new_path.lower().endswith('src/') or
            new_path.lower().endswith('source/')):
          normalized_path = new_path.split('/')[-2] + '/' + normalized_path

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

  # Strip 'r' from both start and end range.
  range_start = revisions[0].lstrip('r')
  range_end = revisions[1].lstrip('r')

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


def GetDataFromURL(url, retries=10, sleep_time=0.1, timeout=10):
  """Retrieves raw data from URL, tries 10 times.

  Args:
    url: URL to get data from.
    retries: Number of times to retry connection.
    sleep_time: Time in seconds to wait before retrying connection.
    timeout: Time in seconds to wait before time out.

  Returns:
    None if the data retrieval fails, or the raw data.
  """
  data = None
  for i in range(retries):
    # Retrieves data from URL.
    try:
      data = urllib2.urlopen(url, timeout=timeout)

      # If retrieval is successful, return the data.
      if data:
        return data.read()

    # If retrieval fails, try after sleep_time second.
    except urllib2.URLError:
      time.sleep(sleep_time)
      continue
    except IOError:
      time.sleep(sleep_time)
      continue

  # Return None if it fails to read data from URL 'retries' times.
  return None


def FindMinLineDistance(crashed_line_list, changed_line_numbers):
  """Calculates how far the changed line is from one of the crashes.

  Finds the minimum distance between the lines that the file crashed on
  and the lines that the file changed. For example, if the file crashed on
  line 200 and the CL changes line 203,204 and 205, the function returns 3.

  Args:
    crashed_line_list: A list of lines that the file crashed on.
    changed_line_numbers: A list of lines that the file changed.

  Returns:
    The minimum distance. If either of the input lists is empty,
    it returns inf.

  """
  min_distance = INFINITY

  for line in crashed_line_list:
    for distance in changed_line_numbers:
      # Find the current distance and update the min if current distance is
      # less than current min.
      current_distance = abs(line - distance)
      if current_distance < min_distance:
        min_distance = current_distance

  return min_distance


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


def PrettifyList(l):
  """Returns a string representation of a list.

  It adds comma in between the elements and removes the brackets.
  Args:
    l: A list to prettify.
  Returns:
    A string representation of the list.
  """
  return str(l)[1:-1]


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
                 line_range=3):
  """Finds the overlap betwee changed lines and crashed lines.

  Finds the intersection of the lines that caused the crash and
  lines that the file changes. The intersection looks within 3 lines
  of the line that caused the crash.

  Args:
    crashed_line_list: A list of lines that the file crashed on.
    stack_frame_index: A list of positions in stack for each of the lines.
    changed_line_numbers: A list of lines that the file changed.
    line_range: Number of lines to look backwards from crashed lines.

  Returns:
    line_intersection: Intersection between crashed_line_list and
                       changed_line_numbers.
    stack_frame_index_intersection: Stack number for each of the intersections.
  """
  line_intersection = []
  stack_frame_index_intersection = []

  # Iterate through the crashed lines, and its occurence in stack.
  for (line, stack_frame_index) in zip(crashed_line_list, stack_frame_index):
    # Also check previous 'line_range' lines.
    line_minus_n = range(line - line_range, line + 1)

    for changed_line in changed_line_numbers:
      # If a CL does not change crahsed line, check next line.
      if changed_line not in line_minus_n:
        continue

      # If the changed line is exactly the crashed line, add that line.
      if line in changed_line_numbers:
        intersected_line = line

      # If the changed line is in 3 lines of the crashed line, add the line.
      else:
        intersected_line = changed_line

      # Avoid adding the same line twice.
      if intersected_line not in line_intersection:
        line_intersection.append(intersected_line)
        stack_frame_index_intersection.append(stack_frame_index)

      break

  return (line_intersection, stack_frame_index_intersection)


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

    result = Result(suspected_cl, revision_url, component_name, author, reason,
                    review_url, reviewers, line_content)
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
        'The CL changes line %s of file %s from stack %d.' %
        (blame.line_number, blame.file, blame.stack_frame_index))
    # Blame object does not have review url and reviewers.
    review_url = None
    reviewers = None
    line_content = blame.content

    result = Result(suspected_cl, revision_url, component_name, author, reason,
                    review_url, reviewers, line_content)
    result_list.append(result)

  return result_list


def ResultListToJSON(result_list):
  """Converts result list to JSON format.

  Args:
    result_list: A list of result objects

  Returns:
    A string, JSON format of the result_list.

  """
  return json.dumps([result.ToDictionary() for result in result_list])
