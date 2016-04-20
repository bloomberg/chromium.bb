# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import json
import logging
import os
import re
import sys
import time

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
    self._credentials = GoogleCredentials.get_application_default()
    self._logger = logger

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

    self._traces_dir = os.path.join(self._base_path_in_bucket, 'traces')
    self._trace_database_path = os.path.join(
        self._traces_dir,
        config.get('trace_database_filename', 'trace_database.json'))

    # Recover any existing trace database in case the worker died.
    self._DownloadTraceDatabase()

    # Initialize the global options that will be used during trace generation.
    options.OPTIONS.ParseArgs([])
    options.OPTIONS.local_binary = config['chrome_path']

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
        if self._trace_database.ToJsonDict():
          self._logger.info('No remaining tasks in the queue.')
          break
        else:
          delay_seconds = 60
          self._logger.info(
              'Nothing in the queue, retrying in %i seconds.' % delay_seconds)
          time.sleep(delay_seconds)
          continue

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
    trace_database_dict = json.loads(trace_database_string)
    self._trace_database = LoadingTraceDatabase(trace_database_dict)

  def _UploadTraceDatabase(self):
    """Uploads the trace database to CloudStorage."""
    self._logger.info('Uploading trace database')
    self._google_storage_accessor.UploadString(
        json.dumps(self._trace_database.ToJsonDict(), indent=2),
        self._trace_database_path)

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
        project=project_name, taskqueue=queue_name, numTasks=1, leaseSecs=180,
        groupByTag=True, tag=self._taskqueue_tag).execute()
    if (not response.get('items')) or (len(response['items']) < 1):
      return (None, None)

    google_task = response['items'][0]
    task_id = google_task['id']
    clovis_task = ClovisTask.FromBase64(google_task['payloadBase64'])
    return (clovis_task, task_id)

  def _Finalize(self):
    """Called before exiting."""
    # TODO(droger): Implement automatic instance destruction.
    self._logger.info('Done')

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
        with chrome_ctl.OpenWithRedirection(sys.stdout,
                                            sys.stderr) as connection:
          connection.ClearCache()
          trace = loading_trace.LoadingTrace.RecordUrlNavigation(
              url, connection, chrome_ctl.ChromeMetadata())
          trace_metadata['succeeded'] = True
          trace_metadata.update(trace.ToJsonDict()[trace._METADATA_KEY])
      except Exception as e:
        sys.stderr.write(str(e))

      if trace:
        with open(filename, 'w') as f:
          json.dump(trace.ToJsonDict(), f, sort_keys=True, indent=2)

    sys.stdout = old_stdout
    sys.stderr = old_stderr

    return trace_metadata

  def _ProcessClovisTask(self, clovis_task):
    """Processes one clovis_task."""
    if clovis_task.Action() != 'trace':
      self._logger.error('Unsupported task action: %s' % clovis_task.Action())
      return

    # Extract the task parameters.
    params = clovis_task.Params()
    urls = params['urls']
    repeat_count = params.get('repeat_count', 1)
    emulate_device = params.get('emulate_device')
    emulate_network = params.get('emulate_network')

    failures_dir = os.path.join(self._base_path_in_bucket, 'failures')
    # TODO(blundell): Fix this up.
    logs_dir = os.path.join(self._base_path_in_bucket, 'analyze_logs')
    log_filename = 'analyze.log'
    # Avoid special characters in storage object names
    pattern = re.compile(r"[#\?\[\]\*/]")

    while len(urls) > 0:
      url = urls.pop()
      local_filename = pattern.sub('_', url)
      for repeat in range(repeat_count):
        self._logger.debug('Generating trace for URL: %s' % url)
        remote_filename = os.path.join(local_filename, str(repeat))
        trace_metadata = self._GenerateTrace(
            url, emulate_device, emulate_network, local_filename, log_filename)
        if trace_metadata['succeeded']:
          self._logger.debug('Uploading: %s' % remote_filename)
          remote_trace_location = os.path.join(self._traces_dir,
                                               remote_filename)
          self._google_storage_accessor.UploadFile(local_filename,
                                                   remote_trace_location)
          full_cloud_storage_path = os.path.join('gs://' + self._bucket_name,
                                                 remote_trace_location)
          self._trace_database.SetTrace(full_cloud_storage_path, trace_metadata)
        else:
          self._logger.warning('Trace generation failed for URL: %s' % url)
          if os.path.isfile(local_filename):
            self._google_storage_accessor.UploadFile(
                local_filename, os.path.join(failures_dir, remote_filename))
        self._logger.debug('Uploading analyze log')
        self._google_storage_accessor.UploadFile(
            log_filename, os.path.join(logs_dir, remote_filename))
    self._UploadTraceDatabase()

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

