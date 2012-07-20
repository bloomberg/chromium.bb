# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Presubmit script for Chromium browser resources.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into gcl/git cl, and see
http://www.chromium.org/developers/web-development-style-guide for the rules
we're checking against here.
"""


import os
import struct


class ResourceScaleFactors(object):
  """Verifier of image dimensions for Chromium resources.

  This class verifies the image dimensions of resources in the various
  resource subdirectories.

  Attributes:
      paths: An array of tuples giving the folders to check and their
          relevant scale factors. For example:

          [(1, 'default_100_percent'), (2, 'default_200_percent')]
  """

  def __init__(self, input_api, output_api, paths):
    """ Initializes ResourceScaleFactors with paths."""
    self.input_api = input_api
    self.output_api = output_api
    self.paths = paths

  def RunChecks(self):
    """Verifies the scale factors of resources being added or modified.

    Returns:
        An array of presubmit errors if any images were detected not
        having the correct dimensions.
    """
    def ImageSize(filename):
      with open(filename, 'rb', buffering=0) as f:
        data = f.read(24)
      assert data[:8] == '\x89PNG\r\n\x1A\n' and data[12:16] == 'IHDR'
      return struct.unpack('>ii', data[16:24])

    # TODO(flackr): This should allow some flexibility for non-integer scale
    # factors such as allowing any size between the floor and ceiling of
    # base * scale.
    def ExpectedSize(base_width, base_height, scale):
      return round(base_width * scale), round(base_height * scale)

    repository_path = self.input_api.os_path.relpath(
        self.input_api.PresubmitLocalPath(),
        self.input_api.change.RepositoryRoot())
    results = []

    # Check for affected files in any of the paths specified.
    affected_files = self.input_api.AffectedFiles(include_deletes=False)
    files = []
    for f in affected_files:
      for path_spec in self.paths:
        path_root = self.input_api.os_path.join(
            repository_path, path_spec[1])
        if (f.LocalPath().endswith('.png') and
            f.LocalPath().startswith(path_root)):
          # Only save the relative path from the resource directory.
          relative_path = self.input_api.os_path.relpath(f.LocalPath(),
              path_root)
          if relative_path not in files:
            files.append(relative_path)

    for f in files:
      base_image = self.input_api.os_path.join(self.paths[0][1], f)
      if not os.path.exists(base_image):
        results.append(self.output_api.PresubmitError(
            'Base image %s does not exist' % self.input_api.os_path.join(
            repository_path, base_image)))
        continue
      base_width, base_height = ImageSize(base_image)
      # Find all scaled versions of the base image and verify their sizes.
      for i in range(1, len(self.paths)):
        image_path = self.input_api.os_path.join(self.paths[i][1], f)
        if not os.path.exists(image_path):
          continue
        # Ensure that each image for a particular scale factor is the
        # correct scale of the base image.
        exp_width, exp_height = ExpectedSize(base_width, base_height,
            self.paths[i][0])
        width, height = ImageSize(image_path)
        if width != exp_width or height != exp_height:
          results.append(self.output_api.PresubmitError(
              'Image %s is %dx%d, expected to be %dx%d' % (
              self.input_api.os_path.join(repository_path, image_path),
              width, height, exp_width, exp_height)))
    return results
