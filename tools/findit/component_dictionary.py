# Copyright (c) 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os


class FileDictionary(object):
  """Maps file in a stacktrace to its crash information.

  It maps file to another dictionary, which maps the file's path to crashed
  lines, stack frame indices and crashed functions.
  """

  def __init__(self):
    """Initializes the file dictionary."""
    self.file_dict = {}

  def AddFile(self, file_name, file_path, crashed_line_number,
              stack_frame_index, function):
    """Adds file and its crash information to the map.

    Args:
      file_name: The name of the crashed file.
      file_path: The path of the crashed file.
      crashed_line_number: The crashed line of the file.
      stack_frame_index: The file's position in the callstack.
      function: The name of the crashed function.
    """
    # Populate the dictionary if this file/path has not been added before.
    if file_name not in self.file_dict:
      self.file_dict[file_name] = {}

    if file_path not in self.file_dict[file_name]:
      self.file_dict[file_name][file_path] = {}
      self.file_dict[file_name][file_path]['line_numbers'] = []
      self.file_dict[file_name][file_path]['stack_frame_indices'] = []
      self.file_dict[file_name][file_path]['function'] = []

    # Add the crashed line, frame index and function name.
    self.file_dict[file_name][file_path]['line_numbers'].append(
        crashed_line_number)
    self.file_dict[file_name][file_path]['stack_frame_indices'].append(
        stack_frame_index)
    self.file_dict[file_name][file_path]['function'].append(function)

  def GetPathDic(self, file_name):
    """Returns file's path and crash information."""
    return self.file_dict[file_name]

  def GetCrashedLineNumbers(self, file_path):
    """Returns crashed line numbers given a file path."""
    file_name = os.path.basename(file_path)
    return self.file_dict[file_name][file_path]['line_numbers']

  def GetCrashStackFrameindex(self, file_path):
    """Returns stack frame indices given a file path."""
    file_name = os.path.basename(file_path)
    return self.file_dict[file_name][file_path]['stack_frame_indices']

  def GetCrashFunction(self, file_path):
    """Returns list of crashed functions given a file path."""
    file_name = os.path.basename(file_path)
    return self.file_dict[file_name][file_path]['function']

  def __iter__(self):
    return iter(self.file_dict)


class ComponentDictionary(object):
  """Represents a file dictionary.

  It maps each component path to a file dictionary.
  """

  def __init__(self, callstack, components):
    """Initializes the dictionary with given components."""
    self.component_dict = {}

    # Create file dictionary for all the components.
    for component in components:
      self.component_dict[component] = FileDictionary()

    # Create file dict from callstack.
    self.__CreateFileDictFromCallstack(callstack)

  def GetFileDict(self, component):
    """Returns a file dictionary for a given component."""
    return self.component_dict.get(component)

  def __GenerateFileDict(self, stack_frame_list):
    """Generates file dictionary, given an instance of StackFrame list."""
    # Iterate through the list of stackframe objects.
    for stack_frame in stack_frame_list:
      # If the component of this line is not in the list of components to
      # look for, ignore this line.
      component_path = stack_frame.component_path
      if component_path not in self.component_dict:
        continue

      # Get values of the variables
      file_name = stack_frame.file_name
      file_path = stack_frame.file_path
      crashed_line_number = stack_frame.crashed_line_number
      stack_frame_index = stack_frame.index
      function = stack_frame.function

      # Add the file to this component's dictionary of files.
      file_dict = self.component_dict[component_path]
      file_dict.AddFile(file_name, file_path, crashed_line_number,
                        stack_frame_index, function)

  def __CreateFileDictFromCallstack(self, callstack, top_n_frames=15):
    """Creates a file dict that maps a file to the occurrence in the stack.

    Args:
      callstack: A list containing parsed result from a single stack
                  within a stacktrace. For example, a stacktrace from
                  previously-allocated thread in release build stacktrace.
      top_n_frames: The number of frames to look for.

    Returns:
      Component_dict, a dictionary with key as a file name and value as another
      dictionary, which maps the file's path (because there can be multiple
      files with same name but in different directory) to the list of this
      file's place in stack, lines that this file caused a crash, and the name
      of the function.
    """

    # Only look at first top_n_frames of the stacktrace, below those are likely
    # to be noisy. Parse the stacktrace into the component dictionary.
    stack_list = callstack.GetTopNFrames(top_n_frames)
    self.__GenerateFileDict(stack_list)

  def __iter__(self):
    return iter(self.component_dict)
