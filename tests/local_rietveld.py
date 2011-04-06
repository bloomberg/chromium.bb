#!/usr/bin/env python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Setups a local Rietveld instance to test against a live server for
integration tests.

It makes sure Google AppEngine SDK is found, download Rietveld and Django code
if necessary and starts the server on a free inbound TCP port.
"""

import optparse
import os
import socket
import sys
import time

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))
import subprocess2


class Failure(Exception):
  pass


def test_port(port):
  s = socket.socket()
  try:
    return s.connect_ex(('127.0.0.1', port)) == 0
  finally:
    s.close()


def find_free_port():
  # Test to find an available port starting at 8080.
  port = 8080
  max_val = (2<<16)
  while test_port(port) and port < max_val:
    port += 1
  if port == max_val:
    raise Failure('Having issues finding an available port')
  return port


class LocalRietveld(object):
  """Downloads everything needed to run a local instance of Rietveld."""

  def __init__(self, base_dir=None):
    # Paths
    self.base_dir = base_dir
    if not self.base_dir:
      self.base_dir = os.path.dirname(os.path.abspath(__file__))
      self.base_dir = os.path.realpath(os.path.join(self.base_dir, '..'))
    self.sdk_path = os.path.abspath(
        os.path.join(self.base_dir, '..', 'google_appengine'))
    self.dev_app = os.path.join(self.sdk_path, 'dev_appserver.py')
    self.rietveld = os.path.join(self.base_dir, 'tests', '_rietveld')
    self.test_server = None
    self.port = None

  def install_prerequisites(self):
    # First, verify the Google AppEngine SDK is available.
    if not os.path.isfile(self.dev_app):
      raise Failure('Install google_appengine sdk in %s' % self.sdk_path)

    # Second, checkout rietveld if not available.
    if not os.path.isdir(self.rietveld):
      print('Checking out rietveld...')
      try:
        subprocess2.check_call(
            ['svn', 'co', '-q', 'http://rietveld.googlecode.com/svn/trunk@681',
             self.rietveld])
      except (OSError, subprocess2.CalledProcessError), e:
        raise Failure('Failed to checkout rietveld\n%s' % e)
    else:
      print('Syncing rietveld...')
      try:
        subprocess2.check_call(
            ['svn', 'up', '-q', '-r', '681'], cwd=self.rietveld)
      except (OSError, subprocess2.CalledProcessError), e:
        raise Failure('Failed to sync rietveld\n%s' % e)

  def start_server(self, verbose=False):
    self.install_prerequisites()
    self.port = find_free_port()
    if verbose:
      pipe = None
    else:
      pipe = subprocess2.VOID
    cmd = [
        sys.executable,
        self.dev_app,
        '--skip_sdk_update_check',
        '.',
        '--port=%d' % self.port,
        '--datastore_path=' + os.path.join(self.rietveld, 'tmp.db'),
        '-c']
    self.test_server = subprocess2.Popen(
        cmd, stdout=pipe, stderr=pipe, cwd=self.rietveld)
    # Loop until port 127.0.0.1:port opens or the process dies.
    while not test_port(self.port):
      self.test_server.poll()
      if self.test_server.returncode is not None:
        raise Failure(
            'Test rietveld instance failed early on port %s' %
            self.port)
      time.sleep(0.01)

  def stop_server(self):
    if self.test_server:
      self.test_server.kill()
      self.test_server.wait()
      self.test_server = None
      self.port = None


def main():
  parser = optparse.OptionParser()
  parser.add_option('-v', '--verbose', action='store_true')
  options, args = parser.parse_args()
  if args:
    parser.error('Unknown arguments: %s' % ' '.join(args))
  instance = LocalRietveld()
  try:
    instance.start_server(verbose=options.verbose)
    print 'Local rietveld instance started on port %d' % instance.port
    while True:
      time.sleep(0.1)
  finally:
    instance.stop_server()


if __name__ == '__main__':
  main()
