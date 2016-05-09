#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Setups a local Rietveld instance to test against a live server for
integration tests.

It makes sure Google AppEngine SDK is found, download Rietveld and Django code
if necessary and starts the server on a free inbound TCP port.
"""

import logging
import optparse
import os
import shutil
import socket
import sys
import tempfile
import time

try:
  import subprocess2
except ImportError:
  sys.path.append(
      os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
  import subprocess2

DEPOT_TOOLS=os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

class Failure(Exception):
  pass


def test_port(port):
  s = socket.socket()
  try:
    return s.connect_ex(('127.0.0.1', port)) == 0
  finally:
    s.close()


def find_free_port(start_port):
  """Search for a free port starting at specified port."""
  for port in xrange(start_port, (2<<16)):
    if not test_port(port):
      return port
  raise Failure('Having issues finding an available port')


class LocalRietveld(object):
  """Downloads everything needed to run a local instance of Rietveld."""

  def __init__(self, base_dir=None):
    # Paths
    self.base_dir = base_dir
    if not self.base_dir:
      self.base_dir = os.path.dirname(os.path.abspath(__file__))
    self.rietveld = os.path.join(self.base_dir, '_rietveld')
    self.infra = os.path.join(self.base_dir, '_infra')
    self.rietveld_app = os.path.join(
        self.infra, 'infra', 'appengine', 'chromium_rietveld')
    self.dev_app = os.path.join(
        self.infra, 'google_appengine', 'dev_appserver.py')
    self.test_server = None
    self.port = None
    self.tempdir = None

  def install_prerequisites(self):
    if os.path.exists(self.rietveld):
      print "Removing old rietveld dir"
      shutil.rmtree(self.rietveld)

    sdk_path = os.path.join(self.base_dir, 'google_appengine')
    if os.path.exists(sdk_path):
      print "Removing old appengine SDK dir"
      shutil.rmtree(sdk_path)

    previous = os.environ.get('DEPOT_TOOLS_UPDATE')
    os.environ['DEPOT_TOOLS_UPDATE'] = '0'
    try:
      if not os.path.isfile(os.path.join(self.infra, '.gclient')):
        print('Checking out infra...')
        shutil.rmtree(self.infra, ignore_errors=True)
        try:
          os.makedirs(self.infra)
          subprocess2.call(
            [sys.executable, os.path.join(DEPOT_TOOLS, 'fetch.py'),
             '--force', 'infra', '--managed=true'],
            cwd=self.infra)
        except (OSError, subprocess2.CalledProcessError), e:
          raise Failure('Failed to clone infra. \n%s' % e)
      else:
        print('Syncing infra...')
        try:
          subprocess2.call(
            [sys.executable, os.path.join(DEPOT_TOOLS, 'gclient.py'),
             'sync', '--force'],
            cwd=self.infra)
        except (OSError, subprocess2.CalledProcessError), e:
          raise Failure('Failed to sync infra. \n%s' % e)
    finally:
      if previous is None:
        del os.environ['DEPOT_TOOLS_UPDATE']
      else:
        os.environ['DEPOT_TOOLS_UPDATE'] = previous

  def start_server(self, verbose=False):
    self.install_prerequisites()
    assert not self.tempdir
    self.tempdir = tempfile.mkdtemp(prefix='rietveld_test')
    self.port = find_free_port(10000)
    admin_port = find_free_port(self.port + 1)
    if verbose:
      stdout = stderr = None
    else:
      stdout = subprocess2.PIPE
      stderr = subprocess2.STDOUT
    cmd = [
        sys.executable,
        self.dev_app,
        './app.yaml',  # Explicitly specify file to avoid bringing up backends.
        '--port', str(self.port),
        '--admin_port', str(admin_port),
        '--storage', self.tempdir,
        '--clear_search_indexes',
        '--skip_sdk_update_check',
    ]

    # CHEAP TRICK
    # By default you only want to bind on loopback but I'm testing over a
    # headless computer so it's useful to be able to access the test instance
    # remotely.
    if os.environ.get('GAE_LISTEN_ALL', '') == 'true':
      cmd.extend(('-a', '0.0.0.0'))
    logging.info(' '.join(cmd))
    self.test_server = subprocess2.Popen(
        cmd, stdout=stdout, stderr=stderr, cwd=self.rietveld_app)
    # Loop until port 127.0.0.1:port opens or the process dies.
    while not test_port(self.port):
      self.test_server.poll()
      if self.test_server.returncode is not None:
        if not verbose:
          out = self.test_server.communicate()[0]
        shutil.rmtree(self.tempdir)
        self.tempdir = None
        raise Failure(
            'Test rietveld instance failed early on port %s\n%s' %
              (self.port, out))
      time.sleep(0.01)

  def stop_server(self):
    if self.test_server:
      try:
        self.test_server.kill()
      except OSError:
        pass
      self.test_server.wait()
      self.test_server = None
      self.port = None
    if self.tempdir:
      shutil.rmtree(self.tempdir)
      self.tempdir = None


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
