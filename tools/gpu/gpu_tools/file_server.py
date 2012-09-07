# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import os
import subprocess

class FileServer(object):
  def __init__(self, path, port=8000):
    self._server = None
    self._devnull = None
    self._path = path
    self._port = port

    assert os.path.exists(path)
    assert os.path.isdir(path)

  def __enter__(self):
    self._devnull = open(os.devnull, 'w')
    self._server = subprocess.Popen(
        ['python', '-m', 'SimpleHTTPServer', str(self._port)],
        cwd=self._path,
        stdout=self._devnull, stderr=self._devnull)
    return self

  @property
  def url(self):
    return 'http://localhost:%d' % self._port

  def __exit__(self, *args):
    if self._server:
      self._server.kill()
      self._server = None
    if self._devnull:
      self._devnull.close()
      self._devnull = None

  def __del__(self):
    if self._server:
      self._server.kill()
    if self._devnull:
      self._devnull.close()
      self._devnull = None
