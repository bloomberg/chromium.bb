# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Script to summarize stats for different builds in prod."""

from __future__ import print_function

import datetime
import itertools
import numpy
import re
import sys

from chromite.cbuildbot import constants
from chromite.lib import cidb
from chromite.lib import commandline
from chromite.lib import cros_build_lib
from chromite.lib import cros_logging as logging


# These are the preferred base URLs we use to canonicalize bugs/CLs.
BUGANIZER_BASE_URL = 'b/'
GUTS_BASE_URL = 't/'
CROS_BUG_BASE_URL = 'crbug.com/'
INTERNAL_CL_BASE_URL = 'crosreview.com/i/'
EXTERNAL_CL_BASE_URL = 'crosreview.com/'
CHROMIUM_CL_BASE_URL = 'codereview.chromium.org/'

class CLStatsEngine(object):
  """Engine to generate stats about CL actions taken by the Commit Queue."""

  def __init__(self, db):
    self.db = db
    self.builds = []
    self.claction_history = None
    self.reasons = {}
    self.blames = {}
    self.summary = {}
    self.builds_by_build_id = {}
    self.slave_builds_by_master_id = {}
    self.slave_builds_by_config = {}

  def GatherBuildAnnotations(self):
    """Gather the failure annotations for builds from cidb."""
    annotations_by_builds = self.db.GetAnnotationsForBuilds(
        [b['id'] for b in self.builds])
    for b in self.builds:
      build_id = b['id']
      build_number = b['build_number']
      annotations = annotations_by_builds.get(build_id, [])
      if not annotations:
        self.reasons[build_number] = ['None']
        self.blames[build_number] = []
      else:
        # TODO(pprabhu) crbug.com/458275
        # We currently squash together multiple annotations into one to ease
        # co-existence with the spreadsheet based logic. Once we've moved off of
        # using the spreadsheet, we should update all uses of the annotations to
        # expect one or more annotations.
        self.reasons[build_number] = [
            a['failure_category'] for a in annotations]
        self.blames[build_number] = []
        for annotation in annotations:
          self.blames[build_number] += self.ProcessBlameString(
              annotation['blame_url'])

  @staticmethod
  def ProcessBlameString(blame_string):
    """Parse a human-created |blame_string| from the spreadsheet.

    Returns:
      A list of canonicalized URLs for bugs or CLs that appear in the blame
      string. Canonicalized form will be 'crbug.com/1234',
      'crosreview.com/1234', 'b/1234', 't/1234', or 'crosreview.com/i/1234' as
      applicable.
    """
    urls = []
    tokens = blame_string.split()

    # Format to generate the regex patterns. Matches one of provided domain
    # names, followed by lazy wildcard, followed by greedy digit wildcard,
    # followed by optional slash and optional comma and optional (# +
    # alphanum wildcard).
    general_regex = r'^.*(%s).*?([0-9]+)/?,?(#\S*)?$'

    crbug = general_regex % r'crbug.com|bugs.chromium.org'
    internal_review = general_regex % (
        r'chrome-internal-review.googlesource.com|crosreview.com/i')
    external_review = general_regex % (
        r'crosreview.com|chromium-review.googlesource.com')
    guts = general_regex % r't/|gutsv\d.corp.google.com/#ticket/'
    chromium_review = general_regex % r'codereview.chromium.org'

    # Buganizer regex is different, as buganizer urls do not end with the bug
    # number.
    buganizer = r'^.*(b/|b.corp.google.com/issue\?id=)([0-9]+).*$'

    # Patterns need to be tried in a specific order -- internal review needs
    # to be tried before external review, otherwise urls like crosreview.com/i
    # will be incorrectly parsed as external.
    patterns = [crbug,
                internal_review,
                external_review,
                buganizer,
                guts,
                chromium_review]
    url_patterns = [CROS_BUG_BASE_URL,
                    INTERNAL_CL_BASE_URL,
                    EXTERNAL_CL_BASE_URL,
                    BUGANIZER_BASE_URL,
                    GUTS_BASE_URL,
                    CHROMIUM_CL_BASE_URL]

    for t in tokens:
      for p, u in zip(patterns, url_patterns):
        m = re.match(p, t)
        if m:
          urls.append(u + m.group(2))
          break

    return urls

  def Gather(self, start_date, end_date,
             master_config=constants.CQ_MASTER,
             sort_by_build_number=True,
             starting_build_number=None,
             ending_build_number=None):
    """Fetches build data and failure reasons.

    Args:
      start_date: A datetime.date instance for the earliest build to
          examine.
      end_date: A datetime.date instance for the latest build to
          examine.
      master_config: Config name of master to gather data for.
                     Default to CQ_MASTER.
      sort_by_build_number: Optional boolean. If True, builds will be
          sorted by build number.
      starting_build_number: (optional) The lowest build number to
          include in the results.
      ending_build_number: (optional) The highest build number to include
          in the results.
    """
    logging.info('Gathering data for %s from %s until %s', master_config,
                 start_date, end_date)
    self.builds = self.db.GetBuildHistory(
        master_config,
        start_date=start_date,
        end_date=end_date,
        starting_build_number=starting_build_number,
        num_results=self.db.NUM_RESULTS_NO_LIMIT)
    if ending_build_number is not None:
      self.builds = [x for x in self.builds
                     if x['build_number'] <= ending_build_number]
    if self.builds:
      logging.info('Fetched %d builds (build_id: %d to %d)', len(self.builds),
                   self.builds[0]['id'], self.builds[-1]['id'])
    else:
      logging.info('Fetched no builds.')
    if sort_by_build_number:
      logging.info('Sorting by build number.')
      self.builds.sort(key=lambda x: x['build_number'])

    self.claction_history = self.db.GetActionHistory(start_date, end_date)
    self.GatherBuildAnnotations()

    self.builds_by_build_id.update(
        {b['id'] : b for b in self.builds})

    # Gather slave statuses for each of the master builds. For now this is a
    # separate query per CQ run, but this could be consolidated to a single
    # query if necessary (requires adding a cidb.py API method).
    for bid in self.builds_by_build_id:
      self.slave_builds_by_master_id[bid] = self.db.GetSlaveStatuses(bid)

    self.slave_builds_by_config = cros_build_lib.GroupByKey(
        itertools.chain(*self.slave_builds_by_master_id.values()),
        'build_config')

  def _PrintCounts(self, reasons, fmt):
    """Print a sorted list of reasons in descending order of frequency.

    Args:
      reasons: A key/value mapping mapping the reason to the count.
      fmt: A format string for our log message, containing %(cnt)d
        and %(reason)s.
    """
    d = reasons
    for cnt, reason in sorted(((v, k) for (k, v) in d.items()), reverse=True):
      logging.info(fmt, dict(cnt=cnt, reason=reason))
    if not d:
      logging.info('  None')

  def FalseRejectionRate(self, good_patch_count, false_rejection_count):
    """Calculate the false rejection ratio.

    This is the chance that a good patch will be rejected by the Pre-CQ or CQ
    in a given run.

    Args:
      good_patch_count: The number of good patches in the run.
      false_rejection_count: A dict containing the number of false rejections
          for the CQ and PRE_CQ.

    Returns:
      A dict containing the false rejection ratios for CQ, PRE_CQ, and combined.
    """
    false_rejection_rate = dict()
    for bot, rejection_count in false_rejection_count.iteritems():
      false_rejection_rate[bot] = (
          rejection_count * 100. / (rejection_count + good_patch_count)
      )
    false_rejection_rate['combined'] = 0
    if good_patch_count:
      rejection_count = sum(false_rejection_count.values())
      false_rejection_rate['combined'] = (
          rejection_count * 100. / (good_patch_count + rejection_count)
      )
    return false_rejection_rate

  def GetBuildRunTimes(self, builds):
    """Gets the elapsed run times of the completed builds within |builds|.

    Args:
      builds: Iterable of build statuses as returned by cidb.

    Returns:
      A list of the elapsed times (in seconds) of the builds that completed.
    """
    times = []
    for b in builds:
      if b['finish_time']:
        td = (b['finish_time'] - b['start_time']).total_seconds()
        times.append(td)
    return times

  def Summarize(self, build_type, bad_patch_candidates=False):
    """Process, print, and return a summary of statistics.

    As a side effect, save summary to self.summary.

    Returns:
      A dictionary summarizing the statistics.
    """
    if build_type == 'cq':
      return self.SummarizeCQ(bad_patch_candidates=bad_patch_candidates)
    else:
      return self.SummarizePFQ()

  def SummarizeCQ(self, bad_patch_candidates=False):
    """Process, print, and return a summary of cl action statistics.

    As a side effect, save summary to self.summary.

    Returns:
      A dictionary summarizing the statistics.
    """
    if self.builds:
      logging.info('%d total runs included, from build %d to %d.',
                   len(self.builds), self.builds[-1]['build_number'],
                   self.builds[0]['build_number'])
      total_passed = len([b for b in self.builds
                          if b['status'] == constants.BUILDER_STATUS_PASSED])
      logging.info('%d of %d runs passed.', total_passed, len(self.builds))
    else:
      logging.info('No runs included.')

    build_times_sec = sorted(self.GetBuildRunTimes(self.builds))

    build_reason_counts = {}
    for reasons in self.reasons.values():
      for reason in reasons:
        if reason != 'None':
          build_reason_counts[reason] = build_reason_counts.get(reason, 0) + 1

    unique_blames = set()
    build_blame_counts = {}
    for blames in self.blames.itervalues():
      unique_blames.update(blames)
      for blame in blames:
        build_blame_counts[blame] = build_blame_counts.get(blame, 0) + 1
    unique_cl_blames = {blame for blame in unique_blames if
                        EXTERNAL_CL_BASE_URL in blame}

    # Shortcuts to some time aggregates about action history.
    patch_handle_times = self.claction_history.GetPatchHandlingTimes().values()
    pre_cq_handle_times = self.claction_history.GetPreCQHandlingTimes().values()
    cq_wait_times = self.claction_history.GetCQWaitingTimes().values()
    cq_handle_times = self.claction_history.GetCQHandlingTimes().values()

    # Calculate how many good patches were falsely rejected and why.
    good_patch_rejections = self.claction_history.GetFalseRejections()
    patch_reason_counts = {}
    patch_blame_counts = {}
    for k, v in good_patch_rejections.iteritems():
      for a in v:
        build = self.builds_by_build_id.get(a.build_id)
        if a.bot_type == constants.CQ and build is not None:
          build_number = build['build_number']
          reasons = self.reasons.get(build_number, ['None'])
          blames = self.blames.get(build_number, ['None'])
          for x in reasons:
            patch_reason_counts[x] = patch_reason_counts.get(x, 0) + 1
          for x in blames:
            patch_blame_counts[x] = patch_blame_counts.get(x, 0) + 1

    good_patch_count = len(self.claction_history.GetSubmittedPatches(False))
    false_rejection_count = {}
    bad_cl_candidates = {}
    for bot_type in [constants.CQ, constants.PRE_CQ]:
      rejections = self.claction_history.GetFalseRejections(bot_type)
      false_rejection_count[bot_type] = sum(map(len,
                                                rejections.values()))

      rejections = self.claction_history.GetTrueRejections(bot_type)
      rejected_cls = set([x.GetChangeTuple() for x in rejections.keys()])
      bad_cl_candidates[bot_type] = self.claction_history.SortBySubmitTimes(
          rejected_cls)

    false_rejection_rate = self.FalseRejectionRate(good_patch_count,
                                                   false_rejection_count)

    # This list counts how many times each good patch was rejected.
    rejection_counts = [0] * (good_patch_count - len(good_patch_rejections))
    rejection_counts += [len(x) for x in good_patch_rejections.values()]

    # Break down the frequency of how many times each patch is rejected.
    good_patch_rejection_breakdown = []
    if rejection_counts:
      for x in range(max(rejection_counts) + 1):
        good_patch_rejection_breakdown.append((x, rejection_counts.count(x)))

    # For CQ runs that passed, track which slave was the long pole, i.e. the
    # last to finish.
    long_pole_slave_counts = {}
    for bid, master_build in self.builds_by_build_id.items():
      if master_build['status'] == constants.BUILDER_STATUS_PASSED:
        if not self.slave_builds_by_master_id[bid]:
          continue
        _, long_config = max((slave['finish_time'], slave['build_config'])
                             for slave in self.slave_builds_by_master_id[bid]
                             if slave['finish_time'] and slave['important'])
        long_pole_slave_counts[long_config] = (
            long_pole_slave_counts.get(long_config, 0) + 1)

    summary = {
        'total_cl_actions': len(self.claction_history),
        'unique_cls': len(self.claction_history.affected_cls),
        'unique_patches': len(self.claction_history.affected_patches),
        'submitted_patches': len(self.claction_history.submit_actions),
        'rejections': len(self.claction_history.reject_actions),
        'submit_fails': len(self.claction_history.submit_fail_actions),
        'good_patch_rejections': sum(rejection_counts),
        'mean_good_patch_rejections': numpy.mean(rejection_counts),
        'good_patch_rejection_breakdown': good_patch_rejection_breakdown,
        'good_patch_rejection_count': false_rejection_count,
        'false_rejection_rate': false_rejection_rate,
        'median_handling_time': numpy.median(patch_handle_times),
        'patch_handling_time': patch_handle_times,
        'bad_cl_candidates': bad_cl_candidates,
        'unique_blames_change_count': len(unique_cl_blames),
        'long_pole_slave_counts': long_pole_slave_counts,
    }

    logging.info('CQ committed %s changes', summary['submitted_patches'])
    logging.info('CQ correctly rejected %s unique changes',
                 summary['unique_blames_change_count'])
    logging.info('pre-CQ and CQ incorrectly rejected %s changes a total of '
                 '%s times (pre-CQ: %s; CQ: %s)',
                 len(good_patch_rejections),
                 sum(false_rejection_count.values()),
                 false_rejection_count[constants.PRE_CQ],
                 false_rejection_count[constants.CQ])

    logging.info('      Total CL actions: %d.', summary['total_cl_actions'])
    logging.info('    Unique CLs touched: %d.', summary['unique_cls'])
    logging.info('Unique patches touched: %d.', summary['unique_patches'])
    logging.info('   Total CLs submitted: %d.', summary['submitted_patches'])
    logging.info('      Total rejections: %d.', summary['rejections'])
    logging.info(' Total submit failures: %d.', summary['submit_fails'])
    logging.info(' Good patches rejected: %d.',
                 len(good_patch_rejections))
    logging.info('   Mean rejections per')
    logging.info('            good patch: %.2f',
                 summary['mean_good_patch_rejections'])
    logging.info(' False rejection rate for CQ: %.1f%%',
                 summary['false_rejection_rate'].get(constants.CQ, 0))
    logging.info(' False rejection rate for Pre-CQ: %.1f%%',
                 summary['false_rejection_rate'].get(constants.PRE_CQ, 0))
    logging.info(' Combined false rejection rate: %.1f%%',
                 summary['false_rejection_rate']['combined'])

    for x, p in summary['good_patch_rejection_breakdown']:
      logging.info('%d good patches were rejected %d times.', p, x)
    logging.info('')
    logging.info('Good patch handling time:')
    logging.info('  10th percentile: %.2f hours',
                 numpy.percentile(patch_handle_times, 10) / 3600.0)
    logging.info('  25th percentile: %.2f hours',
                 numpy.percentile(patch_handle_times, 25) / 3600.0)
    logging.info('  50th percentile: %.2f hours',
                 summary['median_handling_time'] / 3600.0)
    logging.info('  75th percentile: %.2f hours',
                 numpy.percentile(patch_handle_times, 75) / 3600.0)
    logging.info('  90th percentile: %.2f hours',
                 numpy.percentile(patch_handle_times, 90) / 3600.0)
    logging.info('')
    logging.info('Time spent in Pre-CQ:')
    logging.info('  10th percentile: %.2f hours',
                 numpy.percentile(pre_cq_handle_times, 10) / 3600.0)
    logging.info('  25th percentile: %.2f hours',
                 numpy.percentile(pre_cq_handle_times, 25) / 3600.0)
    logging.info('  50th percentile: %.2f hours',
                 numpy.percentile(pre_cq_handle_times, 50) / 3600.0)
    logging.info('  75th percentile: %.2f hours',
                 numpy.percentile(pre_cq_handle_times, 75) / 3600.0)
    logging.info('  90th percentile: %.2f hours',
                 numpy.percentile(pre_cq_handle_times, 90) / 3600.0)
    logging.info('')
    logging.info('Time spent waiting for CQ:')
    logging.info('  10th percentile: %.2f hours',
                 numpy.percentile(cq_wait_times, 10) / 3600.0)
    logging.info('  25th percentile: %.2f hours',
                 numpy.percentile(cq_wait_times, 25) / 3600.0)
    logging.info('  50th percentile: %.2f hours',
                 numpy.percentile(cq_wait_times, 50) / 3600.0)
    logging.info('  75th percentile: %.2f hours',
                 numpy.percentile(cq_wait_times, 75) / 3600.0)
    logging.info('  90th percentile: %.2f hours',
                 numpy.percentile(cq_wait_times, 90) / 3600.0)
    logging.info('')
    logging.info('Time spent in CQ:')
    logging.info('  10th percentile: %.2f hours',
                 numpy.percentile(cq_handle_times, 10) / 3600.0)
    logging.info('  25th percentile: %.2f hours',
                 numpy.percentile(cq_handle_times, 25) / 3600.0)
    logging.info('  50th percentile: %.2f hours',
                 numpy.percentile(cq_handle_times, 50) / 3600.0)
    logging.info('  75th percentile: %.2f hours',
                 numpy.percentile(cq_handle_times, 75) / 3600.0)
    logging.info('  90th percentile: %.2f hours',
                 numpy.percentile(cq_handle_times, 90) / 3600.0)
    logging.info('')

    # Log some statistics about cq-master run-time.
    logging.info('CQ-master run time:')
    logging.info('  50th percentile: %.2f hours',
                 numpy.percentile(build_times_sec, 50) / 3600.0)
    logging.info('  90th percenfile: %.2f hours',
                 numpy.percentile(build_times_sec, 90) / 3600.0)

    for bot_type, patches in summary['bad_cl_candidates'].items():
      logging.info('%d bad patch candidates were rejected by the %s',
                   len(patches), bot_type)
      if bad_patch_candidates:
        for k in patches:
          logging.info('Bad patch candidate in: %s', k)

    fmt_fai = '  %(cnt)d failures in %(reason)s'
    fmt_rej = '  %(cnt)d rejections due to %(reason)s'

    logging.info('Reasons why good patches were rejected:')
    self._PrintCounts(patch_reason_counts, fmt_rej)

    logging.info('Bugs or CLs responsible for good patches rejections:')
    self._PrintCounts(patch_blame_counts, fmt_rej)

    logging.info('Reasons why builds failed:')
    self._PrintCounts(build_reason_counts, fmt_fai)

    logging.info('Bugs or CLs responsible for build failures:')
    self._PrintCounts(build_blame_counts, fmt_fai)

    total_counts = sum(long_pole_slave_counts.values())
    logging.info('Slowest CQ slaves out of %s passing runs:', total_counts)
    for (count, config) in sorted(
        (v, k) for (k, v) in long_pole_slave_counts.items()):
      if count < (total_counts / 20.0):
        continue
      build_times = self.GetBuildRunTimes(self.slave_builds_by_config[config])
      logging.info('%s times the slowest slave was %s', count, config)
      logging.info('  50th percentile: %.2f hours, 90th percentile: %.2f hours',
                   numpy.percentile(build_times, 50) / 3600.0,
                   numpy.percentile(build_times, 90) / 3600.0)

    return summary

  # TODO(akeshet): some of this logic is copied directly from SummarizeCQ.
  # Refactor to reuse that code instead.
  def SummarizePFQ(self):
    """Process, print, and return a summary of pfq bug and failure statistics.

    As a side effect, save summary to self.summary.

    Returns:
      A dictionary summarizing the statistics.
    """
    if self.builds:
      logging.info('%d total runs included, from build %d to %d.',
                   len(self.builds), self.builds[-1]['build_number'],
                   self.builds[0]['build_number'])
      total_passed = len([b for b in self.builds
                          if b['status'] == constants.BUILDER_STATUS_PASSED])
      logging.info('%d of %d runs passed.', total_passed, len(self.builds))
    else:
      logging.info('No runs included.')

    # TODO(akeshet): This is the end of the verbatim copied code.

    # Count the number of times each particular (canonicalized) blame url was
    # given.
    unique_blame_counts = {}
    for blames in self.blames.itervalues():
      for b in blames:
        unique_blame_counts[b] = unique_blame_counts.get(b, 0) + 1

    top_blames = sorted([(count, blame) for
                         blame, count in unique_blame_counts.iteritems()],
                        reverse=True)
    logging.info('Top blamed issues:')
    if top_blames:
      for tb in top_blames:
        logging.info('   %s x %s', tb[0], tb[1])
    else:
      logging.info('None!')

    return {}


def _CheckOptions(options):
  # Ensure that specified start date is in the past.
  now = datetime.datetime.now()
  if options.start_date and now.date() < options.start_date:
    logging.error('Specified start date is in the future: %s',
                  options.start_date)
    return False

  return True


def GetParser():
  """Creates the argparse parser."""
  parser = commandline.ArgumentParser(description=__doc__)

  ex_group = parser.add_mutually_exclusive_group(required=True)
  ex_group.add_argument('--start-date', action='store', type='date',
                        default=None,
                        help='Limit scope to a start date in the past.')
  ex_group.add_argument('--past-month', action='store_true', default=False,
                        help='Limit scope to the past 30 days up to now.')
  ex_group.add_argument('--past-week', action='store_true', default=False,
                        help='Limit scope to the past week up to now.')
  ex_group.add_argument('--past-day', action='store_true', default=False,
                        help='Limit scope to the past day up to now.')

  parser.add_argument('--cred-dir', action='store', required=True,
                      metavar='CIDB_CREDENTIALS_DIR',
                      help='Database credentials directory with certificates '
                           'and other connection information. Obtain your '
                           'credentials at go/cros-cidb-admin .')
  parser.add_argument('--starting-build', action='store', type=int,
                      default=None, help='Filter to builds after given number'
                                         '(inclusive).')
  parser.add_argument('--ending-build', action='store', type=int,
                      default=None, help='Builder to builds before a given '
                                         'number (inclusive).')
  parser.add_argument('--end-date', action='store', type='date', default=None,
                      help='Limit scope to an end date in the past.')

  parser.add_argument('--build-type', choices=['cq', 'chrome-pfq'],
                      default='cq',
                      help='Build type to summarize. Default: cq.')
  parser.add_argument('--bad-patch-candidates', action='store_true',
                      default=False,
                      help='In CQ mode, whether to print bad patch '
                           'candidates.')
  return parser


def main(argv):
  parser = GetParser()
  options = parser.parse_args(argv)

  if not _CheckOptions(options):
    sys.exit(1)

  db = cidb.CIDBConnection(options.cred_dir)

  if options.end_date:
    end_date = options.end_date
  else:
    end_date = datetime.datetime.now().date()

  # Determine the start date to use, which is required.
  if options.start_date:
    start_date = options.start_date
  else:
    assert options.past_month or options.past_week or options.past_day
    if options.past_month:
      start_date = end_date - datetime.timedelta(days=30)
    elif options.past_week:
      start_date = end_date - datetime.timedelta(days=7)
    else:
      start_date = end_date - datetime.timedelta(days=1)

  if options.build_type == 'cq':
    master_config = constants.CQ_MASTER
  else:
    master_config = constants.PFQ_MASTER

  cl_stats_engine = CLStatsEngine(db)
  cl_stats_engine.Gather(start_date, end_date, master_config,
                         starting_build_number=options.starting_build,
                         ending_build_number=options.ending_build)
  cl_stats_engine.Summarize(options.build_type,
                            options.bad_patch_candidates)
