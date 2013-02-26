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

import imp
import optparse
import os
import re
import shlex
import subprocess
import sys

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
DEPOT_DEPS_NAME = { 'webkit' : {
                      "src" : "src/third_party/WebKit",
                      "recurse" : True,
                      "depends" : None
                    },
                    'v8' : {
                      "src" : "src/v8",
                      "recurse" : True,
                      "depends" : None
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


def RunProcess(command):
  """Run an arbitrary command, returning its output and return code.

  Args:
    command: A list containing the command and args to execute.

  Returns:
    A tuple of the output and return code.
  """
  # On Windows, use shell=True to get PATH interpretation.
  shell = (os.name == 'nt')
  proc = subprocess.Popen(command,
                          shell=shell,
                          stdout=subprocess.PIPE,
                          stderr=subprocess.PIPE)
  out = proc.communicate()[0]

  return (out, proc.returncode)


def RunGit(command):
  """Run a git subcommand, returning its output and return code.

  Args:
    command: A list containing the args to git.

  Returns:
    A tuple of the output and return code.
  """
  command = ['git'] + command

  return RunProcess(command)


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
      A tuple of the output and return code.
    """
    args = ['gclient', 'sync', '--revision', revision]

    return RunProcess(args)


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
      results = RunGit(['checkout', revision])

    return not results[1]

  def ResolveToRevision(self, revision_to_check, depot, search=1):
    """If an SVN revision is supplied, try to resolve it to a git SHA1.

    Args:
      revision_to_check: The user supplied revision string that may need to be
        resolved to a git SHA1.
      depot: The depot the revision_to_check is from.
      search: The number of changelists to try if the first fails to resolve
        to a git hash.

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

    for i in xrange(svn_revision, svn_revision - search, -1):
      svn_pattern = 'git-svn-id: %s@%d' %\
                    (depot_svn, i)
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

    for d in DEPOT_NAMES:
      # The working directory of each depot is just the path to the depot, but
      # since we're already in 'src', we can skip that part.

      self.depot_cwd[d] = self.src_cwd + DEPOT_DEPS_NAME[d]['src'][3:]

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

    gyp_var = os.getenv('GYP_GENERATORS')

    num_threads = 16

    if self.opts.use_goma:
      num_threads = 100

    if gyp_var != None and 'ninja' in gyp_var:
      args = ['ninja',
              '-C',
              'out/Release',
              '-j%d' % num_threads,
              'chrome',
              'performance_ui_tests']
    else:
      args = ['make',
              'BUILDTYPE=Release',
              '-j%d' % num_threads,
              'chrome',
              'performance_ui_tests']

    cwd = os.getcwd()
    os.chdir(self.src_cwd)

    (output, return_code) = RunProcess(args)

    os.chdir(cwd)

    return not return_code

  def RunGClientHooks(self):
    """Runs gclient with runhooks command.

    Returns:
      True if gclient reports no errors.
    """

    if self.opts.debug_ignore_build:
      return True

    results = RunProcess(['gclient', 'runhooks'])

    return not results[1]

  def ParseMetricValuesFromOutput(self, metric, text):
    """Parses output from performance_ui_tests and retrieves the results for
    a given metric.

    Args:
      metric: The metric as a list of [<trace>, <value>] strings.
      text: The text to parse the metric values from.

    Returns:
      A dict of lists of floating point numbers found.

      {
        'list_name':[values]
      }
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
    values_dict = {}

    # If the metric is times/t, we need to group the timings by page or the
    # results aren't very useful.
    # Will make the assumption that if pages are supplied, and the metric
    # is "times/t", that the results needs to be grouped.
    metric_re = "Pages:(\s)*\[(\s)*(?P<values>[a-zA-Z0-9-_,.]+)\]"
    metric_re = re.compile(metric_re)

    regex_results = metric_re.search(text)
    page_names = []

    if regex_results:
      page_names = regex_results.group('values')
      page_names = page_names.split(',')

    if not metric == ['times', 't'] or not page_names:
      values_dict['%s: %s' % (metric[0], metric[1])] = values_list
    else:
      if not (len(values_list) % len(page_names)):
        values_dict = dict([(k, []) for k in page_names])

        num = len(values_list) / len(page_names)

        for j in xrange(num):
          for i in xrange(len(page_names)):
            k = num * j + i
            values_dict[page_names[i]].append(values_list[k])

    return values_dict

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
      return ({'debug' : 0.0}, 0)

    args = shlex.split(command_to_run)

    cwd = os.getcwd()
    os.chdir(self.src_cwd)

    # Can ignore the return code since if the tests fail, it won't return 0.
    (output, return_code) = RunProcess(args)

    os.chdir(cwd)

    metric_values = self.ParseMetricValuesFromOutput(metric, output)

    # Need to get the average value if there were multiple values.
    if metric_values:
      for k, v in metric_values.iteritems():
        average_metric_value = reduce(lambda x, y: float(x) + float(y),
                                      v) / len(v)

        metric_values[k] = average_metric_value

      return (metric_values, 0)
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

        dependant_rev = self.source_control.ResolveToRevision(svn_rev, d, 1000)

        if dependant_rev:
          revisions_to_sync.append([d, dependant_rev])

      num_resolved = len(revisions_to_sync)
      num_needed = len(DEPOT_DEPS_NAME[depot]['depends'])

      self.ChangeToDepotWorkingDirectory(depot)

      if not ((num_resolved - 1) == num_needed):
        return None

    return revisions_to_sync

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
    passes = 0
    fails = 0

    for k in current_value.keys():
      dist_to_good_value = abs(current_value[k] - known_good_value[k])
      dist_to_bad_value = abs(current_value[k] - known_bad_value[k])

      if dist_to_good_value < dist_to_bad_value:
        passes += 1
      else:
        fails += 1

    return passes > fails

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
                                                         'src')
    good_revision = self.source_control.ResolveToRevision(good_revision_in,
                                                          'src')

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

            rev_range = [min_revision_data['external'][current_depot],
                         max_revision_data['external'][current_depot]]

            new_revision_list = self.PrepareToBisectOnDepot(external_depot,
                                                            rev_range[1],
                                                            rev_range[0])

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

  # Haven't tested the script out on any other platforms yet.
  if not os.name in ['posix']:
    print "Sorry, this platform isn't supported yet."
    print
    return 1

  metric_values = opts.metric.split('/')
  if len(metric_values) != 2:
    print "Invalid metric specified: [%s]" % (opts.metric,)
    print
    return 1

  if opts.working_directory:
    if bisect_utils.CreateBisectDirectoryAndSetupDepot(opts):
      return 1

    os.chdir(os.path.join(os.getcwd(), 'src'))

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
  bisect_results = bisect_test.Run(opts.command,
                                   opts.bad_revision,
                                   opts.good_revision,
                                   metric_values)

  if not(bisect_results['error']):
    bisect_test.FormatAndPrintResults(bisect_results)
    return 0
  else:
    print 'Error: ' + bisect_results['error']
    print
    return 1

if __name__ == '__main__':
  sys.exit(main())
