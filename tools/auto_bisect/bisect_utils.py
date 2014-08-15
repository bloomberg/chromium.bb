# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Utility functions used by the bisect tool.

This includes functions related to checking out the depot and outputting
annotations for the Buildbot waterfall.
"""

import errno
import imp
import os
import shutil
import stat
import subprocess
import sys

DEFAULT_GCLIENT_CUSTOM_DEPS = {
    'src/data/page_cycler': 'https://chrome-internal.googlesource.com/'
                            'chrome/data/page_cycler/.git',
    'src/data/dom_perf': 'https://chrome-internal.googlesource.com/'
                         'chrome/data/dom_perf/.git',
    'src/data/mach_ports': 'https://chrome-internal.googlesource.com/'
                           'chrome/data/mach_ports/.git',
    'src/tools/perf/data': 'https://chrome-internal.googlesource.com/'
                           'chrome/tools/perf/data/.git',
    'src/third_party/adobe/flash/binaries/ppapi/linux':
        'https://chrome-internal.googlesource.com/'
        'chrome/deps/adobe/flash/binaries/ppapi/linux/.git',
    'src/third_party/adobe/flash/binaries/ppapi/linux_x64':
        'https://chrome-internal.googlesource.com/'
        'chrome/deps/adobe/flash/binaries/ppapi/linux_x64/.git',
    'src/third_party/adobe/flash/binaries/ppapi/mac':
        'https://chrome-internal.googlesource.com/'
        'chrome/deps/adobe/flash/binaries/ppapi/mac/.git',
    'src/third_party/adobe/flash/binaries/ppapi/mac_64':
        'https://chrome-internal.googlesource.com/'
        'chrome/deps/adobe/flash/binaries/ppapi/mac_64/.git',
    'src/third_party/adobe/flash/binaries/ppapi/win':
        'https://chrome-internal.googlesource.com/'
        'chrome/deps/adobe/flash/binaries/ppapi/win/.git',
    'src/third_party/adobe/flash/binaries/ppapi/win_x64':
        'https://chrome-internal.googlesource.com/'
        'chrome/deps/adobe/flash/binaries/ppapi/win_x64/.git',
    'src/chrome/tools/test/reference_build/chrome_win': None,
    'src/chrome/tools/test/reference_build/chrome_mac': None,
    'src/chrome/tools/test/reference_build/chrome_linux': None,
    'src/third_party/WebKit/LayoutTests': None,
    'src/tools/valgrind': None,
}

GCLIENT_SPEC_DATA = [
    {
        'name': 'src',
        'url': 'https://chromium.googlesource.com/chromium/src.git',
        'deps_file': '.DEPS.git',
        'managed': True,
        'custom_deps': {},
        'safesync_url': '',
    },
]
GCLIENT_SPEC_ANDROID = "\ntarget_os = ['android']"
GCLIENT_CUSTOM_DEPS_V8 = {'src/v8_bleeding_edge': 'git://github.com/v8/v8.git'}
FILE_DEPS_GIT = '.DEPS.git'
FILE_DEPS = 'DEPS'

REPO_SYNC_COMMAND = ('git checkout -f $(git rev-list --max-count=1 '
                     '--before=%d remotes/m/master)')

# Paths to CrOS-related files.
# WARNING(qyearsley, 2014-08-15): These haven't been tested recently.
CROS_SDK_PATH = os.path.join('..', 'cros', 'chromite', 'bin', 'cros_sdk')
CROS_TEST_KEY_PATH = os.path.join(
    '..', 'cros', 'chromite', 'ssh_keys', 'testing_rsa')
CROS_SCRIPT_KEY_PATH = os.path.join(
    '..', 'cros', 'src', 'scripts', 'mod_for_test_scripts', 'ssh_keys',
    'testing_rsa')

REPO_PARAMS = [
    'https://chrome-internal.googlesource.com/chromeos/manifest-internal/',
    '--repo-url',
    'https://git.chromium.org/external/repo.git'
]


def OutputAnnotationStepStart(name):
  """Outputs annotation to signal the start of a step to a trybot.

  Args:
    name: The name of the step.
  """
  print
  print '@@@SEED_STEP %s@@@' % name
  print '@@@STEP_CURSOR %s@@@' % name
  print '@@@STEP_STARTED@@@'
  print
  sys.stdout.flush()


def OutputAnnotationStepClosed():
  """Outputs annotation to signal the closing of a step to a trybot."""
  print
  print '@@@STEP_CLOSED@@@'
  print
  sys.stdout.flush()


def OutputAnnotationStepLink(label, url):
  """Outputs appropriate annotation to print a link.

  Args:
    label: The name to print.
    url: The url to print.
  """
  print
  print '@@@STEP_LINK@%s@%s@@@' % (label, url)
  print
  sys.stdout.flush()


def LoadExtraSrc(path_to_file):
  """Attempts to load an extra source file, and overrides global values.

  If the extra source file is loaded successfully, then it will use the new
  module to override some global values, such as gclient spec data.

  Args:
    path_to_file: File path.

  Returns:
    The loaded module object, or None if none was imported.
  """
  try:
    global GCLIENT_SPEC_DATA
    global GCLIENT_SPEC_ANDROID
    extra_src = imp.load_source('data', path_to_file)
    GCLIENT_SPEC_DATA = extra_src.GetGClientSpec()
    GCLIENT_SPEC_ANDROID = extra_src.GetGClientSpecExtraParams()
    return extra_src
  except ImportError:
    return None


def IsTelemetryCommand(command):
  """Attempts to discern whether or not a given command is running telemetry."""
  return ('tools/perf/run_' in command or 'tools\\perf\\run_' in command)


def _CreateAndChangeToSourceDirectory(working_directory):
  """Creates a directory 'bisect' as a subdirectory of |working_directory|.

  If successful, the current working directory will be changed to the new
  'bisect' directory.

  Args:
    working_directory: The directory to create the new 'bisect' directory in.

  Returns:
    True if the directory was successfully created (or already existed).
  """
  cwd = os.getcwd()
  os.chdir(working_directory)
  try:
    os.mkdir('bisect')
  except OSError, e:
    if e.errno != errno.EEXIST:  # EEXIST indicates that it already exists.
      os.chdir(cwd)
      return False
  os.chdir('bisect')
  return True


def _SubprocessCall(cmd, cwd=None):
  """Runs a subprocess with specified parameters.

  Args:
    params: A list of parameters to pass to gclient.
    cwd: Working directory to run from.

  Returns:
    The return code of the call.
  """
  if os.name == 'nt':
    # "HOME" isn't normally defined on windows, but is needed
    # for git to find the user's .netrc file.
    if not os.getenv('HOME'):
      os.environ['HOME'] = os.environ['USERPROFILE']
  shell = os.name == 'nt'
  return subprocess.call(cmd, shell=shell, cwd=cwd)


def RunGClient(params, cwd=None):
  """Runs gclient with the specified parameters.

  Args:
    params: A list of parameters to pass to gclient.
    cwd: Working directory to run from.

  Returns:
    The return code of the call.
  """
  cmd = ['gclient'] + params
  return _SubprocessCall(cmd, cwd=cwd)


def SetupCrosRepo():
  """Sets up CrOS repo for bisecting ChromeOS.

  Returns:
    True if successful, False otherwise.
  """
  cwd = os.getcwd()
  try:
    os.mkdir('cros')
  except OSError as e:
    if e.errno != errno.EEXIST:  # EEXIST means the directory already exists.
      return False
  os.chdir('cros')

  cmd = ['init', '-u'] + REPO_PARAMS

  passed = False

  if not _RunRepo(cmd):
    if not _RunRepo(['sync']):
      passed = True
  os.chdir(cwd)

  return passed


def _RunRepo(params):
  """Runs CrOS repo command with specified parameters.

  Args:
    params: A list of parameters to pass to gclient.

  Returns:
    The return code of the call (zero indicates success).
  """
  cmd = ['repo'] + params
  return _SubprocessCall(cmd)


def RunRepoSyncAtTimestamp(timestamp):
  """Syncs all git depots to the timestamp specified using repo forall.

  Args:
    params: Unix timestamp to sync to.

  Returns:
    The return code of the call.
  """
  cmd = ['forall', '-c', REPO_SYNC_COMMAND % timestamp]
  return _RunRepo(cmd)


def RunGClientAndCreateConfig(opts, custom_deps=None, cwd=None):
  """Runs gclient and creates a config containing both src and src-internal.

  Args:
    opts: The options parsed from the command line through parse_args().
    custom_deps: A dictionary of additional dependencies to add to .gclient.
    cwd: Working directory to run from.

  Returns:
    The return code of the call.
  """
  spec = GCLIENT_SPEC_DATA

  if custom_deps:
    for k, v in custom_deps.iteritems():
      spec[0]['custom_deps'][k] = v

  # Cannot have newlines in string on windows
  spec = 'solutions =' + str(spec)
  spec = ''.join([l for l in spec.splitlines()])

  if 'android' in opts.target_platform:
    spec += GCLIENT_SPEC_ANDROID

  return_code = RunGClient(
      ['config', '--spec=%s' % spec, '--git-deps'], cwd=cwd)
  return return_code


def IsDepsFileBlink():
  """Reads .DEPS.git and returns whether or not we're using blink.

  Returns:
    True if blink, false if webkit.
  """
  locals = {
      'Var': lambda _: locals["vars"][_],
      'From': lambda *args: None
  }
  execfile(FILE_DEPS_GIT, {}, locals)
  return 'blink.git' in locals['vars']['webkit_url']


def OnAccessError(func, path, _):
  """Error handler for shutil.rmtree.

  Source: http://goo.gl/DEYNCT

  If the error is due to an access error (read only file), it attempts to add
  write permissions, then retries.

  If the error is for another reason it re-raises the error.

  Args:
    func: The function that raised the error.
    path: The path name passed to func.
    _: Exception information from sys.exc_info(). Not used.
  """
  if not os.access(path, os.W_OK):
    os.chmod(path, stat.S_IWUSR)
    func(path)
  else:
    raise


def RemoveThirdPartyDirectory(dir_name):
  """Removes third_party directory from the source.

  At some point, some of the third_parties were causing issues to changes in
  the way they are synced. We remove such folder in order to avoid sync errors
  while bisecting.

  Returns:
    True on success, otherwise False.
  """
  path_to_dir = os.path.join(os.getcwd(), 'third_party', dir_name)
  try:
    if os.path.exists(path_to_dir):
      shutil.rmtree(path_to_dir, onerror=OnAccessError)
  except OSError, e:
    print 'Error #%d while running shutil.rmtree(%s): %s' % (
        e.errno, path_to_dir, str(e))
    if e.errno != errno.ENOENT:
      return False
  return True


def _CleanupPreviousGitRuns():
  """Cleans up any leftover index.lock files after running git."""
  # If a previous run of git crashed, or bot was reset, etc., then we might
  # end up with leftover index.lock files.
  for path, _, files in os.walk(os.getcwd()):
    for cur_file in files:
      if cur_file.endswith('index.lock'):
        path_to_file = os.path.join(path, cur_file)
        os.remove(path_to_file)


def RunGClientAndSync(cwd=None):
  """Runs gclient and does a normal sync.

  Args:
    cwd: Working directory to run from.

  Returns:
    The return code of the call.
  """
  params = ['sync', '--verbose', '--nohooks', '--reset', '--force']
  return RunGClient(params, cwd=cwd)


def SetupGitDepot(opts, custom_deps):
  """Sets up the depot for the bisection.

  The depot will be located in a subdirectory called 'bisect'.

  Args:
    opts: The options parsed from the command line through parse_args().
    custom_deps: A dictionary of additional dependencies to add to .gclient.

  Returns:
    True if gclient successfully created the config file and did a sync, False
    otherwise.
  """
  name = 'Setting up Bisection Depot'

  if opts.output_buildbot_annotations:
    OutputAnnotationStepStart(name)

  passed = False

  if not RunGClientAndCreateConfig(opts, custom_deps):
    passed_deps_check = True
    if os.path.isfile(os.path.join('src', FILE_DEPS_GIT)):
      cwd = os.getcwd()
      os.chdir('src')
      if not IsDepsFileBlink():
        passed_deps_check = RemoveThirdPartyDirectory('Webkit')
      else:
        passed_deps_check = True
      if passed_deps_check:
        passed_deps_check = RemoveThirdPartyDirectory('libjingle')
      if passed_deps_check:
        passed_deps_check = RemoveThirdPartyDirectory('skia')
      os.chdir(cwd)

    if passed_deps_check:
      _CleanupPreviousGitRuns()

      RunGClient(['revert'])
      if not RunGClientAndSync():
        passed = True

  if opts.output_buildbot_annotations:
    print
    OutputAnnotationStepClosed()

  return passed


def CheckIfBisectDepotExists(opts):
  """Checks if the bisect directory already exists.

  Args:
    opts: The options parsed from the command line through parse_args().

  Returns:
    Returns True if it exists.
  """
  path_to_dir = os.path.join(opts.working_directory, 'bisect', 'src')
  return os.path.exists(path_to_dir)


def CheckRunGit(command, cwd=None):
  """Run a git subcommand, returning its output and return code. Asserts if
  the return code of the call is non-zero.

  Args:
    command: A list containing the args to git.

  Returns:
    A tuple of the output and return code.
  """
  (output, return_code) = RunGit(command, cwd=cwd)

  assert not return_code, 'An error occurred while running'\
                          ' "git %s"' % ' '.join(command)
  return output


def RunGit(command, cwd=None):
  """Run a git subcommand, returning its output and return code.

  Args:
    command: A list containing the args to git.
    cwd: A directory to change to while running the git command (optional).

  Returns:
    A tuple of the output and return code.
  """
  command = ['git'] + command

  return RunProcessAndRetrieveOutput(command, cwd=cwd)


def CreateBisectDirectoryAndSetupDepot(opts, custom_deps):
  """Sets up a subdirectory 'bisect' and then retrieves a copy of the depot
  there using gclient.

  Args:
    opts: The options parsed from the command line through parse_args().
    custom_deps: A dictionary of additional dependencies to add to .gclient.
  """
  if not _CreateAndChangeToSourceDirectory(opts.working_directory):
    raise RuntimeError('Could not create bisect directory.')

  if not SetupGitDepot(opts, custom_deps):
    raise RuntimeError('Failed to grab source.')


def RunProcess(command):
  """Runs an arbitrary command.

  If output from the call is needed, use RunProcessAndRetrieveOutput instead.

  Args:
    command: A list containing the command and args to execute.

  Returns:
    The return code of the call.
  """
  # On Windows, use shell=True to get PATH interpretation.
  shell = IsWindowsHost()
  return subprocess.call(command, shell=shell)


def RunProcessAndRetrieveOutput(command, cwd=None):
  """Runs an arbitrary command, returning its output and return code.

  Since output is collected via communicate(), there will be no output until
  the call terminates. If you need output while the program runs (ie. so
  that the buildbot doesn't terminate the script), consider RunProcess().

  Args:
    command: A list containing the command and args to execute.
    cwd: A directory to change to while running the command. The command can be
        relative to this directory. If this is None, the command will be run in
        the current directory.

  Returns:
    A tuple of the output and return code.
  """
  if cwd:
    original_cwd = os.getcwd()
    os.chdir(cwd)

  # On Windows, use shell=True to get PATH interpretation.
  shell = IsWindowsHost()
  proc = subprocess.Popen(command, shell=shell, stdout=subprocess.PIPE)
  (output, _) = proc.communicate()

  if cwd:
    os.chdir(original_cwd)

  return (output, proc.returncode)


def IsStringInt(string_to_check):
  """Checks whether or not the given string can be converted to a integer.

  Args:
    string_to_check: Input string to check if it can be converted to an int.

  Returns:
    True if the string can be converted to an int.
  """
  try:
    int(string_to_check)
    return True
  except ValueError:
    return False


def IsStringFloat(string_to_check):
  """Checks whether or not the given string can be converted to a floating
  point number.

  Args:
    string_to_check: Input string to check if it can be converted to a float.

  Returns:
    True if the string can be converted to a float.
  """
  try:
    float(string_to_check)
    return True
  except ValueError:
    return False


def IsWindowsHost():
  """Checks whether or not the script is running on Windows.

  Returns:
    True if running on Windows.
  """
  return sys.platform == 'cygwin' or sys.platform.startswith('win')


def Is64BitWindows():
  """Returns whether or not Windows is a 64-bit version.

  Returns:
    True if Windows is 64-bit, False if 32-bit.
  """
  platform = os.environ['PROCESSOR_ARCHITECTURE']
  try:
    platform = os.environ['PROCESSOR_ARCHITEW6432']
  except KeyError:
    # Must not be running in WoW64, so PROCESSOR_ARCHITECTURE is correct
    pass

  return platform in ['AMD64', 'I64']


def IsLinuxHost():
  """Checks whether or not the script is running on Linux.

  Returns:
    True if running on Linux.
  """
  return sys.platform.startswith('linux')


def IsMacHost():
  """Checks whether or not the script is running on Mac.

  Returns:
    True if running on Mac.
  """
  return sys.platform.startswith('darwin')
