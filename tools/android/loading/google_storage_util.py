# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Contains utility functions for interacting with Google Storage."""

import subprocess

def ReadFromGoogleStorage(path):
  """Given a Google Storage path, returns the contents of the file at that path
     as a string. Will fail if the user does not have authorization to access
     the path or if the path does not exist. To gain authorization, follow the
     instructions for installing gsutil and setting up credentials to access
     protected data that are on this page:
     https://cloud.google.com/storage/docs/gsutil_install"""
  # TODO(blundell): Change this to use the gcloud Python module once
  # https://github.com/GoogleCloudPlatform/gcloud-python/issues/14360 is fixed.
  contents = subprocess.check_output(["gsutil", "cat", path])
  return contents
