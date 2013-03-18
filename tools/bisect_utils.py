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
      "src/v8_bleeding_edge": "git://github.com/v8/v8.git",
    },
    "safesync_url": "",
  },
]
"""
GCLIENT_SPEC = ''.join([l for l in GCLIENT_SPEC.splitlines()])


def OutputAnnotationStepStart(name):
  """Outputs appropriate annotation to signal the start of a step to
  a trybot.

  Args:
    name: The name of the step.
  """
  print
  print '@@@SEED_STEP %s@@@' % name
  print '@@@STEP_CURSOR %s@@@' % name
  print '@@@STEP_STARTED@@@'
  print


def OutputAnnotationStepClosed():
  """Outputs appropriate annotation to signal the closing of a step to
  a trybot."""
  print
  print '@@@STEP_CLOSED@@@'
  print


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
  if os.name == 'nt':
    # "HOME" isn't normally defined on windows, but is needed
    # for git to find the user's .netrc file.
    if not os.getenv('HOME'):
      os.environ['HOME'] = os.environ['USERPROFILE']

  shell = os.name == 'nt'
  cmd = ['gclient'] + params
  return subprocess.call(cmd, shell=shell)


def RunGClientAndCreateConfig():
  """Runs gclient and creates a config containing both src and src-internal.

  Returns:
    The return code of the call.
  """
  return_code = RunGClient(
      ['config', '--spec=%s' % GCLIENT_SPEC, '--git-deps'])
  return return_code


def RunGClientAndSync(reset):
  """Runs gclient and does a normal sync.

  Args:
    reset: Whether to reset any changes to the depot.

  Returns:
    The return code of the call.
  """
  params = ['sync', '--verbose']
  if reset:
    params.extend(['--reset', '--force', '--delete_unversioned_trees'])
  return RunGClient(params)


def SetupGitDepot(output_buildbot_annotations, reset):
  """Sets up the depot for the bisection. The depot will be located in a
  subdirectory called 'bisect'.

  Args:
    reset: Whether to reset any changes to the depot.

  Returns:
    True if gclient successfully created the config file and did a sync, False
    otherwise.
  """
  name = 'Setting up Bisection Depot'

  if output_buildbot_annotations:
    OutputAnnotationStepStart(name)

  passed = False

  if not RunGClientAndCreateConfig():
    if not RunGClientAndSync(reset):
      passed = True

  if output_buildbot_annotations:
    print
    OutputAnnotationStepClosed()

  return passed


def CreateBisectDirectoryAndSetupDepot(opts, reset=False):
  """Sets up a subdirectory 'bisect' and then retrieves a copy of the depot
  there using gclient.

  Args:
    opts: The options parsed from the command line through parse_args().
    reset: Whether to reset any changes to the depot.

  Returns:
    Returns 0 on success, otherwise 1.
  """
  if not CreateAndChangeToSourceDirectory(opts.working_directory):
    print 'Error: Could not create bisect directory.'
    print
    return 1

  if not SetupGitDepot(opts.output_buildbot_annotations, reset):
    print 'Error: Failed to grab source.'
    print
    return 1

  return 0
