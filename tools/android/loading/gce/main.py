# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import os
import re
import threading
import subprocess

from gcloud import storage
from gcloud.exceptions import NotFound
from oauth2client.client import GoogleCredentials

class ServerApp(object):
  """Simple web server application, collecting traces and writing them in
  Google Cloud Storage.
  """

  def __init__(self, configuration_file):
    """|configuration_file| is a path to a file containing JSON as described in
    README.md.
    """
    self._tasks = []
    self._thread = None
    print 'Initializing credentials'
    self._credentials = GoogleCredentials.get_application_default()
    print 'Reading configuration'
    with open(configuration_file) as config_json:
       config = json.load(config_json)
       self._project_name = config['project_name']

       # Separate the cloud storage path into the bucket and the base path under
       # the bucket.
       storage_path_components = config['cloud_storage_path'].split('/')
       self._bucket_name = storage_path_components[0]
       self._base_path_in_bucket = ''
       if len(storage_path_components) > 1:
         self._base_path_in_bucket = '/'.join(storage_path_components[1:])
         if not self._base_path_in_bucket.endswith('/'):
           self._base_path_in_bucket += '/'

       self._chrome_path = config['chrome_path']
       self._src_path = config['src_path']


  def _GetStorageClient(self):
    return storage.Client(project = self._project_name,
                          credentials = self._credentials)

  def _GetStorageBucket(self, storage_client):
    return storage_client.get_bucket(self._bucket_name)

  def _UploadFile(self, filename_src, filename_dest):
    """Uploads a file to Google Cloud Storage

    Args:
      filename_src: name of the local file
      filename_dest: name of the file in Google Cloud Storage

    Returns:
      The URL of the file in Google Cloud Storage.
    """
    client = self._GetStorageClient()
    bucket = self._GetStorageBucket(client)
    blob = bucket.blob(filename_dest)
    with open(filename_src) as file_src:
      blob.upload_from_file(file_src)
    return blob.public_url

  def _UploadString(self, data_string, filename_dest):
    """Uploads a string to Google Cloud Storage

    Args:
      data_string: the contents of the file to be uploaded
      filename_dest: name of the file in Google Cloud Storage

    Returns:
      The URL of the file in Google Cloud Storage.
    """
    client = self._GetStorageClient()
    bucket = self._GetStorageBucket(client)
    blob = bucket.blob(filename_dest)
    blob.upload_from_string(data_string)
    return blob.public_url

  def _GenerateTrace(self, url, filename, log_filename):
    """ Generates a trace using analyze.py

    Args:
      url: url as a string.
      filename: name of the file where the trace is saved.
      log_filename: name of the file where standard output and errors are logged

    Returns:
      True if the trace was generated successfully.
    """
    try:
      os.remove(filename)  # Remove any existing trace for this URL.
    except OSError:
      pass  # Nothing to remove.
    analyze_path = self._src_path + '/tools/android/loading/analyze.py'
    command_line = ['python', analyze_path, 'log_requests', '--local_noisy',
        '--clear_cache', '--local', '--headless', '--local_binary',
        self._chrome_path, '--url', url, '--output', filename]
    with open(log_filename, 'w') as log_file:
      ret = subprocess.call(command_line , stderr = subprocess.STDOUT,
                            stdout = log_file)
    return ret == 0

  def _ProcessTasks(self, repeat_count):
    """Iterates over _tasks and runs analyze.py on each of them. Uploads the
    resulting traces to Google Cloud Storage.

    Args:
      repeat_count: The number of traces generated for each URL.
    """
    failures_dir = self._base_path_in_bucket + 'failures/'
    traces_dir = self._base_path_in_bucket + 'traces/'
    logs_dir = self._base_path_in_bucket + 'analyze_logs/'
    log_filename = 'analyze.log'
    # Avoid special characters in storage object names
    pattern = re.compile(r"[#\?\[\]\*/]")
    failed_tasks = []
    while len(self._tasks) > 0:
      url = self._tasks.pop()
      local_filename = pattern.sub('_', url)
      for repeat in range(repeat_count):
        print 'Generating trace for URL: %s' % url
        remote_filename = local_filename + '/' + str(repeat)
        if self._GenerateTrace(url, local_filename, log_filename):
          print 'Uploading: %s' % remote_filename
          self._UploadFile(local_filename, traces_dir + remote_filename)
        else:
          print 'analyze.py failed for URL: %s' % url
          failed_tasks.append({ "url": url, "repeat": repeat})
          if os.path.isfile(local_filename):
            self._UploadFile(local_filename, failures_dir + remote_filename)
        print 'Uploading analyze log'
        self._UploadFile(log_filename, logs_dir + remote_filename)

    if len(failed_tasks) > 0:
      print 'Uploading failing URLs'
      self._UploadString(json.dumps(failed_tasks),
                         failures_dir + 'failures.json')

  def _SetTaskList(self, http_body):
    """Sets the list of tasks and starts processing them

    Args:
      http_body: JSON dictionary. See README.md for a description of the format.

    Returns:
      A string to be sent back to the client, describing the success status of
      the request.
    """
    load_parameters = json.loads(http_body)
    try:
      self._tasks = load_parameters['urls']
    except KeyError:
      return 'Error: invalid urls'
    try:
      repeat_count = int(load_parameters['repeat_count'])
    except (KeyError, ValueError):
      return 'Error: invalid repeat_count'

    if len(self._tasks) == 0:
      return 'Error: Empty task list'
    elif self._thread is not None and self._thread.is_alive():
      return 'Error: Already running'
    else:
      self._thread = threading.Thread(target = self._ProcessTasks,
                                      args = (repeat_count,))
      self._thread.start()
      return 'Starting generation of ' + str(len(self._tasks)) + 'tasks'

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
      data = 'hello'
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
