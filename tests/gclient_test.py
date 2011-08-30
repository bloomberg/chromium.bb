#!/usr/bin/env python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for gclient.py.

See gclient_smoketest.py for integration tests.
"""

import Queue
import logging
import os
import sys
import unittest

BASE_DIR = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(BASE_DIR))

import gclient
from tests import trial_dir


def write(filename, content):
  """Writes the content of a file and create the directories as needed."""
  filename = os.path.abspath(filename)
  dirname = os.path.dirname(filename)
  if not os.path.isdir(dirname):
    os.makedirs(dirname)
  with open(filename, 'w') as f:
    f.write(content)


class SCMMock(object):
  def __init__(self, unit_test, url):
    self.unit_test = unit_test
    self.url = url

  def RunCommand(self, command, options, args, file_list):
    self.unit_test.assertEquals('None', command)
    self.unit_test.processed.put(self.url)

  def FullUrlForRelativeUrl(self, url):
    return self.url + url


class GclientTest(trial_dir.TestCase):
  def setUp(self):
    super(GclientTest, self).setUp()
    self.processed = Queue.Queue()
    self.previous_dir = os.getcwd()
    os.chdir(self.root_dir)
    # Manual mocks.
    self._old_createscm = gclient.gclient_scm.CreateSCM
    gclient.gclient_scm.CreateSCM = self._createscm
    self._old_sys_stdout = sys.stdout
    sys.stdout = gclient.gclient_utils.MakeFileAutoFlush(sys.stdout)
    sys.stdout = gclient.gclient_utils.MakeFileAnnotated(sys.stdout)

  def tearDown(self):
    self.assertEquals([], self._get_processed())
    gclient.gclient_scm.CreateSCM = self._old_createscm
    sys.stdout = self._old_sys_stdout
    os.chdir(self.previous_dir)
    super(GclientTest, self).tearDown()

  def _createscm(self, parsed_url, root_dir, name):
    self.assertTrue(parsed_url.startswith('svn://example.com/'), parsed_url)
    self.assertTrue(root_dir.startswith(self.root_dir), root_dir)
    return SCMMock(self, parsed_url)

  def testDependencies(self):
    self._dependencies('1', False)

  def testDependenciesReverse(self):
    self._dependencies('1', True)

  def testDependenciesJobs(self):
    self._dependencies('1000', False)

  def testDependenciesJobsReverse(self):
    self._dependencies('1000', True)

  def _dependencies(self, jobs, reverse):
    # Verify that dependencies are processed in the right order, e.g. if there
    # is a dependency 'src' and another 'src/third_party/bar', that bar isn't
    # fetched until 'src' is done.
    # jobs is the number of parallel jobs simulated. reverse is to reshuffle the
    # list to see if it is still processed in order correctly.
    parser = gclient.Parser()
    options, args = parser.parse_args(['--jobs', jobs])
    write(
        '.gclient',
        'solutions = [\n'
        '  { "name": "foo", "url": "svn://example.com/foo" },\n'
        '  { "name": "bar", "url": "svn://example.com/bar" },\n'
        ']')
    write(
        os.path.join('foo', 'DEPS'),
        'deps = {\n'
        '  "foo/dir1": "/dir1",\n'
        '  "foo/dir1/dir2/dir3": "/dir1/dir2/dir3",\n'
        '  "foo/dir1/dir4": "/dir1/dir4",\n'
        '  "foo/dir1/dir2/dir3/dir4": "/dir1/dir2/dir3/dir4",\n'
        '}')
    write(
        os.path.join('bar', 'DEPS'),
        'deps = {\n'
        '  "foo/dir1/dir2": "/dir1/dir2",\n'
        '}')

    obj = gclient.GClient.LoadCurrentConfig(options)
    self._check_requirements(obj.dependencies[0], {})
    self._check_requirements(obj.dependencies[1], {})
    obj.RunOnDeps('None', args)
    # The trick here is to manually process the list to make sure it's out of
    # order.
    obj.dependencies[0].dependencies.sort(key=lambda x: x.name, reverse=reverse)
    actual = self._get_processed()
    # We don't care of the ordering of this item.
    actual.remove('svn://example.com/bar/dir1/dir2')
    self.assertEquals(
        [
          'svn://example.com/foo',
          'svn://example.com/bar',
          'svn://example.com/foo/dir1',
          'svn://example.com/foo/dir1/dir4',
          'svn://example.com/foo/dir1/dir2/dir3',
          'svn://example.com/foo/dir1/dir2/dir3/dir4',
        ],
        actual)
    self._check_requirements(
        obj.dependencies[0],
        {
          'foo/dir1': ['foo'],
          'foo/dir1/dir2/dir3': ['foo', 'foo/dir1'],
          'foo/dir1/dir2/dir3/dir4': ['foo', 'foo/dir1', 'foo/dir1/dir2/dir3'],
          'foo/dir1/dir4': ['foo', 'foo/dir1'],
        })
    self._check_requirements(
        obj.dependencies[1],
        {
          'foo/dir1/dir2': ['bar'],
        })

  def _check_requirements(self, solution, expected):
    for dependency in solution.dependencies:
      self.assertEquals(expected.pop(dependency.name), dependency.requirements)
    self.assertEquals({}, expected)

  def _get_processed(self):
    items = []
    try:
      while True:
        items.append(self.processed.get_nowait())
    except Queue.Empty:
      pass
    return items


if __name__ == '__main__':
  logging.basicConfig(
      level=[logging.ERROR, logging.WARNING, logging.INFO, logging.DEBUG][
        min(sys.argv.count('-v'), 3)],
      format='%(asctime).19s %(levelname)s %(filename)s:'
              '%(lineno)s %(message)s')
  unittest.main()
