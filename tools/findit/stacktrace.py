# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import re

import crash_utils


class StackFrame(object):
  """Represents a frame in stacktrace.

  Attributes:
    index: An index of the stack frame.
    component: A component this line represents, such as blink, chrome, etc.
    file_name: The name of the file that crashed.
    function: The function that caused the crash.
    file_path: The path of the crashed file.
    crashed_line_number: The line of the file that caused the crash.
  """

  def __init__(self, stack_frame_index, component, file_name,
               function, file_path, crashed_line_number):
    self.index = stack_frame_index
    self.component = component
    self.file_name = file_name
    self.function = function
    self.file_path = file_path
    self.crashed_line_number = crashed_line_number


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

  def __init__(self, stacktrace, build_type):
    self.stack_list = []
    self.ParseStacktrace(stacktrace, build_type)

  def ParseStacktrace(self, stacktrace, build_type):
    """Parses stacktrace and normalizes it.

    If there are multiple callstacks within the stacktrace,
    it will parse each of them separately, and store them in the stack_list
    variable.

    Args:
      stacktrace: A string containing stacktrace.
      build_type: A string containing the build type of the crash.
    """
    # If the passed in string is empty, the object does not represent anything.
    if not stacktrace:
      self.stack_list = None
      return

    # Reset the stack list.
    self.stack_list = []
    reached_new_callstack = False
    # Note that we do not need exact stack frame index, we only need relative
    # position of a frame within a callstack. The reason for not extracting
    # index from a line is that some stack frames do not have index.
    stack_frame_index = 0
    current_stack = None

    for line in stacktrace:
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
          stack_frame_index, line, build_type)

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
    # Currently not supported.
    if 'android' in build_type:
      pass

    elif 'syzyasan' in build_type:
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
      crash_callstack_start_pattern = re.compile(
          r'^(Read|Write) of size \d+')

      allocation_callstack_start_pattern = re.compile(
          r'^Previous (write|read) of size \d+')

      location_callstack_start_pattern = re.compile(
          r'^Location is heap block of size \d+')

      # Crash stack gets priority 0.
      if crash_callstack_start_pattern.match(line):
        return (True, 0)

      # All other stacks get priority 1.
      if allocation_callstack_start_pattern.match(line):
        return (True, 1)

      if location_callstack_start_pattern.match(line):
        return (True, 1)

    else:
      # In asan and other build types, crash stack can start
      # in two different ways.
      crash_callstack_start_pattern1 = re.compile(r'^==\d+== ?ERROR:')
      crash_callstack_start_pattern2 = re.compile(
          r'^(READ|WRITE) of size \d+ at')

      freed_callstack_start_pattern = re.compile(
          r'^freed by thread T\d+ (.* )?here:')

      allocation_callstack_start_pattern = re.compile(
          r'^previously allocated by thread T\d+ (.* )?here:')

      other_callstack_start_pattern = re.compile(
          r'^Thread T\d+ (.* )?created by')

      # Crash stack gets priority 0.
      if (crash_callstack_start_pattern1.match(line) or
          crash_callstack_start_pattern2.match(line)):
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

  def __GenerateStackFrame(self, stack_frame_index, line, build_type):
    """Extracts information from a line in stacktrace.

    Args:
      stack_frame_index: A stack frame index of this line.
      line: A stacktrace string to extract data from.
      build_type: A string containing the build type
                    of this crash (e.g. linux_asan_chrome_mp).

    Returns:
      A triple containing the name of the function, the path of the file and
      the crashed line number.
    """
    line_parts = line.split()

    try:
      # Filter out lines that are not stack frame.
      stack_frame_index_pattern = re.compile(r'#(\d+)')
      if not stack_frame_index_pattern.match(line_parts[0]):
        return None

      # Tsan has different stack frame style from other builds.
      if build_type.startswith('linux_tsan'):
        file_path_and_line = line_parts[-2]
        function = ' '.join(line_parts[1:-2])

      else:
        file_path_and_line = line_parts[-1]
        function = ' '.join(line_parts[3:-1])

      # Get file path and line info from the line.
      file_path_and_line = file_path_and_line.split(':')
      file_path = file_path_and_line[0]
      crashed_line_number = int(file_path_and_line[1])

    # Return None if the line is malformed.
    except IndexError:
      return None
    except ValueError:
      return None

    # Normalize the file path so that it can be compared to repository path.
    file_name = os.path.basename(file_path)
    (component, file_path) = crash_utils.NormalizePathLinux(file_path)

    # FIXME(jeun): Add other components.
    if not (component == 'blink' or component == 'chromium'):
      return None

    # Return a new stack frame object with the parsed information.
    return StackFrame(stack_frame_index, component, file_name, function,
                      file_path, crashed_line_number)

  def __getitem__(self, index):
    return self.stack_list[index]

  def GetCrashStack(self):
    for callstack in self.stack_list:
      # Only the crash stack has the priority 0.
      if callstack.priority == 0:
        return callstack
