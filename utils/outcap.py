# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Tools for capturing program output at a low level.

Mostly useful for capturing stdout/stderr as directly assigning to those
variables won't work everywhere.
"""

from __future__ import print_function

import os
import re
import sys
import tempfile


class _FdCapturer(object):
  """Helper class to capture output at the file descriptor level.

  This is meant to be used with sys.stdout or sys.stderr. By capturing
  file descriptors, this will also intercept subprocess output, which
  reassigning sys.stdout or sys.stderr will not do.

  Output will only be captured, it will no longer be printed while
  the capturer is active.
  """

  def __init__(self, source, output=None):
    """Construct the _FdCapturer object.

    Does not start capturing until Start() is called.

    Args:
      source: A file object to capture. Typically sys.stdout or
        sys.stderr, but will work with anything that implements flush()
        and fileno().
      output: A file name where the captured output is to be stored. If None,
        then the output will be stored to a temporary file.
    """
    self._source = source
    self._captured = ''
    self._saved_fd = None
    self._tempfile = None
    self._capturefile = None
    self._capturefile_reader = None
    self._capturefile_name = output

  def _SafeCreateTempfile(self, tempfile_obj):
    """Ensure that the tempfile is created safely.

    (1) Stash away a reference to the tempfile.
    (2) Unlink the file from the filesystem.

    (2) ensures that if we crash, the file gets deleted. (1) ensures that while
    we are running, we hold a reference to the file so the system does not close
    the file.

    Args:
      tempfile_obj: A tempfile object.
    """
    self._tempfile = tempfile_obj
    os.unlink(tempfile_obj.name)

  def Start(self):
    """Begin capturing output."""
    if self._capturefile_name is None:
      tempfile_obj = tempfile.NamedTemporaryFile(delete=False)
      self._capturefile = tempfile_obj.file
      self._capturefile_name = tempfile_obj.name
      self._capturefile_reader = open(self._capturefile_name)
      self._SafeCreateTempfile(tempfile_obj)
    else:
      # Open file passed in for writing. Set buffering=1 for line level
      # buffering.
      self._capturefile = open(self._capturefile_name, 'w', buffering=1)
      self._capturefile_reader = open(self._capturefile_name)
    # Save the original fd so we can revert in Stop().
    self._saved_fd = os.dup(self._source.fileno())
    os.dup2(self._capturefile.fileno(), self._source.fileno())

  def Stop(self):
    """Stop capturing output."""
    self.GetCaptured()
    if self._saved_fd is not None:
      os.dup2(self._saved_fd, self._source.fileno())
      os.close(self._saved_fd)
      self._saved_fd = None
    # If capturefile and capturefile_reader exist, close them as they were
    # opened in self.Start().
    if self._capturefile_reader is not None:
      self._capturefile_reader.close()
      self._capturefile_reader = None
    if self._capturefile is not None:
      self._capturefile.close()
      self._capturefile = None

  def GetCaptured(self):
    """Return all output captured up to this point.

    Can be used while capturing or after Stop() has been called.
    """
    self._source.flush()
    if self._capturefile_reader is not None:
      self._captured += self._capturefile_reader.read()
    return self._captured

  def ClearCaptured(self):
    """Erase all captured output."""
    self.GetCaptured()
    self._captured = ''


class OutputCapturer(object):
  """Class for capturing stdout/stderr output.

  Class is designed as a 'ContextManager'.

  Examples:
    with cros_build_lib.OutputCapturer() as output:
      # Capturing of stdout/stderr automatically starts now.
      # Do stuff that sends output to stdout/stderr.
      # Capturing automatically stops at end of 'with' block.

    # stdout/stderr can be retrieved from the OutputCapturer object:
    stdout = output.GetStdoutLines() # Or other access methods

    # Some Assert methods are only valid if capturing was used in test.
    self.AssertOutputContainsError() # Or other related methods

    # OutputCapturer can also be used to capture output to specified files.
    with self.OutputCapturer(stdout_path='/tmp/stdout.txt') as output:
      # Do stuff.
      # stdout will be captured to /tmp/stdout.txt.
  """

  OPER_MSG_SPLIT_RE = re.compile(r'^\033\[1;.*?\033\[0m$|^[^\n]*$',
                                 re.DOTALL | re.MULTILINE)

  __slots__ = ['_stdout_capturer', '_stderr_capturer', '_quiet_fail']

  def __init__(self, stdout_path=None, stderr_path=None, quiet_fail=False):
    """Initalize OutputCapturer with capture files.

    If OutputCapturer is initialized with filenames to capture stdout and stderr
    to, then those files are used. Otherwise, temporary files are created.

    Args:
      stdout_path: File to capture stdout to. If None, a temporary file is used.
      stderr_path: File to capture stderr to. If None, a temporary file is used.
      quiet_fail: If True fail quietly without printing the captured stdout and
        stderr.
    """
    self._stdout_capturer = _FdCapturer(sys.stdout, output=stdout_path)
    self._stderr_capturer = _FdCapturer(sys.stderr, output=stderr_path)
    self._quiet_fail = quiet_fail

  def __enter__(self):
    # This method is called with entering 'with' block.
    self.StartCapturing()
    return self

  def __exit__(self, exc_type, exc_val, exc_tb):
    # This method is called when exiting 'with' block.
    self.StopCapturing()

    if exc_type and not self._quiet_fail:
      print('Exception during output capturing: %r' % (exc_val,))
      stdout = self.GetStdout()
      if stdout:
        print('Captured stdout was:\n%s' % stdout)
      else:
        print('No captured stdout')
      stderr = self.GetStderr()
      if stderr:
        print('Captured stderr was:\n%s' % stderr)
      else:
        print('No captured stderr')

  def StartCapturing(self):
    """Begin capturing stdout and stderr."""
    self._stdout_capturer.Start()
    self._stderr_capturer.Start()

  def StopCapturing(self):
    """Stop capturing stdout and stderr."""
    self._stdout_capturer.Stop()
    self._stderr_capturer.Stop()

  def ClearCaptured(self):
    """Clear any captured stdout/stderr content."""
    self._stdout_capturer.ClearCaptured()
    self._stderr_capturer.ClearCaptured()

  def GetStdout(self):
    """Return captured stdout so far."""
    return self._stdout_capturer.GetCaptured()

  def GetStderr(self):
    """Return captured stderr so far."""
    return self._stderr_capturer.GetCaptured()

  def _GetOutputLines(self, output, include_empties):
    """Split |output| into lines, optionally |include_empties|.

    Return array of lines.
    """

    lines = self.OPER_MSG_SPLIT_RE.findall(output)
    if not include_empties:
      lines = [ln for ln in lines if ln]

    return lines

  def GetStdoutLines(self, include_empties=True):
    """Return captured stdout so far as array of lines.

    If |include_empties| is false filter out all empty lines.
    """
    return self._GetOutputLines(self.GetStdout(), include_empties)

  def GetStderrLines(self, include_empties=True):
    """Return captured stderr so far as array of lines.

    If |include_empties| is false filter out all empty lines.
    """
    return self._GetOutputLines(self.GetStderr(), include_empties)
