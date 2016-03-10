# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json

from gcloud import storage
from gcloud.exceptions import NotFound
from oauth2client.client import GoogleCredentials

class ServerApp(object):
  """Simple web server application, collecting traces and writing them in
  Google Cloud Storage.
  """

  def __init__(self):
    print 'Initializing credentials'
    self._credentials = GoogleCredentials.get_application_default()
    print 'Reading server configuration'
    with open('server_config.json') as configuration_file:
       self._config = json.load(configuration_file)

  def _GetStorageClient(self):
    return storage.Client(project = self._config['project_name'],
                          credentials = self._credentials)

  def _GetStorageBucket(self, storage_client):
    return storage_client.get_bucket(self._config['bucket_name'])

  def _UploadFile(self, file_stream, filename):
    client = self._GetStorageClient()
    bucket = self._GetStorageBucket(client)
    blob = bucket.blob(filename)
    blob.upload_from_string(file_stream)
    url = blob.public_url
    return url

  def _DeleteFile(self, filename):
    client = self._GetStorageClient()
    bucket = self._GetStorageBucket(client)
    try:
      bucket.delete_blob(filename)
      return True
    except NotFound:
      return False

  def _ReadFile(self, filename):
    client = self._GetStorageClient()
    bucket = self._GetStorageBucket(client)
    blob = bucket.get_blob(filename)
    if not blob:
      return None
    return blob.download_as_string()

  def __call__(self, environ, start_response):
    path = environ['PATH_INFO']
    if path == '/favicon.ico':
        start_response('404 NOT FOUND', [('Content-Length', '0')])
        return iter([''])

    status = '200 OK'

    if path == '/write':
      url = self._UploadFile('foo', 'test.txt')
      data = 'Writing file at\n' + url + '\n'
    elif path == '/read':
      data = self._ReadFile('test.txt')
      if not data:
        data = ''
        status = '404 NOT FOUND'
    elif path == '/delete':
      if self._DeleteFile('test.txt'):
        data = 'Success\n'
      else:
        data = 'Failed\n'
    else:
      data = environ['PATH_INFO'] + '\n'

    response_headers = [
        ('Content-type','text/plain'),
        ('Content-Length', str(len(data)))
    ]
    start_response(status, response_headers)
    return iter([data])


app = ServerApp()
