# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import math
import os

import bisect_utils
import math_utils
import ttest


def ConfidenceScore(good_results_lists, bad_results_lists):
  """Calculates a confidence score.

  This score is a percentage which represents our degree of confidence in the
  proposition that the good results and bad results are distinct groups, and
  their differences aren't due to chance alone.


  Args:
    good_results_lists: A list of lists of "good" result numbers.
    bad_results_lists: A list of lists of "bad" result numbers.

  Returns:
    A number in the range [0, 100].
  """
  # If there's only one item in either list, this means only one revision was
  # classified good or bad; this isn't good enough evidence to make a decision.
  # If an empty list was passed, that also implies zero confidence.
  if len(good_results_lists) <= 1 or len(bad_results_lists) <= 1:
    return 0.0

  # Flatten the lists of results lists.
  sample1 = sum(good_results_lists, [])
  sample2 = sum(bad_results_lists, [])

  # If there were only empty lists in either of the lists (this is unexpected
  # and normally shouldn't happen), then we also want to return 0.
  if not sample1 or not sample2:
    return 0.0

  # The p-value is approximately the probability of obtaining the given set
  # of good and bad values just by chance.
  _, _, p_value = ttest.WelchsTTest(sample1, sample2)
  return 100.0 * (1.0 - p_value)


class BisectResults(object):

  def __init__(self, depot_registry, source_control):
    self._depot_registry = depot_registry
    self.revision_data = {}
    self.error = None
    self._source_control = source_control

  @staticmethod
  def _FindOtherRegressions(revision_data_sorted, bad_greater_than_good):
    """Compiles a list of other possible regressions from the revision data.

    Args:
      revision_data_sorted: Sorted list of (revision, revision data) pairs.
      bad_greater_than_good: Whether the result value at the "bad" revision is
          numerically greater than the result value at the "good" revision.

    Returns:
      A list of [current_rev, previous_rev, confidence] for other places where
      there may have been a regression.
    """
    other_regressions = []
    previous_values = []
    previous_id = None
    for current_id, current_data in revision_data_sorted:
      current_values = current_data['value']
      if current_values:
        current_values = current_values['values']
        if previous_values:
          confidence = ConfidenceScore(previous_values, [current_values])
          mean_of_prev_runs = math_utils.Mean(sum(previous_values, []))
          mean_of_current_runs = math_utils.Mean(current_values)

          # Check that the potential regression is in the same direction as
          # the overall regression. If the mean of the previous runs < the
          # mean of the current runs, this local regression is in same
          # direction.
          prev_less_than_current = mean_of_prev_runs < mean_of_current_runs
          is_same_direction = (prev_less_than_current if
              bad_greater_than_good else not prev_less_than_current)

          # Only report potential regressions with high confidence.
          if is_same_direction and confidence > 50:
            other_regressions.append([current_id, previous_id, confidence])
        previous_values.append(current_values)
        previous_id = current_id
    return other_regressions

  def GetResultsDict(self):
    """Prepares and returns information about the final resulsts as a dict.

    Returns:
      A dictionary with the following fields

      'first_working_revision': First good revision.
      'last_broken_revision': Last bad revision.
      'culprit_revisions': A list of revisions, which contain the bad change
          introducing the failure.
      'other_regressions': A list of tuples representing other regressions,
          which may have occured.
      'regression_size': For performance bisects, this is a relative change of
          the mean metric value. For other bisects this field always contains
          'zero-to-nonzero'.
      'regression_std_err': For performance bisects, it is a pooled standard
          error for groups of good and bad runs. Not used for other bisects.
      'confidence': For performance bisects, it is a confidence that the good
          and bad runs are distinct groups. Not used for non-performance
          bisects.
      'revision_data_sorted': dict mapping revision ids to data about that
          revision. Each piece of revision data consists of a dict with the
          following keys:

          'passed': Represents whether the performance test was successful at
              that revision. Possible values include: 1 (passed), 0 (failed),
              '?' (skipped), 'F' (build failed).
          'depot': The depot that this revision is from (i.e. WebKit)
          'external': If the revision is a 'src' revision, 'external' contains
              the revisions of each of the external libraries.
          'sort': A sort value for sorting the dict in order of commits.

          For example:
          {
            'CL #1':
            {
              'passed': False,
              'depot': 'chromium',
              'external': None,
              'sort': 0
            }
          }
    """
    revision_data_sorted = sorted(self.revision_data.iteritems(),
                                  key = lambda x: x[1]['sort'])

    # Find range where it possibly broke.
    first_working_revision = None
    first_working_revision_index = -1
    last_broken_revision = None
    last_broken_revision_index = -1

    culprit_revisions = []
    other_regressions = []
    regression_size = 0.0
    regression_std_err = 0.0
    confidence = 0.0

    for i in xrange(len(revision_data_sorted)):
      k, v = revision_data_sorted[i]
      if v['passed'] == 1:
        if not first_working_revision:
          first_working_revision = k
          first_working_revision_index = i

      if not v['passed']:
        last_broken_revision = k
        last_broken_revision_index = i

    if last_broken_revision != None and first_working_revision != None:
      broken_means = []
      for i in xrange(0, last_broken_revision_index + 1):
        if revision_data_sorted[i][1]['value']:
          broken_means.append(revision_data_sorted[i][1]['value']['values'])

      working_means = []
      for i in xrange(first_working_revision_index, len(revision_data_sorted)):
        if revision_data_sorted[i][1]['value']:
          working_means.append(revision_data_sorted[i][1]['value']['values'])

      # Flatten the lists to calculate mean of all values.
      working_mean = sum(working_means, [])
      broken_mean = sum(broken_means, [])

      # Calculate the approximate size of the regression
      mean_of_bad_runs = math_utils.Mean(broken_mean)
      mean_of_good_runs = math_utils.Mean(working_mean)

      regression_size = 100 * math_utils.RelativeChange(mean_of_good_runs,
                                                      mean_of_bad_runs)
      if math.isnan(regression_size):
        regression_size = 'zero-to-nonzero'

      regression_std_err = math.fabs(math_utils.PooledStandardError(
          [working_mean, broken_mean]) /
          max(0.0001, min(mean_of_good_runs, mean_of_bad_runs))) * 100.0

      # Give a "confidence" in the bisect. At the moment we use how distinct the
      # values are before and after the last broken revision, and how noisy the
      # overall graph is.
      confidence = ConfidenceScore(working_means, broken_means)

      culprit_revisions = []

      cwd = os.getcwd()
      self._depot_registry.ChangeToDepotDir(
          self.revision_data[last_broken_revision]['depot'])

      if self.revision_data[last_broken_revision]['depot'] == 'cros':
        # Want to get a list of all the commits and what depots they belong
        # to so that we can grab info about each.
        cmd = ['repo', 'forall', '-c',
            'pwd ; git log --pretty=oneline --before=%d --after=%d' % (
            last_broken_revision, first_working_revision + 1)]
        output, return_code = bisect_utils.RunProcessAndRetrieveOutput(cmd)

        changes = []
        assert not return_code, ('An error occurred while running '
                                 '"%s"' % ' '.join(cmd))
        last_depot = None
        cwd = os.getcwd()
        for l in output.split('\n'):
          if l:
            # Output will be in form:
            # /path_to_depot
            # /path_to_other_depot
            # <SHA1>
            # /path_again
            # <SHA1>
            # etc.
            if l[0] == '/':
              last_depot = l
            else:
              contents = l.split(' ')
              if len(contents) > 1:
                changes.append([last_depot, contents[0]])
        for c in changes:
          os.chdir(c[0])
          info = self._source_control.QueryRevisionInfo(c[1])
          culprit_revisions.append((c[1], info, None))
      else:
        for i in xrange(last_broken_revision_index, len(revision_data_sorted)):
          k, v = revision_data_sorted[i]
          if k == first_working_revision:
            break
          self._depot_registry.ChangeToDepotDir(v['depot'])
          info = self._source_control.QueryRevisionInfo(k)
          culprit_revisions.append((k, info, v['depot']))
      os.chdir(cwd)

      # Check for any other possible regression ranges.
      other_regressions = self._FindOtherRegressions(
          revision_data_sorted, mean_of_bad_runs > mean_of_good_runs)

    return {
        'first_working_revision': first_working_revision,
        'last_broken_revision': last_broken_revision,
        'culprit_revisions': culprit_revisions,
        'other_regressions': other_regressions,
        'regression_size': regression_size,
        'regression_std_err': regression_std_err,
        'confidence': confidence,
        'revision_data_sorted': revision_data_sorted
    }
