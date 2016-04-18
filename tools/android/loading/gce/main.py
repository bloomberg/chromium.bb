# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import os
import re
import threading
import time
import subprocess
import sys

# NOTE: The parent directory needs to be first in sys.path to avoid conflicts
# with catapult modules that have colliding names, as catapult inserts itself
# into the path as the second element. This is an ugly and fragile hack.
sys.path.insert(0,
    os.path.join(os.path.dirname(os.path.realpath(__file__)), os.pardir))
import controller
from google_storage_accessor import GoogleStorageAccessor
import loading_trace
from loading_trace_database import LoadingTraceDatabase
import options


class ServerApp(object):
  """Simple web server application, collecting traces and writing them in
  Google Cloud Storage.
  """

  def __init__(self, configuration_file):
    """|configuration_file| is a path to a file containing JSON as described in
    README.md.
    """
    self._tasks = []  # List of remaining tasks, only modified by _thread.
    self._failed_tasks = []  # Failed tasks, only modified by _thread.
    self._thread = None
    self._tasks_lock = threading.Lock()  # Protects _tasks and _failed_tasks.
    self._initial_task_count = -1
    self._start_time = None
    print 'Reading configuration'
    with open(configuration_file) as config_json:
       config = json.load(config_json)

       # Separate the cloud storage path into the bucket and the base path under
       # the bucket.
       storage_path_components = config['cloud_storage_path'].split('/')
       self._bucket_name = storage_path_components[0]
       self._base_path_in_bucket = ''
       if len(storage_path_components) > 1:
         self._base_path_in_bucket = '/'.join(storage_path_components[1:])
         if not self._base_path_in_bucket.endswith('/'):
           self._base_path_in_bucket += '/'

       self._src_path = config['src_path']
       self._google_storage_accessor = GoogleStorageAccessor(
           project_name=config['project_name'], bucket_name=self._bucket_name)

    # Initialize the global options that will be used during trace generation.
    options.OPTIONS.ParseArgs([])
    options.OPTIONS.local_binary = config['chrome_path']

  def _IsProcessingTasks(self):
    """Returns True if the application is currently processing tasks."""
    return self._thread is not None and self._thread.is_alive()

  def _GenerateTrace(self, url, emulate_device, emulate_network, filename,
                     log_filename):
    """ Generates a trace on _thread.

    Args:
      url: URL as a string.
      emulate_device: Name of the device to emulate. Empty for no emulation.
      emulate_network: Type of network emulation. Empty for no emulation.
      filename: Name of the file where the trace is saved.
      log_filename: Name of the file where standard output and errors are logged

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

  def _GetCurrentTaskCount(self):
    """Returns the number of remaining tasks. Thread safe."""
    self._tasks_lock.acquire()
    task_count = len(self._tasks)
    self._tasks_lock.release()
    return task_count

  def _ProcessTasks(self, tasks, repeat_count, emulate_device, emulate_network):
    """Iterates over _task, generating a trace for each of them. Uploads the
    resulting traces to Google Cloud Storage.  Runs on _thread.

    Args:
      tasks: The list of URLs to process.
      repeat_count: The number of traces generated for each URL.
      emulate_device: Name of the device to emulate. Empty for no emulation.
      emulate_network: Type of network emulation. Empty for no emulation.
    """
    # The main thread might be reading the task lists, take the lock to modify.
    self._tasks_lock.acquire()
    self._tasks = tasks
    self._failed_tasks = []
    self._tasks_lock.release()
    failures_dir = self._base_path_in_bucket + 'failures/'
    traces_dir = self._base_path_in_bucket + 'traces/'

    trace_database = LoadingTraceDatabase({})

    # TODO(blundell): Fix this up.
    logs_dir = self._base_path_in_bucket + 'analyze_logs/'
    log_filename = 'analyze.log'
    # Avoid special characters in storage object names
    pattern = re.compile(r"[#\?\[\]\*/]")
    while len(self._tasks) > 0:
      url = self._tasks[-1]
      local_filename = pattern.sub('_', url)
      for repeat in range(repeat_count):
        print 'Generating trace for URL: %s' % url
        remote_filename = local_filename + '/' + str(repeat)
        trace_metadata = self._GenerateTrace(
            url, emulate_device, emulate_network, local_filename, log_filename)
        if trace_metadata['succeeded']:
          print 'Uploading: %s' % remote_filename
          remote_trace_location = traces_dir + remote_filename
          self._google_storage_accessor.UploadFile(local_filename,
                                           remote_trace_location)
          full_cloud_storage_path = ('gs://' + self._bucket_name + '/' +
              remote_trace_location)
          trace_database.AddTrace(full_cloud_storage_path, trace_metadata)
        else:
          print 'Trace generation failed for URL: %s' % url
          self._tasks_lock.acquire()
          self._failed_tasks.append({ "url": url, "repeat": repeat})
          self._tasks_lock.release()
          if os.path.isfile(local_filename):
            self._google_storage_accessor.UploadFile(local_filename,
                                            failures_dir + remote_filename)
        print 'Uploading log'
        self._google_storage_accessor.UploadFile(log_filename,
                                         logs_dir + remote_filename)
      # Pop once task is finished, for accurate status tracking.
      self._tasks_lock.acquire()
      url = self._tasks.pop()
      self._tasks_lock.release()

    self._google_storage_accessor.UploadString(
        json.dumps(trace_database.ToJsonDict(), indent=2),
        traces_dir + 'trace_database.json')

    if len(self._failed_tasks) > 0:
      print 'Uploading failing URLs'
      self._google_storage_accessor.UploadString(
          json.dumps(self._failed_tasks, indent=2),
          failures_dir + 'failures.json')

  def _SetTaskList(self, http_body):
    """Sets the list of tasks and starts processing them

    Args:
      http_body: JSON dictionary. See README.md for a description of the format.

    Returns:
      A string to be sent back to the client, describing the success status of
      the request.
    """
    if self._IsProcessingTasks():
      return 'Error: Already running\n'

    load_parameters = json.loads(http_body)
    try:
      tasks = load_parameters['urls']
    except KeyError:
      return 'Error: invalid urls\n'
    # Optional parameters.
    try:
      repeat_count = int(load_parameters.get('repeat_count', '1'))
    except ValueError:
      return 'Error: invalid repeat_count\n'
    emulate_device = load_parameters.get('emulate_device', '')
    emulate_network = load_parameters.get('emulate_network', '')

    if len(tasks) == 0:
      return 'Error: Empty task list\n'
    else:
      self._initial_task_count = len(tasks)
      self._start_time = time.time()
      self._thread = threading.Thread(
          target = self._ProcessTasks,
          args = (tasks, repeat_count, emulate_device, emulate_network))
      self._thread.start()
      return 'Starting generation of %s tasks\n' % str(self._initial_task_count)

  def __call__(self, environ, start_response):
    path = environ['PATH_INFO']

    if path == '/set_tasks':
      # Get the tasks from the HTTP body.
      try:
        body_size = int(environ.get('CONTENT_LENGTH', 0))
      except (ValueError):
        body_size = 0
      body = environ['wsgi.input'].read(body_size)
      data = self._SetTaskList(body)
    elif path == '/test':
      data = 'hello\n'
    elif path == '/status':
      if not self._IsProcessingTasks():
        data = 'Idle\n'
      else:
        task_count = self._GetCurrentTaskCount()
        if task_count == 0:
          data = '%s tasks complete. Finalizing.\n' % self._initial_task_count
        else:
          data = 'Remaining tasks: %s / %s\n' % (
              task_count, self._initial_task_count)
        elapsed = time.time() - self._start_time
        data += 'Elapsed time: %s seconds\n' % str(elapsed)
        self._tasks_lock.acquire()
        failed_tasks = self._failed_tasks
        self._tasks_lock.release()
        data += '%s failed tasks:\n' % len(failed_tasks)
        data += json.dumps(failed_tasks, indent=2)
    else:
      start_response('404 NOT FOUND', [('Content-Length', '0')])
      return iter([''])

    response_headers = [
        ('Content-type','text/plain'),
        ('Content-Length', str(len(data)))
    ]
    start_response('200 OK', response_headers)
    return iter([data])


def StartApp(configuration_file):
  return ServerApp(configuration_file)
