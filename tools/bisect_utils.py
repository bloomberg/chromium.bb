# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Set of operations/utilities related to checking out the depot, and
outputting annotations on the buildbot waterfall. These are intended to be
used by the bisection scripts."""

import errno
import os
import subprocess


GCLIENT_SPEC = """
solutions = [
  { "name"        : "src",
    "url"         : "https://chromium.googlesource.com/chromium/src.git",
    "deps_file"   : ".DEPS.git",
    "managed"     : True,
    "custom_deps" : {
      "src/data/page_cycler": "https://chrome-internal.googlesource.com/" +
                              "chrome/data/page_cycler/.git",
      "src/tools/perf/data": "https://chrome-internal.googlesource.com/" +
                             "chrome/tools/perf/data/.git",
    },
    "safesync_url": "",
  },
]
"""


def OutputAnnotationStepStart(name):
  """Outputs appropriate annotation to signal the start of a step to
  a trybot.

  Args:
    name: The name of the step.
  """
  print '@@@SEED_STEP %s@@@' % name
  print '@@@STEP_CURSOR %s@@@' % name
  print '@@@STEP_STARTED@@@'


def OutputAnnotationStepClosed():
  """Outputs appropriate annotation to signal the closing of a step to
  a trybot."""
  print '@@@STEP_CLOSED@@@'


def CreateAndChangeToSourceDirectory(working_directory):
  """Creates a directory 'bisect' as a subdirectory of 'working_directory'.  If
  the function is successful, the current working directory will change to that
  of the new 'bisect' directory.

  Returns:
    True if the directory was successfully created (or already existed).
  """
  cwd = os.getcwd()
  os.chdir(working_directory)
  try:
    os.mkdir('bisect')
  except OSError, e:
    if e.errno != errno.EEXIST:
      return False
  os.chdir('bisect')
  return True


def RunGClient(params):
  """Runs gclient with the specified parameters.

  Args:
    params: A list of parameters to pass to gclient.

  Returns:
    The return code of the call.
  """
  cmd = ['gclient'] + params
  return subprocess.call(cmd)


def RunGClientAndCreateConfig():
  """Runs gclient and creates a config containing both src and src-internal.

  Returns:
    The return code of the call.
  """
  return_code = RunGClient(
      ['config', '--spec=%s' % GCLIENT_SPEC, '--git-deps'])
  return return_code


def RunGClientAndSync():
  """Runs gclient and does a normal sync.

  Returns:
    The return code of the call.
  """
  return RunGClient(['sync'])


def SetupGitDepot(output_buildbot_annotations):
  """Sets up the depot for the bisection. The depot will be located in a
  subdirectory called 'bisect'.

  Returns:
    True if gclient successfully created the config file and did a sync, False
    otherwise.
  """
  name = 'Setting up Bisection Depot'

  if output_buildbot_annotations:
    OutputAnnotationStepStart(name)

  passed = False

  if not RunGClientAndCreateConfig():
    if not RunGClientAndSync():
      passed = True

  if output_buildbot_annotations:
    print
    OutputAnnotationStepClosed()

  return passed


def CreateBisectDirectoryAndSetupDepot(opts):
  """Sets up a subdirectory 'bisect' and then retrieves a copy of the depot
  there using gclient.

  Args:
    opts: The options parsed from the command line through parse_args().

  Returns:
    Returns 0 on success, otherwise 1.
  """
  if not CreateAndChangeToSourceDirectory(opts.working_directory):
    print 'Error: Could not create bisect directory.'
    print
    return 1

  if not SetupGitDepot(opts.output_buildbot_annotations):
    print 'Error: Failed to grab source.'
    print
    return 1

  return 0
