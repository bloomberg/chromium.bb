# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import glob
import itertools
import io
import logging
import multiprocessing
import os
from PIL import Image
import requests
import shutil
import subprocess
import sys
import tempfile

import parameter_set

CHROMIUM_SRC_DIR = os.path.realpath(
    os.path.join(os.path.dirname(__file__), '..', '..', '..', '..'))
GOLDCTL_PATHS = [
    os.path.join(CHROMIUM_SRC_DIR, 'tools', 'skia_goldctl', 'linux', 'goldctl'),
    os.path.join(CHROMIUM_SRC_DIR, 'tools', 'skia_goldctl', 'mac', 'goldctl'),
    os.path.join(CHROMIUM_SRC_DIR, 'tools', 'skia_goldctl', 'win',
                 'goldctl.exe'),
]


class BaseParameterOptimizer(object):
  """Abstract base class for running a parameter optimization for a test."""
  MIN_EDGE_THRESHOLD = 0
  MAX_EDGE_THRESHOLD = 255
  MIN_MAX_DIFF = 0
  MIN_DELTA_THRESHOLD = 0
  # 4 channels, ranging from 0-255 each.
  MAX_DELTA_THRESHOLD = 255 * 4

  def __init__(self, args, test_name):
    """
    Args:
      args: The parse arguments from an argparse.ArgumentParser.
      test_name: The name of the test to optimize.
    """
    self._args = args
    self._test_name = test_name
    self._goldctl_binary = None
    self._working_dir = None
    self._expectations = None
    self._gold_url = 'https://%s-gold.skia.org' % args.gold_instance
    # A map of strings, denoting a resolution or trace, to an iterable of
    # strings, denoting images that are that dimension or belong to that
    # trace.
    self._images = {}
    self._VerifyArgs()

  @classmethod
  def AddArguments(cls, parser):
    """Add optimizer-specific arguments to the parser.

    Args:
      parser: An argparse.ArgumentParser instance.

    Returns:
      A 3-tuple (common_group, sobel_group, fuzzy_group). All three are
      argument groups of |parser| corresponding to arguments for any sort of
      inexact matching algorithm, arguments specific to Sobel filter matching,
      and arguments specific to fuzzy matching.
    """
    common_group = parser.add_argument_group('Common Arguments')
    common_group.add_argument(
        '--test',
        required=True,
        action='append',
        dest='test_names',
        help='The name of a test to find parameter values for, as reported in '
        'the Skia Gold UI. Can be passed multiple times to run optimizations '
        'for multiple tests.')
    common_group.add_argument(
        '--gold-instance',
        default='chrome-gpu',
        help='The Skia Gold instance to interact with.')
    common_group.add_argument(
        '--corpus',
        default='chrome-gpu',
        help='The corpus within the instance to interact with.')
    common_group.add_argument(
        '--target-success-percent',
        default=100,
        type=float,
        help='The percentage of comparisons that need to succeed in order for '
        'a set of parameters to be considered good.')
    common_group.add_argument(
        '--no-cleanup',
        action='store_true',
        default=False,
        help="Don't clean up the temporary files left behind by the "
        'optimization process.')
    common_group.add_argument(
        '--group-images-by-resolution',
        action='store_true',
        default=False,
        help='Group images for comparison based on resolution instead of by '
        'Gold trace. This will likely add some noise, as some comparisons will '
        'be made that Gold would not consider, but this has the benefit of '
        'optimizing over all historical data instead of only over data in '
        'the past several hundred commits. Note that this will likely '
        'result in a significantly longer runtime.')

    sobel_group = parser.add_argument_group(
        'Sobel Arguments',
        'To disable Sobel functionality, set both min and max edge thresholds '
        'to 255.')
    sobel_group.add_argument(
        '--min-edge-threshold',
        default=10,
        type=int,
        help='The minimum value to consider for the Sobel edge threshold. '
        'Lower values result in more of the image being blacked out before '
        'comparison.')
    sobel_group.add_argument(
        '--max-edge-threshold',
        default=255,
        type=int,
        help='The maximum value to consider for the Sobel edge threshold. '
        'Higher values result in less of the image being blacked out before '
        'comparison.')

    fuzzy_group = parser.add_argument_group(
        'Fuzzy Arguments',
        'To disable Fuzzy functionality, set min/max for both parameters to 0')
    fuzzy_group.add_argument(
        '--min-max-different-pixels',
        dest='min_max_diff',
        default=0,
        type=int,
        help='The minimum value to consider for the maximum number of '
        'different pixels. Lower values result in less fuzzy comparisons being '
        'allowed.')
    fuzzy_group.add_argument(
        '--max-max-different-pixels',
        dest='max_max_diff',
        default=50,
        type=int,
        help='The maximum value to consider for the maximum number of '
        'different pixels. Higher values result in more fuzzy comparisons '
        'being allowed.')
    fuzzy_group.add_argument(
        '--min-delta-threshold',
        default=0,
        type=int,
        help='The minimum value to consider for the per-channel delta sum '
        'threshold. Lower values result in less fuzzy comparisons being '
        'allowed.')
    fuzzy_group.add_argument(
        '--max-delta-threshold',
        default=30,
        type=int,
        help='The maximum value to consider for the per-channel delta sum '
        'threshold. Higher values result in more fuzzy comparisons being '
        'allowed.')

    return common_group, sobel_group, fuzzy_group

  def _VerifyArgs(self):
    """Verifies that the provided arguments are valid for an optimizer."""
    assert self._args.target_success_percent > 0
    assert self._args.target_success_percent <= 100

    assert self._args.min_edge_threshold >= self.MIN_EDGE_THRESHOLD
    assert self._args.max_edge_threshold <= self.MAX_EDGE_THRESHOLD
    assert self._args.min_edge_threshold <= self._args.max_edge_threshold

    assert self._args.min_max_diff >= self.MIN_MAX_DIFF
    assert self._args.min_max_diff <= self._args.max_max_diff
    assert self._args.min_delta_threshold >= self.MIN_DELTA_THRESHOLD
    assert self._args.max_delta_threshold <= self.MAX_DELTA_THRESHOLD
    assert self._args.min_delta_threshold <= self._args.max_delta_threshold

  def RunOptimization(self):
    """Runs an optimization for whatever test and parameters were supplied.

    The results should be printed to stdout when they are available.
    """
    self._working_dir = tempfile.mkdtemp()
    try:
      self._DownloadData()

      # Do a preliminary test to make sure that the most permissive
      # parameters can succeed.
      logging.info('Verifying initial parameters')
      success, num_pixels, max_delta = self._RunComparisonForParameters(
          self._GetMostPermissiveParameters())
      if not success:
        raise RuntimeError(
            'Most permissive parameters did not result in a comparison '
            'success. Try loosening parameters or lowering target success '
            'percent. Max differing pixels: %d, max delta: %s' % (num_pixels,
                                                                  max_delta))

      self._RunOptimizationImpl()

    finally:
      if not self._args.no_cleanup:
        shutil.rmtree(self._working_dir)
        # Cleanup files left behind by "goldctl match"
        for f in glob.iglob(os.path.join(tempfile.gettempdir(), 'goldctl-*')):
          shutil.rmtree(f)

  def _RunOptimizationImpl(self):
    """Runs the algorithm-specific optimization code for an optimizer."""
    raise NotImplementedError()

  def _GetMostPermissiveParameters(self):
    return parameter_set.ParameterSet(self._args.max_max_diff,
                                      self._args.max_delta_threshold,
                                      self._args.min_edge_threshold)

  def _DownloadData(self):
    """Downloads all the necessary data for a test."""
    assert self._working_dir
    logging.info('Downloading images')
    if self._args.group_images_by_resolution:
      self._DownloadExpectations('%s/json/expectations' % self._gold_url)
      self._DownloadImagesForResolutionGrouping()
    else:
      self._DownloadExpectations(
          '%s/json/debug/digestsbytestname/%s/%s' %
          (self._gold_url, self._args.corpus, self._test_name))
      self._DownloadImagesForTraceGrouping()
    for grouping, digests in self._images.iteritems():
      logging.info('Found %d images for group %s', len(digests), grouping)
      logging.debug('Digests: %r', digests)

  def _DownloadExpectations(self, url):
    """Downloads the expectation JSON from Gold into memory."""
    logging.info('Downloading expectations JSON')
    r = requests.get(url)
    assert r.status_code == 200
    self._expectations = r.json()

  def _DownloadImagesForResolutionGrouping(self):
    """Downloads all the positive images for a test to disk.

    Images are grouped by resolution.
    """
    assert self._expectations
    test_expectations = self._expectations.get('master', {}).get(
        self._test_name, {})
    positive_digests = [
        digest for digest, val in test_expectations.items() if val == 1
    ]
    if not positive_digests:
      raise RuntimeError('Failed to find any positive digests for test %s',
                         self._test_name)
    for digest in positive_digests:
      content = self._DownloadImageWithDigest(digest)
      image = Image.open(io.BytesIO(content))
      self._images.setdefault('%dx%d' % (image.size[0], image.size[1]),
                              []).append(digest)

  def _DownloadImagesForTraceGrouping(self):
    """Download all recent positive images for a test to disk.

    Images are grouped by Skia Gold trace ID, i.e. each hardware/software
    combination is a separate group.
    """
    assert self._expectations
    # The downloaded trace data maps a trace ID (string) to a list of digests.
    # The digests can be empty (which we don't care about) or duplicated, so
    # convert to a set and filter out the empty strings.
    filtered_traces = {}
    for trace, digests in self._expectations.iteritems():
      filtered_digests = set(digests)
      filtered_digests.discard('')
      if not filtered_digests:
        logging.warning(
            'Failed to find any positive digests for test %s and trace %s. '
            'This is likely due to the trace being old.', self._test_name,
            trace)
      filtered_traces[trace] = filtered_digests
      for digest in filtered_digests:
        self._DownloadImageWithDigest(digest)
    self._images = filtered_traces

  def _DownloadImageWithDigest(self, digest):
    """Downloads an image with the given digest and saves it to disk.

    Args:
      digest: The md5 digest of the image to download.

    Returns:
      A copy of the image content that was written to disk as bytes.
    """
    logging.debug('Downloading image %s.png', digest)
    r = requests.get('%s/img/images/%s.png' % (self._gold_url, digest))
    assert r.status_code == 200
    with open(self._GetImagePath(digest), 'wb') as outfile:
      outfile.write(r.content)
    return r.content

  def _GetImagePath(self, digest):
    """Gets a filepath to an image based on digest.

    Args:
      digest: The md5 digest of the image, as provided by Gold.

    Returns:
      A string containing a filepath to where the image should be on disk.
    """
    return os.path.join(self._working_dir, '%s.png' % digest)

  def _GetGoldctlBinary(self):
    """Gets the filepath to the goldctl binary to use.

    Returns:
      A string containing a filepath to the goldctl binary to use.
    """
    if not self._goldctl_binary:
      for path in GOLDCTL_PATHS:
        if os.path.isfile(path):
          self._goldctl_binary = path
          break
      if not self._goldctl_binary:
        raise RuntimeError(
            'Could not find goldctl binary. Checked %s' % GOLDCTL_PATHS)
    return self._goldctl_binary

  def _RunComparisonForParameters(self, parameters):
    """Runs a comparison for all image combinations using some parameters.

    Args:
      parameters: A parameter_set.ParameterSet instance containing parameters to
          use.

    Returns:
      A 3-tuple (success, num_pixels, max_diff). |success| is a boolean
      denoting whether enough comparisons succeeded to meet the desired success
      percentage. |num_pixels| is an int denoting the maximum number of pixels
      that did not match across all comparisons. |max_delta| is the maximum
      per-channel delta sum across all comparisons.
    """
    logging.debug('Running comparison for parameters: %s', parameters)
    num_attempts = 0
    num_successes = 0
    max_num_pixels = -1
    max_max_delta = -1

    process_pool = multiprocessing.Pool()
    for resolution, digest_list in self._images.iteritems():
      logging.debug('Resolution/trace: %s, digests: %s', resolution,
                    digest_list)
      cmds = [
          self._GenerateComparisonCmd(l, r, parameters)
          for (l, r) in itertools.combinations(digest_list, 2)
      ]
      results = process_pool.map(RunCommandAndExtractData, cmds)
      for (success, num_pixels, max_delta) in results:
        num_attempts += 1
        if success:
          num_successes += 1
        max_num_pixels = max(num_pixels, max_num_pixels)
        max_max_delta = max(max_delta, max_max_delta)

    # This could potentially happen if run on a test where there's only one
    # positive image per resolution/trace.
    if num_attempts == 0:
      num_attempts = 1
      num_successes = 1
    success_percent = float(num_successes) * 100 / num_attempts
    logging.debug('Success percent: %s', success_percent)
    logging.debug('target success percent: %s',
                  self._args.target_success_percent)
    successful = success_percent >= self._args.target_success_percent
    logging.debug(
        'Successful: %s, Max different pixels: %d, Max per-channel delta sum: '
        '%d', successful, max_num_pixels, max_max_delta)
    return successful, max_num_pixels, max_max_delta

  def _GenerateComparisonCmd(self, left_digest, right_digest, parameters):
    """Generates a comparison command for the given arguments.

    The returned command can be passed directly to a subprocess call.

    Args:
      left_digest: The first/left image digest to compare.
      right_digest: The second/right image digest to compare.
      parameters: A parameter_set.ParameterSet instance containing the
          parameters to use for image comparison.

    Returns:
      A list of strings specifying a goldctl command to compare |left_digest|
      to |right_digest| using the parameters in |parameters|.
    """
    cmd = [
        self._GetGoldctlBinary(),
        'match',
        self._GetImagePath(left_digest),
        self._GetImagePath(right_digest),
        '--algorithm',
        'sobel',
    ] + parameters.AsList()
    return cmd


def RunCommandAndExtractData(cmd):
  """Runs a comparison command and extracts data from it.

  This is outside of the parameter optimizers because it is meant to be run via
  multiprocessing.Pool.map(), which does not play nice with class methods since
  they can't be easily pickled.

  Args:
    cmd: A list of strings containing the command to run.

  Returns:
    A 3-tuple (success, num_pixels, max_delta). |success| is a boolean denoting
    whether the comparison succeeded or not. |num_pixels| is an int denoting
    the number of pixels that did not match. |max_delta| is the maximum
    per-channel delta sum in the comparison.
  """
  output = subprocess.check_output(cmd, stderr=subprocess.STDOUT)
  success = False
  num_pixels = 0
  max_delta = 0
  for line in output.splitlines():
    if 'Images match.' in line:
      success = True
    if 'Number of different pixels' in line:
      num_pixels = int(line.split(':')[1])
    if 'Maximum per-channel delta sum' in line:
      max_delta = int(line.split(':')[1])
  logging.debug('Result for %r: success: %s, num_pixels: %d, max_delta: %d',
                cmd, success, num_pixels, max_delta)
  return success, num_pixels, max_delta
