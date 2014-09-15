# Copyright 2013 The Chromium Authors. All rights reserved.
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
      from telemetry.util import cloud_storage
      globals()['cloud_storage'] = cloud_storage
    finally:
      sys.path = _old_sys_path

  return globals()['cloud_storage']


def _GetFilesNotInCloud(input_api):
  """Searches for .sha1 files and uploads them to Cloud Storage.

  It validates all the hashes and skips upload if not necessary.
  """
  hash_paths = []
  for affected_file in input_api.AffectedFiles(include_deletes=False):
    hash_path = affected_file.AbsoluteLocalPath()
    _, extension = os.path.splitext(hash_path)
    if extension == '.sha1':
      hash_paths.append(hash_path)
  if not hash_paths:
    return []

  cloud_storage = LoadSupport(input_api)

  # Look in both buckets, in case the user uploaded the file manually. But this
  # script focuses on WPR archives, so it only uploads to the internal bucket.
  hashes_in_cloud_storage = cloud_storage.List(cloud_storage.PUBLIC_BUCKET)
  try:
    hashes_in_cloud_storage += cloud_storage.List(cloud_storage.INTERNAL_BUCKET)
  except (cloud_storage.PermissionError, cloud_storage.CredentialsError):
    pass

  files = []
  for hash_path in hash_paths:
    file_hash = cloud_storage.ReadHash(hash_path)
    if file_hash not in hashes_in_cloud_storage:
      files.append((hash_path, file_hash))

  return files


def _SyncFilesToCloud(input_api, output_api):
  """Searches for .sha1 files and uploads them to Cloud Storage.

  It validates all the hashes and skips upload if not necessary.
  """

  cloud_storage = LoadSupport(input_api)

  results = []
  for hash_path, file_hash in _GetFilesNotInCloud(input_api):
    file_path, _ = os.path.splitext(hash_path)

    if not re.match('^([A-Za-z0-9]{40})$', file_hash):
      results.append(output_api.PresubmitError(
          'Hash file does not contain a valid SHA-1 hash: %s' % hash_path))
      continue
    if not os.path.exists(file_path):
      results.append(output_api.PresubmitError(
          'Hash file exists, but file not found: %s' % hash_path))
      continue
    if cloud_storage.CalculateHash(file_path) != file_hash:
      results.append(output_api.PresubmitError(
          'Hash file does not match file\'s actual hash: %s' % hash_path))
      continue

    try:
      bucket_aliases_string = ', '.join(cloud_storage.BUCKET_ALIASES)
      bucket_input = raw_input(
          'Uploading to Cloud Storage: %s\n'
          'Which bucket should this go in? (%s) '
          % (file_path, bucket_aliases_string)).lower()
      bucket = cloud_storage.BUCKET_ALIASES.get(bucket_input, None)
      if not bucket:
        results.append(output_api.PresubmitError(
            '"%s" was not one of %s' % (bucket_input, bucket_aliases_string)))
        return results

      cloud_storage.Insert(bucket, file_hash, file_path)
      results.append(output_api.PresubmitNotifyResult(
          'Uploaded file to Cloud Storage: %s' % file_path))
    except cloud_storage.CloudStorageError, e:
      results.append(output_api.PresubmitError(
          'Unable to upload to Cloud Storage: %s\n\n%s' % (file_path, e)))

  return results


def _VerifyFilesInCloud(input_api, output_api):
  """Searches for .sha1 files and uploads them to Cloud Storage.

  It validates all the hashes and skips upload if not necessary.
  """
  results = []
  for hash_path, _ in _GetFilesNotInCloud(input_api):
    results.append(output_api.PresubmitError(
        'Attemping to commit hash file, but corresponding '
        'data file is not in Cloud Storage: %s' % hash_path))
  return results


def _IsNewJsonPageSet(affected_file):
  return (affected_file.Action() == 'A' and
          'page_sets/data/' not in affected_file.AbsoluteLocalPath()
          and affected_file.AbsoluteLocalPath().endswith('.json'))


def _GetNewJsonPageSets(input_api):
  return input_api.AffectedFiles(file_filter=_IsNewJsonPageSet)

def CheckChangeOnUpload(input_api, output_api):
  results = _SyncFilesToCloud(input_api, output_api)
  return results


def CheckChangeOnCommit(input_api, output_api):
  results = _VerifyFilesInCloud(input_api, output_api)
  return results
