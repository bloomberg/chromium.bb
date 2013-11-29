# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import re
import sys


def LoadSupport(input_api):
  if 'cloud_storage' not in globals():
    # Avoid leaking changes to global sys.path.
    _old_sys_path = sys.path
    try:
      telemetry_path = os.path.join(os.path.dirname(os.path.dirname(
          input_api.PresubmitLocalPath())), 'telemetry')
      sys.path = [telemetry_path] + sys.path
      from telemetry.page import cloud_storage
      globals()['cloud_storage'] = cloud_storage
    finally:
      sys.path = _old_sys_path

  return globals()['cloud_storage']


def _SyncFilesToCloud(input_api, output_api):
  """Searches for .sha1 files and uploads them to Cloud Storage.

  It validates all the hashes and skips upload if not necessary.
  """

  cloud_storage = LoadSupport(input_api)

  # Look in both buckets, in case the user uploaded the file manually. But this
  # script focuses on WPR archives, so it only uploads to the internal bucket.
  hashes_in_cloud_storage = cloud_storage.List(cloud_storage.INTERNAL_BUCKET)
  hashes_in_cloud_storage += cloud_storage.List(cloud_storage.PUBLIC_BUCKET)

  results = []
  for affected_file in input_api.AffectedFiles(include_deletes=False):
    hash_path = affected_file.AbsoluteLocalPath()
    file_path, extension = os.path.splitext(hash_path)
    if extension != '.sha1':
      continue

    with open(hash_path, 'rb') as f:
      file_hash = f.read(1024).rstrip()
    if file_hash in hashes_in_cloud_storage:
      results.append(output_api.PresubmitNotifyResult(
          'File already in Cloud Storage, skipping upload: %s' % hash_path))
      continue

    if not re.match('^([A-Za-z0-9]{40})$', file_hash):
      results.append(output_api.PresubmitError(
          'Hash file does not contain a valid SHA-1 hash: %s' % hash_path))
      continue
    if not os.path.exists(file_path):
      results.append(output_api.PresubmitError(
          'Hash file exists, but file not found: %s' % hash_path))
      continue
    if cloud_storage.GetHash(file_path) != file_hash:
      results.append(output_api.PresubmitError(
          'Hash file does not match file\'s actual hash: %s' % hash_path))
      continue

    try:
      cloud_storage.Insert(cloud_storage.INTERNAL_BUCKET, file_hash, file_path)
      results.append(output_api.PresubmitNotifyResult(
          'Uploaded file to Cloud Storage: %s' % hash_path))
    except cloud_storage.CloudStorageError, e:
      results.append(output_api.PresubmitError(
          'Unable to upload to Cloud Storage: %s\n\n%s' % (hash_path, e)))

  return results


def CheckChangeOnUpload(input_api, output_api):
  return _SyncFilesToCloud(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
  return _SyncFilesToCloud(input_api, output_api)
