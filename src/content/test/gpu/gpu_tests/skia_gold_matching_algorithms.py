# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Classes related to the possible matching algorithms for Skia Gold."""


class Parameters(object):
  """Constants for Skia Gold algorithm parameters.

  These correspond to the constants defined in goldctl's
  imgmatching/constants.go.
  """
  # The max number of pixels in an image that can differ and still allow the
  # fuzzy comparison to pass.
  MAX_DIFFERENT_PIXELS = 'fuzzy_max_different_pixels'
  # The max RGBA difference between two pixels that is still considered valid.
  # For example, if a pixel differs by (1, 2, 3, 0), then the threshold would
  # need to be 6 or higher in order for the fuzzy comparison to pass.
  PIXEL_DELTA_THRESHOLD = 'fuzzy_pixel_delta_threshold'
  # A number in the range [0, 255] specifying how much of an image should be
  # blacked out when using a Sobel filter. 0 results in the most pixels being
  # blacked out, while 255 results in no pixels being blacked out, i.e. no
  # Sobel filter functionality.
  EDGE_THRESHOLD = 'sobel_edge_threshold'


class SkiaGoldMatchingAlgorithm(object):
  ALGORITHM_KEY = 'image_matching_algorithm'
  """Abstract base class for all algorithms."""

  def GetCmdline(self):
    """Gets command line parameters for the algorithm.

    Returns:
      A list of strings representing the algorithm's parameters. The returned
      list is suitable for extending an existing goldctl imgtest add
      commandline, which will cause goldctl to use the specified algorithm
      instead of the default.
    """
    return _GenerateOptionalKey(SkiaGoldMatchingAlgorithm.ALGORITHM_KEY,
                                self.Name())

  def Name(self):
    """Returns a string representation of the algorithm."""
    raise NotImplementedError()


class ExactMatchingAlgorithm(SkiaGoldMatchingAlgorithm):
  """Class for the default exact matching algorithm in Gold."""

  def GetCmdline(self):
    return []

  def Name(self):
    return 'exact'


class FuzzyMatchingAlgorithm(SkiaGoldMatchingAlgorithm):
  """Class for the fuzzy matching algorithm in Gold."""

  def __init__(self, max_different_pixels, pixel_delta_threshold):
    super(FuzzyMatchingAlgorithm, self).__init__()
    assert int(max_different_pixels) >= 0
    assert int(pixel_delta_threshold) >= 0
    self._max_different_pixels = max_different_pixels
    self._pixel_delta_threshold = pixel_delta_threshold

  def GetCmdline(self):
    retval = super(FuzzyMatchingAlgorithm, self).GetCmdline()
    retval.extend(
        _GenerateOptionalKey(Parameters.MAX_DIFFERENT_PIXELS,
                             self._max_different_pixels))
    retval.extend(
        _GenerateOptionalKey(Parameters.PIXEL_DELTA_THRESHOLD,
                             self._pixel_delta_threshold))
    return retval

  def Name(self):
    return 'fuzzy'


class SobelMatchingAlgorithm(FuzzyMatchingAlgorithm):
  """Class for the Sobel filter matching algorithm in Gold.

  Technically a superset of the fuzzy matching algorithm.
  """

  def __init__(self, max_different_pixels, pixel_delta_threshold,
               edge_threshold):
    super(SobelMatchingAlgorithm, self).__init__(max_different_pixels,
                                                 pixel_delta_threshold)
    assert int(edge_threshold) >= 0
    assert int(edge_threshold) <= 255
    if edge_threshold == 255:
      raise RuntimeError(
          'Sobel matching with edge threshold set to 255 is the same as fuzzy '
          'matching.')
    self._edge_threshold = edge_threshold

  def GetCmdline(self):
    retval = super(SobelMatchingAlgorithm, self).GetCmdline()
    retval.extend(
        _GenerateOptionalKey(Parameters.EDGE_THRESHOLD, self._edge_threshold))
    return retval

  def Name(self):
    return 'sobel'


def _GenerateOptionalKey(key, value):
  return ['--add-test-optional-key', '%s:%s' % (key, value)]
