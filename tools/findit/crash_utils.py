# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import cgi
import json
import os
import time
import urllib


INFINITY = float('inf')


def NormalizePathLinux(path):
  """Normalizes linux path.

  Args:
    path: A string representing a path.

  Returns:
    A tuple containing a component this path is in (e.g blink, skia, etc)
    and a path in that component's repository.
  """
  normalized_path = os.path.abspath(path)

  if 'src/v8/' in normalized_path:
    component = 'v8'
    normalized_path = normalized_path.split('src/v8/')[1]

  # TODO(jeun): Integrate with parsing DEPS file.
  if 'WebKit/' in normalized_path:
    component = 'blink'
    normalized_path = ''.join(path.split('WebKit/')[1:])
  else:
    component = 'chromium'

  if '/build/' in normalized_path:
    normalized_path = normalized_path.split('/build/')[-1]

  if not (normalized_path.startswith('src/') or
      normalized_path.startswith('Source/')):
    normalized_path = 'src/' + normalized_path

  return (component, normalized_path)


def SplitRange(regression):
  """Splits a range as retrieved from clusterfuzz.

  Args:
    regression: A string in format 'r1234:r5678'.

  Returns:
    A list containing two numbers represented in string, for example
    ['1234','5678'].
  """
  revisions = regression.split(':')

  # If regression information is not available, return none.
  if len(revisions) != 2:
    return None

  # Strip 'r' from both start and end range.
  start_range = revisions[0].lstrip('r')
  end_range = revisions[1].lstrip('r')

  return [start_range, end_range]


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


def GetDataFromURL(url, retries=10, sleep_time=0.1):
  """Retrieves raw data from URL, tries 10 times.

  Args:
    url: URL to get data from.
    retries: Number of times to retry connection.
    sleep_time: Time in seconds to wait before retrying connection.

  Returns:
    None if the data retrieval fails, or the raw data.
  """
  data = None
  for i in range(retries):
    # Retrieves data from URL.
    try:
      data = urllib.urlopen(url)

      # If retrieval is successful, return the data.
      if data:
        return data.read()

    # If retrieval fails, try after sleep_time second.
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
  sanitized_text = cgi.escape(text)
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
