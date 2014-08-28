# Copyright (c) 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import re

import crash_utils


SYZYASAN_STACK_FRAME_PATTERN = re.compile(
    r'(CF: )?(.*?)( \(FPO: .*\) )?( \(CONV: .*\) )?\[(.*) @ (\d+)\]')
FILE_PATH_AND_LINE_PATTERN = re.compile(r'(.*?):(\d+)(:\d+)?')


class StackFrame(object):
  """Represents a frame in stacktrace.

  Attributes:
    index: An index of the stack frame.
    component_path: The path of the component this frame represents.
    component_name: The name of the component this frame represents.
    file_name: The name of the file that crashed.
    function: The function that caused the crash.
    file_path: The path of the crashed file.
    crashed_line_range: The line of the file that caused the crash.
  """

  def __init__(self, stack_frame_index, component_path, component_name,
               file_name, function, file_path, crashed_line_range):
    self.index = stack_frame_index
    self.component_path = component_path
    self.component_name = component_name
    self.file_name = file_name
    self.function = function
    self.file_path = file_path
    self.crashed_line_range = crashed_line_range


class CallStack(object):
  """Represents a call stack within a stacktrace.

  It is a list of StackFrame object, and the object keeps track of whether
  the stack is crash stack, freed or previously-allocated.
  """

  def __init__(self, stack_priority):
    self.frame_list = []
    self.priority = stack_priority

  def Add(self, stacktrace_line):
    self.frame_list.append(stacktrace_line)

  def GetTopNFrames(self, n):
    return self.frame_list[:n]


class Stacktrace(object):
  """Represents Stacktrace object.

  Contains a list of callstacks, because one stacktrace might have more than
  one callstacks.
  """

  def __init__(self, stacktrace, build_type, parsed_deps):
    self.stack_list = None
    self.ParseStacktrace(stacktrace, build_type, parsed_deps)

  def ParseStacktrace(self, stacktrace, build_type, parsed_deps):
    """Parses stacktrace and normalizes it.

    If there are multiple callstacks within the stacktrace,
    it will parse each of them separately, and store them in the stack_list
    variable.

    Args:
      stacktrace: A string containing stacktrace.
      build_type: A string containing the build type of the crash.
      parsed_deps: A parsed DEPS file to normalize path with.
    """
    # If the passed in string is empty, the object does not represent anything.
    if not stacktrace:
      return
    # Reset the stack list.
    self.stack_list = []
    reached_new_callstack = False
    # Note that we do not need exact stack frame index, we only need relative
    # position of a frame within a callstack. The reason for not extracting
    # index from a line is that some stack frames do not have index.
    stack_frame_index = 0
    current_stack = CallStack(-1)

    for line in stacktrace:
      line = line.strip()
      (is_new_callstack, stack_priority) = self.__IsStartOfNewCallStack(
          line, build_type)
      if is_new_callstack:
        # If this callstack is crash stack, update the boolean.
        if not reached_new_callstack:
          reached_new_callstack = True
          current_stack = CallStack(stack_priority)

        # If this is from freed or allocation, add the callstack we have
        # to the list of callstacks, and increment the stack priority.
        else:
          stack_frame_index = 0
          if current_stack and current_stack.frame_list:
            self.stack_list.append(current_stack)
          current_stack = CallStack(stack_priority)

      # Generate stack frame object from the line.
      parsed_stack_frame = self.__GenerateStackFrame(
          stack_frame_index, line, build_type, parsed_deps)

      # If the line does not represent the stack frame, ignore this line.
      if not parsed_stack_frame:
        continue

      # Add the parsed stack frame object to the current stack.
      current_stack.Add(parsed_stack_frame)
      stack_frame_index += 1

    # Add the current callstack only if there are frames in it.
    if current_stack and current_stack.frame_list:
      self.stack_list.append(current_stack)

  def __IsStartOfNewCallStack(self, line, build_type):
    """Check if this line is the start of the new callstack.

    Since each builds have different format of stacktrace, the logic for
    checking the line for all builds is handled in here.

    Args:
      line: Line to check for.
      build_type: The name of the build.

    Returns:
      True if the line is the start of new callstack, False otherwise. If True,
      it also returns the priority of the line.
    """
    if 'syzyasan' in build_type:
      # In syzyasan build, new stack starts with 'crash stack:',
      # 'freed stack:', etc.
      callstack_start_pattern = re.compile(r'^(.*) stack:$')
      match = callstack_start_pattern.match(line)

      # If the line matches the callstack start pattern.
      if match:
        # Check the type of the new match.
        stack_type = match.group(1)

        # Crash stack gets priority 0.
        if stack_type == 'Crash':
          return (True, 0)

        # Other callstacks all get priority 1.
        else:
          return (True, 1)

    elif 'tsan' in build_type:
      # Create patterns for each callstack type.
      crash_callstack_start_pattern1 = re.compile(
          r'^(Read|Write) of size \d+')

      crash_callstack_start_pattern2 = re.compile(
          r'^[A-Z]+: ThreadSanitizer')

      allocation_callstack_start_pattern = re.compile(
          r'^Previous (write|read) of size \d+')

      location_callstack_start_pattern = re.compile(
          r'^Location is heap block of size \d+')

      # Crash stack gets priority 0.
      if (crash_callstack_start_pattern1.match(line) or
          crash_callstack_start_pattern2.match(line)):
        return (True, 0)

      # All other stacks get priority 1.
      if allocation_callstack_start_pattern.match(line):
        return (True, 1)

      if location_callstack_start_pattern.match(line):
        return (True, 1)

    else:
      # In asan and other build types, crash stack can start
      # in two different ways.
      crash_callstack_start_pattern1 = re.compile(r'^==\d+== ?[A-Z]+:')
      crash_callstack_start_pattern2 = re.compile(
          r'^(READ|WRITE) of size \d+ at')
      crash_callstack_start_pattern3 = re.compile(r'^backtrace:')

      freed_callstack_start_pattern = re.compile(
          r'^freed by thread T\d+ (.* )?here:')

      allocation_callstack_start_pattern = re.compile(
          r'^previously allocated by thread T\d+ (.* )?here:')

      other_callstack_start_pattern = re.compile(
          r'^Thread T\d+ (.* )?created by')

      # Crash stack gets priority 0.
      if (crash_callstack_start_pattern1.match(line) or
          crash_callstack_start_pattern2.match(line) or
          crash_callstack_start_pattern3.match(line)):
        return (True, 0)

      # All other callstack gets priority 1.
      if freed_callstack_start_pattern.match(line):
        return (True, 1)

      if allocation_callstack_start_pattern.match(line):
        return (True, 1)

      if other_callstack_start_pattern.match(line):
        return (True, 1)

    # If the line does not match any pattern, return false and a dummy for
    # stack priority.
    return (False, -1)

  def __GenerateStackFrame(self, stack_frame_index, line, build_type,
                           parsed_deps):
    """Extracts information from a line in stacktrace.

    Args:
      stack_frame_index: A stack frame index of this line.
      line: A stacktrace string to extract data from.
      build_type: A string containing the build type
                    of this crash (e.g. linux_asan_chrome_mp).
      parsed_deps: A parsed DEPS file to normalize path with.

    Returns:
      A triple containing the name of the function, the path of the file and
      the crashed line number.
    """
    line_parts = line.split()
    try:

      if 'syzyasan' in build_type:
        stack_frame_match = SYZYASAN_STACK_FRAME_PATTERN.match(line)

        if not stack_frame_match:
          return None
        file_path = stack_frame_match.group(5)
        crashed_line_range = [int(stack_frame_match.group(6))]
        function = stack_frame_match.group(2)

      else:
        if not line_parts[0].startswith('#'):
          return None

        if 'tsan' in build_type:
          file_path_and_line = line_parts[-2]
          function = ' '.join(line_parts[1:-2])
        else:
          file_path_and_line = line_parts[-1]
          function = ' '.join(line_parts[3:-1])

        # Get file path and line info from the line.
        file_path_and_line_match = FILE_PATH_AND_LINE_PATTERN.match(
            file_path_and_line)

        # Return None if the file path information is not available
        if not file_path_and_line_match:
          return None

        file_path = file_path_and_line_match.group(1)

        # Get the crashed line range. For example, file_path:line_number:range.
        crashed_line_range_num = file_path_and_line_match.group(3)

        if crashed_line_range_num:
          # Strip ':' prefix.
          crashed_line_range_num = int(crashed_line_range_num[1:])
        else:
          crashed_line_range_num = 0

        crashed_line_number = int(file_path_and_line_match.group(2))
        # For example, 655:1 has crashed lines 655 and 656.
        crashed_line_range = \
            range(crashed_line_number,
                  crashed_line_number + crashed_line_range_num + 1)

    # Return None if the line is malformed.
    except IndexError:
      return None
    except ValueError:
      return None

    # Normalize the file path so that it can be compared to repository path.
    (component_path, component_name, file_path) = (
        crash_utils.NormalizePath(file_path, parsed_deps))

    # Return a new stack frame object with the parsed information.
    file_name = file_path.split('/')[-1]

    # If we have the common stack frame index pattern, then use it
    # since it is more reliable.
    index_match = re.match('\s*#(\d+)\s.*', line)
    if index_match:
      stack_frame_index = int(index_match.group(1))

    return StackFrame(stack_frame_index, component_path, component_name,
                      file_name, function, file_path, crashed_line_range)

  def __getitem__(self, index):
    return self.stack_list[index]

  def GetCrashStack(self):
    """Returns the callstack with the highest priority.

    Crash stack has priority 0, and allocation/freed/other thread stacks
    get priority 1.

    Returns:
      The highest priority callstack in the stacktrace.
    """
    sorted_stacklist = sorted(self.stack_list,
                              key=lambda callstack: callstack.priority)
    return sorted_stacklist[0]
