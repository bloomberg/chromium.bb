# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import json
import logging
import os
import re
import sys

from googleapiclient import discovery
from oauth2client.client import GoogleCredentials

# NOTE: The parent directory needs to be first in sys.path to avoid conflicts
# with catapult modules that have colliding names, as catapult inserts itself
# into the path as the second element. This is an ugly and fragile hack.
sys.path.insert(0,
    os.path.join(os.path.dirname(os.path.realpath(__file__)), os.pardir,
                 os.pardir))
import controller
from cloud.common.clovis_task import ClovisTask
from cloud.common.google_instance_helper import GoogleInstanceHelper
from failure_database import FailureDatabase
from google_storage_accessor import GoogleStorageAccessor
import loading_trace
from loading_trace_database import LoadingTraceDatabase
import options


class Worker(object):
  def __init__(self, config, logger):
    """See README.md for the config format."""
    self._project_name = config['project_name']
    self._taskqueue_tag = config['taskqueue_tag']
    self._src_path = config['src_path']
    self._instance_name = config.get('instance_name')
    self._worker_log_path = config.get('worker_log_path')
    self._credentials = GoogleCredentials.get_application_default()
    self._logger = logger
    self._self_destruct = config.get('self_destruct')
    if self._self_destruct and not self._instance_name:
      self._logger.error('Self destruction requires an instance name.')

    # Separate the cloud storage path into the bucket and the base path under
    # the bucket.
    storage_path_components = config['cloud_storage_path'].split('/')
    self._bucket_name = storage_path_components[0]
    self._base_path_in_bucket = ''
    if len(storage_path_components) > 1:
      self._base_path_in_bucket = '/'.join(storage_path_components[1:])
      if not self._base_path_in_bucket.endswith('/'):
        self._base_path_in_bucket += '/'

    self._google_storage_accessor = GoogleStorageAccessor(
        credentials=self._credentials, project_name=self._project_name,
        bucket_name=self._bucket_name)

    if self._instance_name:
      trace_database_filename = 'trace_database_%s.json' % self._instance_name
      failure_database_filename = \
          'failure_database_%s.json' % self._instance_name
    else:
      trace_database_filename = 'trace_database.json'
      failure_database_filename = 'failure_dabatase.json'
    self._traces_dir = os.path.join(self._base_path_in_bucket, 'traces')
    self._trace_database_path = os.path.join(self._traces_dir,
                                             trace_database_filename)
    self._failures_dir = os.path.join(self._base_path_in_bucket, 'failures')
    self._failure_database_path = os.path.join(self._failures_dir,
                                               failure_database_filename)

    # Recover any existing trace database and failures in case the worker died.
    self._DownloadTraceDatabase()
    self._DownloadFailureDatabase()

    if self._trace_database.ToJsonDict() or self._failure_database.ToJsonDict():
      # Script is restarting after a crash, or there are already files from a
      # previous run in the directory.
      self._failure_database.AddFailure('startup_with_dirty_state')
      self._UploadFailureDatabase()

    # Initialize the global options that will be used during trace generation.
    options.OPTIONS.ParseArgs(['--local_build_dir', config['binaries_path']])

  def Start(self):
    """Main worker loop.

    Repeatedly pulls tasks from the task queue and processes them. Returns when
    the queue is empty.
    """
    task_api = discovery.build('taskqueue', 'v1beta2',
                               credentials=self._credentials)
    queue_name = 'clovis-queue'
    # Workaround for
    # https://code.google.com/p/googleappengine/issues/detail?id=10199
    project = 's~' + self._project_name

    while True:
      self._logger.debug('Fetching new task.')
      (clovis_task, task_id) = self._FetchClovisTask(project, task_api,
                                                     queue_name)
      if not clovis_task:
        break

      self._logger.info('Processing task %s' % task_id)
      self._ProcessClovisTask(clovis_task)
      self._logger.debug('Deleting task %s' % task_id)
      task_api.tasks().delete(project=project, taskqueue=queue_name,
                              task=task_id).execute()
      self._logger.info('Finished task %s' % task_id)
    self._Finalize()

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

  def _DownloadFailureDatabase(self):
    """Downloads the failure database from CloudStorage."""
    self._logger.info('Downloading failure database')
    failure_database_string = self._google_storage_accessor.DownloadAsString(
        self._failure_database_path)
    self._failure_database = FailureDatabase(failure_database_string)

  def _UploadFailureDatabase(self):
    """Uploads the failure database to CloudStorage."""
    self._logger.info('Uploading failure database')
    self._google_storage_accessor.UploadString(
        self._failure_database.ToJsonString(),
        self._failure_database_path)

  def _FetchClovisTask(self, project_name, task_api, queue_name):
    """Fetches a ClovisTask from the task queue.

    Params:
      project_name(str): The name of the Google Cloud project.
      task_api: The TaskQueue service.
      queue_name(str): The name of the task queue.

    Returns:
      (ClovisTask, str): The fetched ClovisTask and its task ID, or (None, None)
                         if no tasks are found.
    """
    response = task_api.tasks().lease(
        project=project_name, taskqueue=queue_name, numTasks=1, leaseSecs=600,
        groupByTag=True, tag=self._taskqueue_tag).execute()
    if (not response.get('items')) or (len(response['items']) < 1):
      return (None, None)  # The task queue is empty.

    google_task = response['items'][0]
    task_id = google_task['id']

    # Delete the task without processing if it already failed multiple times.
    # TODO(droger): This is a workaround for internal bug b/28442122, revisit
    # once it is fixed.
    retry_count = google_task['retry_count']
    max_retry_count = 3
    skip_task = retry_count >= max_retry_count
    if skip_task:
      task_api.tasks().delete(project=project_name, taskqueue=queue_name,
                              task=task_id).execute()

    clovis_task = ClovisTask.FromBase64(google_task['payloadBase64'])

    if retry_count > 0:
      self._failure_database.AddFailure('task_queue_retry',
                                        clovis_task.ToJsonString())
      self._UploadFailureDatabase()

    if skip_task:
      return self._FetchClovisTask(project_name, task_api, queue_name)

    return (clovis_task, task_id)

  def _Finalize(self):
    """Called before exiting."""
    self._logger.info('Done')
    # Upload the worker log.
    if self._worker_log_path:
      self._logger.info('Uploading worker log.')
      remote_log_path = os.path.join(self._base_path_in_bucket, 'worker_log')
      if self._instance_name:
        remote_log_path += '_' + self._instance_name
      self._google_storage_accessor.UploadFile(self._worker_log_path,
                                               remote_log_path)
    # Self destruct.
    if self._self_destruct:
      self._logger.info('Starting instance destruction: ' + self._instance_name)
      google_instance_helper = GoogleInstanceHelper(
          self._credentials, self._project_name, self._logger)
      success = google_instance_helper.DeleteInstance(self._taskqueue_tag,
                                                      self._instance_name)
      if not success:
        self._logger.error('Self destruction failed.')
    # Do not add anything after this line, as the instance might be killed at
    # any time.

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
    Results related to successful traces are uploaded in the _traces_dir
    directory, and failures are uploaded in the _failures_dir directory.

    Args:
      local_filename (str): Path to the local file containing the trace.
      log_filename (str): Path to the local file containing the log.
      remote_filename (str): Name of the target remote file where the trace and
                             the log (with a .log extension added) are uploaded.
      trace_metadata (dict): Metadata associated with the trace generation.
    """
    if trace_metadata['succeeded']:
      remote_trace_location = os.path.join(self._traces_dir, remote_filename)
      full_cloud_storage_path = os.path.join('gs://' + self._bucket_name,
                                             remote_trace_location)
      self._trace_database.SetTrace(full_cloud_storage_path, trace_metadata)
    else:
      remote_trace_location = os.path.join(self._failures_dir, remote_filename)
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

  def _ProcessClovisTask(self, clovis_task):
    """Processes one clovis_task."""
    if clovis_task.Action() != 'trace':
      self._logger.error('Unsupported task action: %s' % clovis_task.Action())
      return

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
    if failure_happened:
      self._UploadFailureDatabase()

if __name__ == '__main__':
  parser = argparse.ArgumentParser(
      description='ComputeEngine Worker for Clovis')
  parser.add_argument('--config', required=True,
                      help='Path to the configuration file.')
  args = parser.parse_args()

  # Configure logging.
  logging.basicConfig(level=logging.WARNING)
  worker_logger = logging.getLogger('worker')
  worker_logger.setLevel(logging.INFO)

  worker_logger.info('Reading configuration')
  with open(args.config) as config_json:
    worker = Worker(json.load(config_json), worker_logger)
    worker.Start()

