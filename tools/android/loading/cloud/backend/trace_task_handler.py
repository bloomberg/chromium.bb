# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import os
import re
import sys

from cloud.common.clovis_task import ClovisTask
import controller
from failure_database import FailureDatabase
import loading_trace
from loading_trace_database import LoadingTraceDatabase
import options


class TraceTaskHandler(object):
  """Handles 'trace' tasks."""

  def __init__(self, base_path, failure_database,
               google_storage_accessor, binaries_path, logger,
               instance_name=None):
    """Args:
      base_path(str): Base path where results are written.
      binaries_path(str): Path to the directory where Chrome executables are.
      instance_name(str, optional): Name of the ComputeEngine instance.
    """
    self._failure_database = failure_database
    self._logger = logger
    self._google_storage_accessor = google_storage_accessor
    self._base_path = base_path
    if instance_name:
      trace_database_filename = 'trace_database_%s.json' % instance_name
    else:
      trace_database_filename = 'trace_database.json'
    self._trace_database_path = os.path.join(base_path, trace_database_filename)

    # Recover any existing traces in case the worker died.
    self._DownloadTraceDatabase()
    if self._trace_database.ToJsonDict():
      # Script is restarting after a crash, or there are already files from a
      # previous run in the directory.
      self._failure_database.AddFailure(FailureDatabase.DIRTY_STATE_ERROR,
                                        'trace_database')

    # Initialize the global options that will be used during trace generation.
    options.OPTIONS.ParseArgs(['--local_build_dir', binaries_path])

  def _DownloadTraceDatabase(self):
    """Downloads the trace database from CloudStorage."""
    self._logger.info('Downloading trace database')
    trace_database_string = self._google_storage_accessor.DownloadAsString(
        self._trace_database_path) or '{}'
    self._trace_database = LoadingTraceDatabase.FromJsonString(
        trace_database_string)

  def _UploadTraceDatabase(self):
    """Uploads the trace database to CloudStorage."""
    self._logger.info('Uploading trace database')
    self._google_storage_accessor.UploadString(
        self._trace_database.ToJsonString(),
        self._trace_database_path)

  def _GenerateTrace(self, url, emulate_device, emulate_network, filename,
                     log_filename):
    """ Generates a trace.

    Args:
      url: URL as a string.
      emulate_device: Name of the device to emulate. Empty for no emulation.
      emulate_network: Type of network emulation. Empty for no emulation.
      filename: Name of the file where the trace is saved.
      log_filename: Name of the file where standard output and errors are
                    logged.

    Returns:
      A dictionary of metadata about the trace, including a 'succeeded' field
      indicating whether the trace was successfully generated.
    """
    try:
      os.remove(filename)  # Remove any existing trace for this URL.
    except OSError:
      pass  # Nothing to remove.

    if not url.startswith('http') and not url.startswith('file'):
      url = 'http://' + url

    old_stdout = sys.stdout
    old_stderr = sys.stderr

    trace_metadata = { 'succeeded' : False, 'url' : url }
    trace = None
    with open(log_filename, 'w') as sys.stdout:
      try:
        sys.stderr = sys.stdout

        # Set up the controller.
        chrome_ctl = controller.LocalChromeController()
        chrome_ctl.SetHeadless(True)
        if emulate_device:
          chrome_ctl.SetDeviceEmulation(emulate_device)
        if emulate_network:
          chrome_ctl.SetNetworkEmulation(emulate_network)

        # Record and write the trace.
        with chrome_ctl.Open() as connection:
          connection.ClearCache()
          trace = loading_trace.LoadingTrace.RecordUrlNavigation(
              url, connection, chrome_ctl.ChromeMetadata())
          trace_metadata['succeeded'] = True
          trace_metadata.update(trace.ToJsonDict()[trace._METADATA_KEY])
      except controller.ChromeControllerError as e:
        e.Dump(sys.stderr)
      except Exception as e:
        sys.stderr.write(str(e))

      if trace:
        with open(filename, 'w') as f:
          json.dump(trace.ToJsonDict(), f, sort_keys=True, indent=2)

    sys.stdout = old_stdout
    sys.stderr = old_stderr

    return trace_metadata

  def _HandleTraceGenerationResults(self, local_filename, log_filename,
                                    remote_filename, trace_metadata):
    """Updates the trace database and the failure database after a trace
    generation. Uploads the trace and the log.
    Results related to successful traces are uploaded in the 'traces' directory,
    and failures are uploaded in the 'failures' directory.

    Args:
      local_filename (str): Path to the local file containing the trace.
      log_filename (str): Path to the local file containing the log.
      remote_filename (str): Name of the target remote file where the trace and
                             the log (with a .log extension added) are uploaded.
      trace_metadata (dict): Metadata associated with the trace generation.
    """
    if trace_metadata['succeeded']:
      traces_dir = os.path.join(self._base_path, 'traces')
      remote_trace_location = os.path.join(traces_dir, remote_filename)
      full_cloud_storage_path = os.path.join(
          'gs://' + self._google_storage_accessor.BucketName(),
          remote_trace_location)
      self._trace_database.SetTrace(full_cloud_storage_path, trace_metadata)
    else:
      failures_dir = os.path.join(self._base_path, 'failures')
      remote_trace_location = os.path.join(failures_dir, remote_filename)
      self._failure_database.AddFailure('trace_collection',
                                        trace_metadata['url'])

    if os.path.isfile(local_filename):
      self._logger.debug('Uploading: %s' % remote_trace_location)
      self._google_storage_accessor.UploadFile(local_filename,
                                               remote_trace_location)
    else:
      self._logger.warning('No trace found at: ' + local_filename)

    self._logger.debug('Uploading analyze log')
    remote_log_location = remote_trace_location + '.log'
    self._google_storage_accessor.UploadFile(log_filename, remote_log_location)

  def Run(self, clovis_task):
    """Runs a 'trace' clovis_task.

    Args:
      clovis_task(ClovisTask): The task to run.

    Returns:
      boolean: True in case of success, False if a failure has been added to the
               failure database.
    """
    if clovis_task.Action() != 'trace':
      self._logger.error('Unsupported task action: %s' % clovis_task.Action())
      self._failure_database.AddFailure(FailureDatabase.CRITICAL_ERROR,
                                        'trace_task_handler_run')
      return False

    # Extract the task parameters.
    params = clovis_task.ActionParams()
    urls = params['urls']
    repeat_count = params.get('repeat_count', 1)
    emulate_device = params.get('emulate_device')
    emulate_network = params.get('emulate_network')

    log_filename = 'analyze.log'
    # Avoid special characters in storage object names
    pattern = re.compile(r"[#\?\[\]\*/]")

    failure_happened = False
    success_happened = False

    while len(urls) > 0:
      url = urls.pop()
      local_filename = pattern.sub('_', url)
      for repeat in range(repeat_count):
        self._logger.debug('Generating trace for URL: %s' % url)
        trace_metadata = self._GenerateTrace(
            url, emulate_device, emulate_network, local_filename, log_filename)
        if trace_metadata['succeeded']:
          success_happened = True
        else:
          self._logger.warning('Trace generation failed for URL: %s' % url)
          failure_happened = True
        remote_filename = os.path.join(local_filename, str(repeat))
        self._HandleTraceGenerationResults(
            local_filename, log_filename, remote_filename, trace_metadata)

    if success_happened:
      self._UploadTraceDatabase()

    return not failure_happened

