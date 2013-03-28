#!/usr/bin/env python
# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Performance Test Bisect Tool

This script bisects a series of changelists using binary search. It starts at
a bad revision where a performance metric has regressed, and asks for a last
known-good revision. It will then binary search across this revision range by
syncing, building, and running a performance test. If the change is
suspected to occur as a result of WebKit/V8 changes, the script will
further bisect changes to those depots and attempt to narrow down the revision
range.


An example usage (using svn cl's):

./tools/bisect-perf-regression.py -c\
"out/Release/performance_ui_tests --gtest_filter=ShutdownTest.SimpleUserQuit"\
-g 168222 -b 168232 -m shutdown/simple-user-quit

Be aware that if you're using the git workflow and specify an svn revision,
the script will attempt to find the git SHA1 where svn changes up to that
revision were merged in.


An example usage (using git hashes):

./tools/bisect-perf-regression.py -c\
"out/Release/performance_ui_tests --gtest_filter=ShutdownTest.SimpleUserQuit"\
-g 1f6e67861535121c5c819c16a666f2436c207e7b\
-b b732f23b4f81c382db0b23b9035f3dadc7d925bb\
-m shutdown/simple-user-quit

"""

import errno
import imp
import math
import optparse
import os
import re
import shlex
import shutil
import subprocess
import sys
import threading
import time

import bisect_utils


# The additional repositories that might need to be bisected.
# If the repository has any dependant repositories (such as skia/src needs
# skia/include and skia/gyp to be updated), specify them in the 'depends'
# so that they're synced appropriately.
# Format is:
# src: path to the working directory.
# recurse: True if this repositry will get bisected.
# depends: A list of other repositories that are actually part of the same
#   repository in svn.
# svn: Needed for git workflow to resolve hashes to svn revisions.
DEPOT_DEPS_NAME = {
  'webkit' : {
    "src" : "src/third_party/WebKit",
    "recurse" : True,
    "depends" : None
  },
  'v8' : {
    "src" : "src/v8",
    "recurse" : True,
    "depends" : None,
    "build_with": 'v8_bleeding_edge'
  },
  'v8_bleeding_edge' : {
    "src" : "src/v8_bleeding_edge",
    "recurse" : False,
    "depends" : None,
    "svn": "https://v8.googlecode.com/svn/branches/bleeding_edge"
  },
  'skia/src' : {
    "src" : "src/third_party/skia/src",
    "recurse" : True,
    "svn" : "http://skia.googlecode.com/svn/trunk/src",
    "depends" : ['skia/include', 'skia/gyp']
  },
  'skia/include' : {
    "src" : "src/third_party/skia/include",
    "recurse" : False,
    "svn" : "http://skia.googlecode.com/svn/trunk/include",
    "depends" : None
  },
  'skia/gyp' : {
    "src" : "src/third_party/skia/gyp",
    "recurse" : False,
    "svn" : "http://skia.googlecode.com/svn/trunk/gyp",
    "depends" : None
  }
}

DEPOT_NAMES = DEPOT_DEPS_NAME.keys()

FILE_DEPS_GIT = '.DEPS.git'


def CalculateTruncatedMean(data_set, truncate_percent):
  """Calculates the truncated mean of a set of values.

  Args:
    data_set: Set of values to use in calculation.
    truncate_percent: The % from the upper/lower portions of the data set to
        discard, expressed as a value in [0, 1].

  Returns:
    The truncated mean as a float.
  """
  if len(data_set) > 2:
    data_set = sorted(data_set)

    discard_num_float = len(data_set) * truncate_percent
    discard_num_int = int(math.floor(discard_num_float))
    kept_weight = len(data_set) - discard_num_float * 2

    data_set = data_set[discard_num_int:len(data_set)-discard_num_int]

    weight_left = 1.0 - (discard_num_float - discard_num_int)

    if weight_left < 1:
      # If the % to discard leaves a fractional portion, need to weight those
      # values.
      unweighted_vals = data_set[1:len(data_set)-1]
      weighted_vals = [data_set[0], data_set[len(data_set)-1]]
      weighted_vals = [w * weight_left for w in weighted_vals]
      data_set = weighted_vals + unweighted_vals
  else:
    kept_weight = len(data_set)

  truncated_mean = reduce(lambda x, y: float(x) + float(y),
                          data_set) / kept_weight

  return truncated_mean


def CalculateStandardDeviation(v):
  mean = CalculateTruncatedMean(v, 0.0)

  variances = [float(x) - mean for x in v]
  variances = [x * x for x in variances]
  variance = reduce(lambda x, y: float(x) + float(y), variances) / (len(v) - 1)
  std_dev = math.sqrt(variance)

  return std_dev


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


def IsWindows():
  """Checks whether or not the script is running on Windows.

  Returns:
    True if running on Windows.
  """
  return os.name == 'nt'


def RunProcess(command, print_output=False):
  """Run an arbitrary command, returning its output and return code.

  Args:
    command: A list containing the command and args to execute.
    print_output: Optional parameter to write output to stdout as it's
        being collected.

  Returns:
    A tuple of the output and return code.
  """
  if print_output:
    print 'Running: [%s]' % ' '.join(command)

  # On Windows, use shell=True to get PATH interpretation.
  shell = IsWindows()
  proc = subprocess.Popen(command,
                          shell=shell,
                          stdout=subprocess.PIPE,
                          stderr=subprocess.PIPE,
                          bufsize=0)

  out = ['']
  def ReadOutputWhileProcessRuns(stdout, print_output, out):
    while True:
      line = stdout.readline()
      out[0] += line
      if line == '':
        break
      if print_output:
        sys.stdout.write(line)

  thread = threading.Thread(target=ReadOutputWhileProcessRuns,
                            args=(proc.stdout, print_output, out))
  thread.start()
  proc.wait()
  thread.join()

  return (out[0], proc.returncode)


def RunGit(command):
  """Run a git subcommand, returning its output and return code.

  Args:
    command: A list containing the args to git.

  Returns:
    A tuple of the output and return code.
  """
  command = ['git'] + command

  return RunProcess(command)


def BuildWithMake(threads, targets, print_output):
  cmd = ['make', 'BUILDTYPE=Release', '-j%d' % threads] + targets

  (output, return_code) = RunProcess(cmd, print_output)

  return not return_code


def BuildWithNinja(threads, targets, print_output):
  cmd = ['ninja', '-C', os.path.join('out', 'Release'),
      '-j%d' % threads] + targets

  (output, return_code) = RunProcess(cmd, print_output)

  return not return_code


def BuildWithVisualStudio(targets, print_output):
  path_to_devenv = os.path.abspath(
      os.path.join(os.environ['VS100COMNTOOLS'], '..', 'IDE', 'devenv.com'))
  path_to_sln = os.path.join(os.getcwd(), 'chrome', 'chrome.sln')
  cmd = [path_to_devenv, '/build', 'Release', path_to_sln]

  for t in targets:
    cmd.extend(['/Project', t])

  (output, return_code) = RunProcess(cmd, print_output)

  return not return_code


class SourceControl(object):
  """SourceControl is an abstraction over the underlying source control
  system used for chromium. For now only git is supported, but in the
  future, the svn workflow could be added as well."""
  def __init__(self):
    super(SourceControl, self).__init__()

  def SyncToRevisionWithGClient(self, revision):
    """Uses gclient to sync to the specified revision.

    ie. gclient sync --revision <revision>

    Args:
      revision: The git SHA1 or svn CL (depending on workflow).

    Returns:
      The return code of the call.
    """
    return bisect_utils.RunGClient(['sync', '--revision',
        revision, '--verbose'])


class GitSourceControl(SourceControl):
  """GitSourceControl is used to query the underlying source control. """
  def __init__(self):
    super(GitSourceControl, self).__init__()

  def IsGit(self):
    return True

  def GetRevisionList(self, revision_range_end, revision_range_start):
    """Retrieves a list of revisions between |revision_range_start| and
    |revision_range_end|.

    Args:
      revision_range_end: The SHA1 for the end of the range.
      revision_range_start: The SHA1 for the beginning of the range.

    Returns:
      A list of the revisions between |revision_range_start| and
      |revision_range_end| (inclusive).
    """
    revision_range = '%s..%s' % (revision_range_start, revision_range_end)
    cmd = ['log', '--format=%H', '-10000', '--first-parent', revision_range]
    (log_output, return_code) = RunGit(cmd)

    assert not return_code, 'An error occurred while running'\
                            ' "git %s"' % ' '.join(cmd)

    revision_hash_list = log_output.split()
    revision_hash_list.append(revision_range_start)

    return revision_hash_list

  def SyncToRevision(self, revision, use_gclient=True):
    """Syncs to the specified revision.

    Args:
      revision: The revision to sync to.
      use_gclient: Specifies whether or not we should sync using gclient or
        just use source control directly.

    Returns:
      True if successful.
    """

    if use_gclient:
      results = self.SyncToRevisionWithGClient(revision)
    else:
      results = RunGit(['checkout', revision])[1]

    return not results

  def ResolveToRevision(self, revision_to_check, depot, search):
    """If an SVN revision is supplied, try to resolve it to a git SHA1.

    Args:
      revision_to_check: The user supplied revision string that may need to be
        resolved to a git SHA1.
      depot: The depot the revision_to_check is from.
      search: The number of changelists to try if the first fails to resolve
        to a git hash. If the value is negative, the function will search
        backwards chronologically, otherwise it will search forward.

    Returns:
      A string containing a git SHA1 hash, otherwise None.
    """
    if not IsStringInt(revision_to_check):
      return revision_to_check

    depot_svn = 'svn://svn.chromium.org/chrome/trunk/src'

    if depot != 'src':
      depot_svn = DEPOT_DEPS_NAME[depot]['svn']

    svn_revision = int(revision_to_check)
    git_revision = None

    if search > 0:
      search_range = xrange(svn_revision, svn_revision + search, 1)
    else:
      search_range = xrange(svn_revision, svn_revision + search, -1)

    for i in search_range:
      svn_pattern = 'git-svn-id: %s@%d' % (depot_svn, i)
      cmd = ['log', '--format=%H', '-1', '--grep', svn_pattern, 'origin/master']

      (log_output, return_code) = RunGit(cmd)

      assert not return_code, 'An error occurred while running'\
                              ' "git %s"' % ' '.join(cmd)

      if not return_code:
        log_output = log_output.strip()

        if log_output:
          git_revision = log_output

          break

    return git_revision

  def IsInProperBranch(self):
    """Confirms they're in the master branch for performing the bisection.
    This is needed or gclient will fail to sync properly.

    Returns:
      True if the current branch on src is 'master'
    """
    cmd = ['rev-parse', '--abbrev-ref', 'HEAD']
    (log_output, return_code) = RunGit(cmd)

    assert not return_code, 'An error occurred while running'\
                            ' "git %s"' % ' '.join(cmd)

    log_output = log_output.strip()

    return log_output == "master"

  def SVNFindRev(self, revision):
    """Maps directly to the 'git svn find-rev' command.

    Args:
      revision: The git SHA1 to use.

    Returns:
      An integer changelist #, otherwise None.
    """

    cmd = ['svn', 'find-rev', revision]

    (output, return_code) = RunGit(cmd)

    assert not return_code, 'An error occurred while running'\
                            ' "git %s"' % ' '.join(cmd)

    svn_revision = output.strip()

    if IsStringInt(svn_revision):
      return int(svn_revision)

    return None

  def QueryRevisionInfo(self, revision):
    """Gathers information on a particular revision, such as author's name,
    email, subject, and date.

    Args:
      revision: Revision you want to gather information on.
    Returns:
      A dict in the following format:
      {
        'author': %s,
        'email': %s,
        'date': %s,
        'subject': %s,
      }
    """
    commit_info = {}

    formats = ['%cN', '%cE', '%s', '%cD']
    targets = ['author', 'email', 'subject', 'date']

    for i in xrange(len(formats)):
      cmd = ['log', '--format=%s' % formats[i], '-1', revision]
      (output, return_code) = RunGit(cmd)
      commit_info[targets[i]] = output.rstrip()

      assert not return_code, 'An error occurred while running'\
                              ' "git %s"' % ' '.join(cmd)

    return commit_info


class BisectPerformanceMetrics(object):
  """BisectPerformanceMetrics performs a bisection against a list of range
  of revisions to narrow down where performance regressions may have
  occurred."""

  def __init__(self, source_control, opts):
    super(BisectPerformanceMetrics, self).__init__()

    self.opts = opts
    self.source_control = source_control
    self.src_cwd = os.getcwd()
    self.depot_cwd = {}
    self.cleanup_commands = []

    for d in DEPOT_NAMES:
      # The working directory of each depot is just the path to the depot, but
      # since we're already in 'src', we can skip that part.

      self.depot_cwd[d] = self.src_cwd + DEPOT_DEPS_NAME[d]['src'][3:]

  def PerformCleanup(self):
    """Performs cleanup when script is finished."""
    os.chdir(self.src_cwd)
    for c in self.cleanup_commands:
      if c[0] == 'mv':
        shutil.move(c[1], c[2])
      else:
        assert False, 'Invalid cleanup command.'

  def GetRevisionList(self, bad_revision, good_revision):
    """Retrieves a list of all the commits between the bad revision and
    last known good revision."""

    revision_work_list = self.source_control.GetRevisionList(bad_revision,
                                                             good_revision)

    return revision_work_list

  def Get3rdPartyRevisionsFromCurrentRevision(self):
    """Parses the DEPS file to determine WebKit/v8/etc... versions.

    Returns:
      A dict in the format {depot:revision} if successful, otherwise None.
    """

    cwd = os.getcwd()
    os.chdir(self.src_cwd)

    locals = {'Var': lambda _: locals["vars"][_],
              'From': lambda *args: None}
    execfile(FILE_DEPS_GIT, {}, locals)

    os.chdir(cwd)

    results = {}

    rxp = re.compile(".git@(?P<revision>[a-fA-F0-9]+)")

    for d in DEPOT_NAMES:
      if DEPOT_DEPS_NAME[d]['recurse']:
        if locals['deps'].has_key(DEPOT_DEPS_NAME[d]['src']):
          re_results = rxp.search(locals['deps'][DEPOT_DEPS_NAME[d]['src']])

          if re_results:
            results[d] = re_results.group('revision')
          else:
            return None
        else:
          return None

    return results

  def BuildCurrentRevision(self):
    """Builds chrome and performance_ui_tests on the current revision.

    Returns:
      True if the build was successful.
    """
    if self.opts.debug_ignore_build:
      return True

    targets = ['chrome', 'performance_ui_tests']
    threads = 16
    if self.opts.use_goma:
      threads = 300

    cwd = os.getcwd()
    os.chdir(self.src_cwd)

    if self.opts.build_preference == 'make':
      build_success = BuildWithMake(threads, targets,
          self.opts.output_buildbot_annotations)
    elif self.opts.build_preference == 'ninja':
      if IsWindows():
        targets = [t + '.exe' for t in targets]
      build_success = BuildWithNinja(threads, targets,
          self.opts.output_buildbot_annotations)
    elif self.opts.build_preference == 'msvs':
      assert IsWindows(), 'msvs is only supported on Windows.'
      build_success = BuildWithVisualStudio(targets,
          self.opts.output_buildbot_annotations)
    else:
      assert False, 'No build system defined.'

    os.chdir(cwd)

    return build_success

  def RunGClientHooks(self):
    """Runs gclient with runhooks command.

    Returns:
      True if gclient reports no errors.
    """

    if self.opts.debug_ignore_build:
      return True

    return not bisect_utils.RunGClient(['runhooks'])

  def ParseMetricValuesFromOutput(self, metric, text):
    """Parses output from performance_ui_tests and retrieves the results for
    a given metric.

    Args:
      metric: The metric as a list of [<trace>, <value>] strings.
      text: The text to parse the metric values from.

    Returns:
      A list of floating point numbers found.
    """
    # Format is: RESULT <graph>: <trace>= <value> <units>
    metric_formatted = 'RESULT %s: %s=' % (metric[0], metric[1])

    text_lines = text.split('\n')
    values_list = []

    for current_line in text_lines:
      # Parse the output from the performance test for the metric we're
      # interested in.
      metric_re = metric_formatted +\
                  "(\s)*(?P<values>[0-9]+(\.[0-9]*)?)"
      metric_re = re.compile(metric_re)
      regex_results = metric_re.search(current_line)

      if not regex_results is None:
        values_list += [regex_results.group('values')]
      else:
        metric_re = metric_formatted +\
                    "(\s)*\[(\s)*(?P<values>[0-9,.]+)\]"
        metric_re = re.compile(metric_re)
        regex_results = metric_re.search(current_line)

        if not regex_results is None:
          metric_values = regex_results.group('values')

          values_list += metric_values.split(',')

    values_list = [float(v) for v in values_list if IsStringFloat(v)]

    # If the metric is times/t, we need to sum the timings in order to get
    # similar regression results as the try-bots.

    if metric == ['times', 't']:
      if values_list:
        values_list = [reduce(lambda x, y: float(x) + float(y), values_list)]

    return values_list

  def RunPerformanceTestAndParseResults(self, command_to_run, metric):
    """Runs a performance test on the current revision by executing the
    'command_to_run' and parses the results.

    Args:
      command_to_run: The command to be run to execute the performance test.
      metric: The metric to parse out from the results of the performance test.

    Returns:
      On success, it will return a tuple of the average value of the metric,
      and a success code of 0.
    """

    if self.opts.debug_ignore_perf_test:
      return ({'mean': 0.0, 'std_dev': 0.0}, 0)

    if IsWindows():
      command_to_run = command_to_run.replace('/', r'\\')

    args = shlex.split(command_to_run)

    cwd = os.getcwd()
    os.chdir(self.src_cwd)

    start_time = time.time()

    metric_values = []
    for i in xrange(self.opts.repeat_test_count):
      # Can ignore the return code since if the tests fail, it won't return 0.
      (output, return_code) = RunProcess(args,
          self.opts.output_buildbot_annotations)

      metric_values += self.ParseMetricValuesFromOutput(metric, output)

      elapsed_minutes = (time.time() - start_time) / 60.0

      if elapsed_minutes >= self.opts.repeat_test_max_time:
        break

    os.chdir(cwd)

    # Need to get the average value if there were multiple values.
    if metric_values:
      truncated_mean = CalculateTruncatedMean(metric_values,
          self.opts.truncate_percent)
      standard_dev = CalculateStandardDeviation(metric_values)

      values = {
        'mean': truncated_mean,
        'std_dev': standard_dev,
      }

      print 'Results of performance test: %12f %12f' % (
          truncated_mean, standard_dev)
      print
      return (values, 0)
    else:
      return ('No values returned from performance test.', -1)

  def FindAllRevisionsToSync(self, revision, depot):
    """Finds all dependant revisions and depots that need to be synced for a
    given revision. This is only useful in the git workflow, as an svn depot
    may be split into multiple mirrors.

    ie. skia is broken up into 3 git mirrors over skia/src, skia/gyp, and
    skia/include. To sync skia/src properly, one has to find the proper
    revisions in skia/gyp and skia/include.

    Args:
      revision: The revision to sync to.
      depot: The depot in use at the moment (probably skia).

    Returns:
      A list of [depot, revision] pairs that need to be synced.
    """
    revisions_to_sync = [[depot, revision]]

    use_gclient = (depot == 'chromium')

    # Some SVN depots were split into multiple git depots, so we need to
    # figure out for each mirror which git revision to grab. There's no
    # guarantee that the SVN revision will exist for each of the dependant
    # depots, so we have to grep the git logs and grab the next earlier one.
    if not use_gclient and\
       DEPOT_DEPS_NAME[depot]['depends'] and\
       self.source_control.IsGit():
      svn_rev = self.source_control.SVNFindRev(revision)

      for d in DEPOT_DEPS_NAME[depot]['depends']:
        self.ChangeToDepotWorkingDirectory(d)

        dependant_rev = self.source_control.ResolveToRevision(svn_rev, d, -1000)

        if dependant_rev:
          revisions_to_sync.append([d, dependant_rev])

      num_resolved = len(revisions_to_sync)
      num_needed = len(DEPOT_DEPS_NAME[depot]['depends'])

      self.ChangeToDepotWorkingDirectory(depot)

      if not ((num_resolved - 1) == num_needed):
        return None

    return revisions_to_sync

  def PerformPreBuildCleanup(self):
    """Performs necessary cleanup between runs."""
    print 'Cleaning up between runs.'
    print

    # Having these pyc files around between runs can confuse the
    # perf tests and cause them to crash.
    for (path, dir, files) in os.walk(os.getcwd()):
      for cur_file in files:
        if cur_file.endswith('.pyc'):
          path_to_file = os.path.join(path, cur_file)
          os.remove(path_to_file)

  def SyncBuildAndRunRevision(self, revision, depot, command_to_run, metric):
    """Performs a full sync/build/run of the specified revision.

    Args:
      revision: The revision to sync to.
      depot: The depot that's being used at the moment (src, webkit, etc.)
      command_to_run: The command to execute the performance test.
      metric: The performance metric being tested.

    Returns:
      On success, a tuple containing the results of the performance test.
      Otherwise, a tuple with the error message.
    """
    use_gclient = (depot == 'chromium')

    revisions_to_sync = self.FindAllRevisionsToSync(revision, depot)

    if not revisions_to_sync:
      return ('Failed to resolve dependant depots.', 1)

    success = True

    if not self.opts.debug_ignore_sync:
      for r in revisions_to_sync:
        self.ChangeToDepotWorkingDirectory(r[0])

        if use_gclient:
          self.PerformPreBuildCleanup()

        if not self.source_control.SyncToRevision(r[1], use_gclient):
          success = False

          break

    if success:
      if not(use_gclient):
        success = self.RunGClientHooks()

      if success:
        if self.BuildCurrentRevision():
          results = self.RunPerformanceTestAndParseResults(command_to_run,
                                                           metric)

          if results[1] == 0 and use_gclient:
            external_revisions = self.Get3rdPartyRevisionsFromCurrentRevision()

            if external_revisions:
              return (results[0], results[1], external_revisions)
            else:
              return ('Failed to parse DEPS file for external revisions.', 1)
          else:
            return results
        else:
          return ('Failed to build revision: [%s]' % (str(revision, )), 1)
      else:
        return ('Failed to run [gclient runhooks].', 1)
    else:
      return ('Failed to sync revision: [%s]' % (str(revision, )), 1)

  def CheckIfRunPassed(self, current_value, known_good_value, known_bad_value):
    """Given known good and bad values, decide if the current_value passed
    or failed.

    Args:
      current_value: The value of the metric being checked.
      known_bad_value: The reference value for a "failed" run.
      known_good_value: The reference value for a "passed" run.

    Returns:
      True if the current_value is closer to the known_good_value than the
      known_bad_value.
    """
    dist_to_good_value = abs(current_value['mean'] - known_good_value['mean'])
    dist_to_bad_value = abs(current_value['mean'] - known_bad_value['mean'])

    return dist_to_good_value < dist_to_bad_value

  def ChangeToDepotWorkingDirectory(self, depot_name):
    """Given a depot, changes to the appropriate working directory.

    Args:
      depot_name: The name of the depot (see DEPOT_NAMES).
    """
    if depot_name == 'chromium':
      os.chdir(self.src_cwd)
    elif depot_name in DEPOT_NAMES:
      os.chdir(self.depot_cwd[depot_name])
    else:
      assert False, 'Unknown depot [ %s ] encountered. Possibly a new one'\
                    ' was added without proper support?' %\
                    (depot_name,)

  def PrepareToBisectOnDepot(self,
                             current_depot,
                             end_revision,
                             start_revision):
    """Changes to the appropriate directory and gathers a list of revisions
    to bisect between |start_revision| and |end_revision|.

    Args:
      current_depot: The depot we want to bisect.
      end_revision: End of the revision range.
      start_revision: Start of the revision range.

    Returns:
      A list containing the revisions between |start_revision| and
      |end_revision| inclusive.
    """
    # Change into working directory of external library to run
    # subsequent commands.
    old_cwd = os.getcwd()
    os.chdir(self.depot_cwd[current_depot])

    # V8 (and possibly others) is merged in periodically. Bisecting
    # this directory directly won't give much good info.
    if DEPOT_DEPS_NAME[current_depot].has_key('build_with'):
      new_depot = DEPOT_DEPS_NAME[current_depot]['build_with']

      svn_start_revision = self.source_control.SVNFindRev(start_revision)
      svn_end_revision = self.source_control.SVNFindRev(end_revision)
      os.chdir(self.depot_cwd[new_depot])

      start_revision = self.source_control.ResolveToRevision(
          svn_start_revision, new_depot, -1000)
      end_revision = self.source_control.ResolveToRevision(
          svn_end_revision, new_depot, -1000)

      old_name = DEPOT_DEPS_NAME[current_depot]['src'][4:]
      new_name = DEPOT_DEPS_NAME[new_depot]['src'][4:]

      os.chdir(self.src_cwd)

      shutil.move(old_name, old_name + '.bak')
      shutil.move(new_name, old_name)
      os.chdir(self.depot_cwd[current_depot])

      self.cleanup_commands.append(['mv', old_name, new_name])
      self.cleanup_commands.append(['mv', old_name + '.bak', old_name])

      os.chdir(self.depot_cwd[current_depot])

    depot_revision_list = self.GetRevisionList(end_revision, start_revision)

    os.chdir(old_cwd)

    return depot_revision_list

  def GatherReferenceValues(self, good_rev, bad_rev, cmd, metric):
    """Gathers reference values by running the performance tests on the
    known good and bad revisions.

    Args:
      good_rev: The last known good revision where the performance regression
        has not occurred yet.
      bad_rev: A revision where the performance regression has already occurred.
      cmd: The command to execute the performance test.
      metric: The metric being tested for regression.

    Returns:
      A tuple with the results of building and running each revision.
    """
    bad_run_results = self.SyncBuildAndRunRevision(bad_rev,
                                                   'chromium',
                                                   cmd,
                                                   metric)

    good_run_results = None

    if not bad_run_results[1]:
      good_run_results = self.SyncBuildAndRunRevision(good_rev,
                                                      'chromium',
                                                      cmd,
                                                      metric)

    return (bad_run_results, good_run_results)

  def AddRevisionsIntoRevisionData(self, revisions, depot, sort, revision_data):
    """Adds new revisions to the revision_data dict and initializes them.

    Args:
      revisions: List of revisions to add.
      depot: Depot that's currently in use (src, webkit, etc...)
      sort: Sorting key for displaying revisions.
      revision_data: A dict to add the new revisions into. Existing revisions
        will have their sort keys offset.
    """

    num_depot_revisions = len(revisions)

    for k, v in revision_data.iteritems():
      if v['sort'] > sort:
        v['sort'] += num_depot_revisions

    for i in xrange(num_depot_revisions):
      r = revisions[i]

      revision_data[r] = {'revision' : r,
                          'depot' : depot,
                          'value' : None,
                          'passed' : '?',
                          'sort' : i + sort + 1}

  def PrintRevisionsToBisectMessage(self, revision_list, depot):
    if self.opts.output_buildbot_annotations:
      step_name = 'Bisection Range: [%s - %s]' % (
          revision_list[len(revision_list)-1], revision_list[0])
      bisect_utils.OutputAnnotationStepStart(step_name)

    print
    print 'Revisions to bisect on [%s]:' % depot
    for revision_id in revision_list:
      print '  -> %s' % (revision_id, )
    print

    if self.opts.output_buildbot_annotations:
      bisect_utils.OutputAnnotationStepClosed()

  def Run(self, command_to_run, bad_revision_in, good_revision_in, metric):
    """Given known good and bad revisions, run a binary search on all
    intermediate revisions to determine the CL where the performance regression
    occurred.

    Args:
        command_to_run: Specify the command to execute the performance test.
        good_revision: Number/tag of the known good revision.
        bad_revision: Number/tag of the known bad revision.
        metric: The performance metric to monitor.

    Returns:
        A dict with 2 members, 'revision_data' and 'error'. On success,
        'revision_data' will contain a dict mapping revision ids to
        data about that revision. Each piece of revision data consists of a
        dict with the following keys:

        'passed': Represents whether the performance test was successful at
            that revision. Possible values include: 1 (passed), 0 (failed),
            '?' (skipped), 'F' (build failed).
        'depot': The depot that this revision is from (ie. WebKit)
        'external': If the revision is a 'src' revision, 'external' contains
            the revisions of each of the external libraries.
        'sort': A sort value for sorting the dict in order of commits.

        For example:
        {
          'error':None,
          'revision_data':
          {
            'CL #1':
            {
              'passed':False,
              'depot':'chromium',
              'external':None,
              'sort':0
            }
          }
        }

        If an error occurred, the 'error' field will contain the message and
        'revision_data' will be empty.
    """

    results = {'revision_data' : {},
               'error' : None}

    # If they passed SVN CL's, etc... we can try match them to git SHA1's.
    bad_revision = self.source_control.ResolveToRevision(bad_revision_in,
                                                         'src', 100)
    good_revision = self.source_control.ResolveToRevision(good_revision_in,
                                                          'src', -100)

    if bad_revision is None:
      results['error'] = 'Could\'t resolve [%s] to SHA1.' % (bad_revision_in,)
      return results

    if good_revision is None:
      results['error'] = 'Could\'t resolve [%s] to SHA1.' % (good_revision_in,)
      return results

    if self.opts.output_buildbot_annotations:
      bisect_utils.OutputAnnotationStepStart('Gathering Revisions')

    print 'Gathering revision range for bisection.'

    # Retrieve a list of revisions to do bisection on.
    src_revision_list = self.GetRevisionList(bad_revision, good_revision)

    if self.opts.output_buildbot_annotations:
      bisect_utils.OutputAnnotationStepClosed()

    if src_revision_list:
      # revision_data will store information about a revision such as the
      # depot it came from, the webkit/V8 revision at that time,
      # performance timing, build state, etc...
      revision_data = results['revision_data']

      # revision_list is the list we're binary searching through at the moment.
      revision_list = []

      sort_key_ids = 0

      for current_revision_id in src_revision_list:
        sort_key_ids += 1

        revision_data[current_revision_id] = {'value' : None,
                                              'passed' : '?',
                                              'depot' : 'chromium',
                                              'external' : None,
                                              'sort' : sort_key_ids}
        revision_list.append(current_revision_id)

      min_revision = 0
      max_revision = len(revision_list) - 1

      self.PrintRevisionsToBisectMessage(revision_list, 'src')

      if self.opts.output_buildbot_annotations:
        bisect_utils.OutputAnnotationStepStart('Gathering Reference Values')

      print 'Gathering reference values for bisection.'

      # Perform the performance tests on the good and bad revisions, to get
      # reference values.
      (bad_results, good_results) = self.GatherReferenceValues(good_revision,
                                                               bad_revision,
                                                               command_to_run,
                                                               metric)

      if self.opts.output_buildbot_annotations:
        bisect_utils.OutputAnnotationStepClosed()

      if bad_results[1]:
        results['error'] = bad_results[0]
        return results

      if good_results[1]:
        results['error'] = good_results[0]
        return results


      # We need these reference values to determine if later runs should be
      # classified as pass or fail.
      known_bad_value = bad_results[0]
      known_good_value = good_results[0]

      # Can just mark the good and bad revisions explicitly here since we
      # already know the results.
      bad_revision_data = revision_data[revision_list[0]]
      bad_revision_data['external'] = bad_results[2]
      bad_revision_data['passed'] = 0
      bad_revision_data['value'] = known_bad_value

      good_revision_data = revision_data[revision_list[max_revision]]
      good_revision_data['external'] = good_results[2]
      good_revision_data['passed'] = 1
      good_revision_data['value'] = known_good_value

      while True:
        if not revision_list:
          break

        min_revision_data = revision_data[revision_list[min_revision]]
        max_revision_data = revision_data[revision_list[max_revision]]

        if max_revision - min_revision <= 1:
          if min_revision_data['passed'] == '?':
            next_revision_index = min_revision
          elif max_revision_data['passed'] == '?':
            next_revision_index = max_revision
          elif min_revision_data['depot'] == 'chromium':
            # If there were changes to any of the external libraries we track,
            # should bisect the changes there as well.
            external_depot = None

            for current_depot in DEPOT_NAMES:
              if DEPOT_DEPS_NAME[current_depot]["recurse"]:
                if min_revision_data['external'][current_depot] !=\
                   max_revision_data['external'][current_depot]:
                  external_depot = current_depot

                  break

            # If there was no change in any of the external depots, the search
            # is over.
            if not external_depot:
              break

            earliest_revision = max_revision_data['external'][current_depot]
            latest_revision = min_revision_data['external'][current_depot]

            new_revision_list = self.PrepareToBisectOnDepot(external_depot,
                                                            latest_revision,
                                                            earliest_revision)

            if not new_revision_list:
              results['error'] = 'An error occurred attempting to retrieve'\
                                 ' revision range: [%s..%s]' %\
                                 (depot_rev_range[1], depot_rev_range[0])
              return results

            self.AddRevisionsIntoRevisionData(new_revision_list,
                                              external_depot,
                                              min_revision_data['sort'],
                                              revision_data)

            # Reset the bisection and perform it on the newly inserted
            # changelists.
            revision_list = new_revision_list
            min_revision = 0
            max_revision = len(revision_list) - 1
            sort_key_ids += len(revision_list)

            print 'Regression in metric:%s appears to be the result of changes'\
                  ' in [%s].' % (metric, current_depot)

            self.PrintRevisionsToBisectMessage(revision_list, external_depot)

            continue
          else:
            break
        else:
          next_revision_index = int((max_revision - min_revision) / 2) +\
                                min_revision

        next_revision_id = revision_list[next_revision_index]
        next_revision_data = revision_data[next_revision_id]
        next_revision_depot = next_revision_data['depot']

        self.ChangeToDepotWorkingDirectory(next_revision_depot)

        if self.opts.output_buildbot_annotations:
          step_name = 'Working on [%s]' % next_revision_id
          bisect_utils.OutputAnnotationStepStart(step_name)

        print 'Working on revision: [%s]' % next_revision_id

        run_results = self.SyncBuildAndRunRevision(next_revision_id,
                                                   next_revision_depot,
                                                   command_to_run,
                                                   metric)

        if self.opts.output_buildbot_annotations:
          bisect_utils.OutputAnnotationStepClosed()

        # If the build is successful, check whether or not the metric
        # had regressed.
        if not run_results[1]:
          if next_revision_depot == 'chromium':
            next_revision_data['external'] = run_results[2]

          passed_regression = self.CheckIfRunPassed(run_results[0],
                                                    known_good_value,
                                                    known_bad_value)

          next_revision_data['passed'] = passed_regression
          next_revision_data['value'] = run_results[0]

          if passed_regression:
            max_revision = next_revision_index
          else:
            min_revision = next_revision_index
        else:
          next_revision_data['passed'] = 'F'

          # If the build is broken, remove it and redo search.
          revision_list.pop(next_revision_index)

          max_revision -= 1
    else:
      # Weren't able to sync and retrieve the revision range.
      results['error'] = 'An error occurred attempting to retrieve revision '\
                         'range: [%s..%s]' % (good_revision, bad_revision)

    return results

  def FormatAndPrintResults(self, bisect_results):
    """Prints the results from a bisection run in a readable format.

    Args
      bisect_results: The results from a bisection test run.
    """
    revision_data = bisect_results['revision_data']
    revision_data_sorted = sorted(revision_data.iteritems(),
                                  key = lambda x: x[1]['sort'])

    if self.opts.output_buildbot_annotations:
      bisect_utils.OutputAnnotationStepStart('Results')

    print
    print 'Full results of bisection:'
    for current_id, current_data  in revision_data_sorted:
      build_status = current_data['passed']

      if type(build_status) is bool:
        build_status = int(build_status)

      print '  %8s  %s  %s' % (current_data['depot'], current_id, build_status)
    print

    print
    print 'Tested commits:'
    for current_id, current_data in revision_data_sorted:
      if current_data['value']:
        print '  %8s  %s  %12f %12f' % (
            current_data['depot'], current_id,
            current_data['value']['mean'], current_data['value']['std_dev'])
    print

    # Find range where it possibly broke.
    first_working_revision = None
    last_broken_revision = None

    for k, v in revision_data_sorted:
      if v['passed'] == 1:
        if not first_working_revision:
          first_working_revision = k

      if not v['passed']:
        last_broken_revision = k

    if last_broken_revision != None and first_working_revision != None:
      print 'Results: Regression may have occurred in range:'
      print '  -> First Bad Revision: [%s] [%s]' %\
            (last_broken_revision,
            revision_data[last_broken_revision]['depot'])
      print '  -> Last Good Revision: [%s] [%s]' %\
            (first_working_revision,
            revision_data[first_working_revision]['depot'])

      cwd = os.getcwd()
      self.ChangeToDepotWorkingDirectory(
          revision_data[last_broken_revision]['depot'])
      info = self.source_control.QueryRevisionInfo(last_broken_revision)

      print
      print 'Commit  : %s' % last_broken_revision
      print 'Author  : %s' % info['author']
      print 'Email   : %s' % info['email']
      print 'Date    : %s' % info['date']
      print 'Subject : %s' % info['subject']
      print
      os.chdir(cwd)

      # Give a warning if the values were very close together
      good_std_dev = revision_data[first_working_revision]['value']['std_dev']
      good_mean = revision_data[first_working_revision]['value']['mean']
      bad_mean = revision_data[last_broken_revision]['value']['mean']

      # A standard deviation of 0 could indicate either insufficient runs
      # or a test that consistently returns the same value.
      if good_std_dev > 0:
        deviations = math.fabs(bad_mean - good_mean) / good_std_dev

        if deviations < 1.5:
          print 'Warning: Regression was less than 1.5 standard deviations '\
                'from "good" value. Results may not be accurate.'
          print
      elif self.opts.repeat_test_count == 1:
        print 'Warning: Tests were only set to run once. This may be '\
              'insufficient to get meaningful results.'
        print

      # Check for any other possible regression ranges
      prev_revision_data = revision_data_sorted[0][1]
      prev_revision_id = revision_data_sorted[0][0]
      possible_regressions = []
      for current_id, current_data in revision_data_sorted:
        if current_data['value']:
          prev_mean = prev_revision_data['value']['mean']
          cur_mean = current_data['value']['mean']

          if good_std_dev:
            deviations = math.fabs(prev_mean - cur_mean) / good_std_dev
          else:
            deviations = None

          if good_mean:
            percent_change = (prev_mean - cur_mean) / good_mean

            # If the "good" valuse are supposed to be higher than the "bad"
            # values (ie. scores), flip the sign of the percent change so that
            # a positive value always represents a regression.
            if bad_mean < good_mean:
              percent_change *= -1.0
          else:
            percent_change = None

          if deviations >= 1.5 or percent_change > 0.01:
            if current_id != first_working_revision:
              possible_regressions.append(
                  [current_id, prev_revision_id, percent_change, deviations])
          prev_revision_data = current_data
          prev_revision_id = current_id

      if possible_regressions:
        print
        print 'Other regressions may have occurred:'
        print
        for p in possible_regressions:
          current_id = p[0]
          percent_change = p[2]
          deviations = p[3]
          current_data = revision_data[current_id]
          previous_id = p[1]
          previous_data = revision_data[previous_id]

          if deviations is None:
            deviations = 'N/A'
          else:
            deviations = '%.2f' % deviations

          if percent_change is None:
            percent_change = 0

          print '  %8s  %s  [%.2f%%, %s x std.dev]' % (
              previous_data['depot'], previous_id, 100 * percent_change,
              deviations)
          print '  %8s  %s' % (
              current_data['depot'], current_id)
          print

    if self.opts.output_buildbot_annotations:
      bisect_utils.OutputAnnotationStepClosed()


def DetermineAndCreateSourceControl():
  """Attempts to determine the underlying source control workflow and returns
  a SourceControl object.

  Returns:
    An instance of a SourceControl object, or None if the current workflow
    is unsupported.
  """

  (output, return_code) = RunGit(['rev-parse', '--is-inside-work-tree'])

  if output.strip() == 'true':
    return GitSourceControl()

  return None


def SetNinjaBuildSystemDefault():
  """Makes ninja the default build system to be used by
  the bisection script."""
  gyp_var = os.getenv('GYP_GENERATORS')

  if not gyp_var or not 'ninja' in gyp_var:
    if gyp_var:
      os.environ['GYP_GENERATORS'] = gyp_var + ',ninja'
    else:
      os.environ['GYP_GENERATORS'] = 'ninja'

    if IsWindows():
      os.environ['GYP_DEFINES'] = 'component=shared_library '\
          'incremental_chrome_dll=1 disable_nacl=1 fastbuild=1 '\
          'chromium_win_pch=0'


def CheckPlatformSupported(opts):
  """Checks that this platform and build system are supported.

  Args:
    opts: The options parsed from the command line.

  Returns:
    True if the platform and build system are supported.
  """
  # Haven't tested the script out on any other platforms yet.
  supported = ['posix', 'nt']
  if not os.name in supported:
    print "Sorry, this platform isn't supported yet."
    print
    return False

  if IsWindows():
    if not opts.build_preference:
      opts.build_preference = 'msvs'

    if opts.build_preference == 'msvs':
      if not os.getenv('VS100COMNTOOLS'):
        print 'Error: Path to visual studio could not be determined.'
        print
        return False
    elif opts.build_preference == 'ninja':
      SetNinjaBuildSystemDefault()
    else:
      assert False, 'Error: %s build not supported' % opts.build_preference
  else:
    if not opts.build_preference:
      opts.build_preference = 'make'

    if opts.build_preference == 'ninja':
      SetNinjaBuildSystemDefault()
    elif opts.build_preference != 'make':
      assert False, 'Error: %s build not supported' % opts.build_preference

  bisect_utils.RunGClient(['runhooks'])

  return True


def RmTreeAndMkDir(path_to_dir):
  """Removes the directory tree specified, and then creates an empty
  directory in the same location.

  Args:
    path_to_dir: Path to the directory tree.

  Returns:
    True if successful, False if an error occurred.
  """
  try:
    if os.path.exists(path_to_dir):
      shutil.rmtree(path_to_dir)
  except OSError, e:
    if e.errno != errno.ENOENT:
      return False

  try:
    os.mkdir(path_to_dir)
  except OSError, e:
    if e.errno != errno.EEXIST:
      return False

  return True


def RemoveBuildFiles():
  """Removes build files from previous runs."""
  if RmTreeAndMkDir(os.path.join('out', 'Release')):
    if RmTreeAndMkDir(os.path.join('build', 'Release')):
      return True
  return False


def main():

  usage = ('%prog [options] [-- chromium-options]\n'
           'Perform binary search on revision history to find a minimal '
           'range of revisions where a peformance metric regressed.\n')

  parser = optparse.OptionParser(usage=usage)

  parser.add_option('-c', '--command',
                    type='str',
                    help='A command to execute your performance test at' +
                    ' each point in the bisection.')
  parser.add_option('-b', '--bad_revision',
                    type='str',
                    help='A bad revision to start bisection. ' +
                    'Must be later than good revision. May be either a git' +
                    ' or svn revision.')
  parser.add_option('-g', '--good_revision',
                    type='str',
                    help='A revision to start bisection where performance' +
                    ' test is known to pass. Must be earlier than the ' +
                    'bad revision. May be either a git or svn revision.')
  parser.add_option('-m', '--metric',
                    type='str',
                    help='The desired metric to bisect on. For example ' +
                    '"vm_rss_final_b/vm_rss_f_b"')
  parser.add_option('-w', '--working_directory',
                    type='str',
                    help='Path to the working directory where the script will '
                    'do an initial checkout of the chromium depot. The '
                    'files will be placed in a subdirectory "bisect" under '
                    'working_directory and that will be used to perform the '
                    'bisection. This parameter is optional, if it is not '
                    'supplied, the script will work from the current depot.')
  parser.add_option('-r', '--repeat_test_count',
                    type='int',
                    default=20,
                    help='The number of times to repeat the performance test. '
                    'Values will be clamped to range [1, 100]. '
                    'Default value is 20.')
  parser.add_option('--repeat_test_max_time',
                    type='int',
                    default=20,
                    help='The maximum time (in minutes) to take running the '
                    'performance tests. The script will run the performance '
                    'tests according to --repeat_test_count, so long as it '
                    'doesn\'t exceed --repeat_test_max_time. Values will be '
                    'clamped to range [1, 60].'
                    'Default value is 20.')
  parser.add_option('-t', '--truncate_percent',
                    type='int',
                    default=25,
                    help='The highest/lowest % are discarded to form a '
                    'truncated mean. Values will be clamped to range [0, 25]. '
                    'Default value is 25 (highest/lowest 25% will be '
                    'discarded).')
  parser.add_option('--build_preference',
                    type='choice',
                    choices=['msvs', 'ninja', 'make'],
                    help='The preferred build system to use. On linux/mac '
                    'the options are make/ninja. On Windows, the options '
                    'are msvs/ninja.')
  parser.add_option('--use_goma',
                    action="store_true",
                    help='Add a bunch of extra threads for goma.')
  parser.add_option('--output_buildbot_annotations',
                    action="store_true",
                    help='Add extra annotation output for buildbot.')
  parser.add_option('--debug_ignore_build',
                    action="store_true",
                    help='DEBUG: Don\'t perform builds.')
  parser.add_option('--debug_ignore_sync',
                    action="store_true",
                    help='DEBUG: Don\'t perform syncs.')
  parser.add_option('--debug_ignore_perf_test',
                    action="store_true",
                    help='DEBUG: Don\'t perform performance tests.')
  (opts, args) = parser.parse_args()

  if not opts.command:
    print 'Error: missing required parameter: --command'
    print
    parser.print_help()
    return 1

  if not opts.good_revision:
    print 'Error: missing required parameter: --good_revision'
    print
    parser.print_help()
    return 1

  if not opts.bad_revision:
    print 'Error: missing required parameter: --bad_revision'
    print
    parser.print_help()
    return 1

  if not opts.metric:
    print 'Error: missing required parameter: --metric'
    print
    parser.print_help()
    return 1

  opts.repeat_test_count = min(max(opts.repeat_test_count, 1), 100)
  opts.repeat_test_max_time = min(max(opts.repeat_test_max_time, 1), 60)
  opts.truncate_percent = min(max(opts.truncate_percent, 0), 25)
  opts.truncate_percent = opts.truncate_percent / 100.0

  metric_values = opts.metric.split('/')
  if len(metric_values) != 2:
    print "Invalid metric specified: [%s]" % (opts.metric,)
    print
    return 1

  if opts.working_directory:
    if bisect_utils.CreateBisectDirectoryAndSetupDepot(opts):
      return 1

    os.chdir(os.path.join(os.getcwd(), 'src'))

    if not RemoveBuildFiles():
      print "Something went wrong removing the build files."
      print
      return 1

  if not CheckPlatformSupported(opts):
    return 1

  # Check what source control method they're using. Only support git workflow
  # at the moment.
  source_control = DetermineAndCreateSourceControl()

  if not source_control:
    print "Sorry, only the git workflow is supported at the moment."
    print
    return 1

  # gClient sync seems to fail if you're not in master branch.
  if not source_control.IsInProperBranch() and not opts.debug_ignore_sync:
    print "You must switch to master branch to run bisection."
    print
    return 1

  bisect_test = BisectPerformanceMetrics(source_control, opts)
  try:
    bisect_results = bisect_test.Run(opts.command,
                                     opts.bad_revision,
                                     opts.good_revision,
                                     metric_values)
    if not(bisect_results['error']):
      bisect_test.FormatAndPrintResults(bisect_results)
  finally:
    bisect_test.PerformCleanup()

  if not(bisect_results['error']):
    return 0
  else:
    print 'Error: ' + bisect_results['error']
    print
    return 1

if __name__ == '__main__':
  sys.exit(main())
