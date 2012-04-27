#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
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

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

import gclient
import gclient_utils
from testing_support import trial_dir


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
    self._dependencies('1')

  def testDependenciesJobs(self):
    self._dependencies('1000')

  def _dependencies(self, jobs):
    """Verifies that dependencies are processed in the right order.

    e.g. if there is a dependency 'src' and another 'src/third_party/bar', that
    bar isn't fetched until 'src' is done.
    Also test that a From() dependency should not be processed when it is listed
    as a requirement.

    Args:
      |jobs| is the number of parallel jobs simulated.
    """
    parser = gclient.Parser()
    options, args = parser.parse_args(['--jobs', jobs])
    write(
        '.gclient',
        'solutions = [\n'
        '  { "name": "foo", "url": "svn://example.com/foo" },\n'
        '  { "name": "bar", "url": "svn://example.com/bar" },\n'
        '  { "name": "bar/empty", "url": "svn://example.com/bar_empty" },\n'
        ']')
    write(
        os.path.join('foo', 'DEPS'),
        'deps = {\n'
        '  "foo/dir1": "/dir1",\n'
        # This one will depend on dir1/dir2 in bar.
        '  "foo/dir1/dir2/dir3": "/dir1/dir2/dir3",\n'
        '  "foo/dir1/dir2/dir3/dir4": "/dir1/dir2/dir3/dir4",\n'
        '  "foo/dir1/dir2/dir5/dir6":\n'
        '    From("foo/dir1/dir2/dir3/dir4", "foo/dir1/dir2"),\n'
        '}')
    write(
        os.path.join('bar', 'DEPS'),
        'deps = {\n'
        # There is two foo/dir1/dir2. This one is fetched as bar/dir1/dir2.
        '  "foo/dir1/dir2": "/dir1/dir2",\n'
        '}')
    write(
        os.path.join('bar/empty', 'DEPS'),
        'deps = {\n'
        '}')
    # Test From()
    write(
        os.path.join('foo/dir1/dir2/dir3/dir4', 'DEPS'),
        'deps = {\n'
        # This one should not be fetched or set as a requirement.
        '  "foo/dir1/dir2/dir5": "svn://example.com/x",\n'
        # This foo/dir1/dir2 points to a different url than the one in bar.
        '  "foo/dir1/dir2": "/dir1/another",\n'
        '}')

    obj = gclient.GClient.LoadCurrentConfig(options)
    self._check_requirements(obj.dependencies[0], {})
    self._check_requirements(obj.dependencies[1], {})
    obj.RunOnDeps('None', args)
    actual = self._get_processed()
    first_3 = [
        'svn://example.com/bar',
        'svn://example.com/bar_empty',
        'svn://example.com/foo',
    ]
    if jobs != 1:
      # We don't care of the ordering of these items except that bar must be
      # before bar/empty.
      self.assertTrue(
          actual.index('svn://example.com/bar') <
          actual.index('svn://example.com/bar_empty'))
      self.assertEquals(first_3, sorted(actual[0:3]))
    else:
      self.assertEquals(first_3, actual[0:3])
    self.assertEquals(
        [
          'svn://example.com/foo/dir1',
          'svn://example.com/bar/dir1/dir2',
          'svn://example.com/foo/dir1/dir2/dir3',
          'svn://example.com/foo/dir1/dir2/dir3/dir4',
          'svn://example.com/foo/dir1/dir2/dir3/dir4/dir1/another',
        ],
        actual[3:])

    self.assertEquals(3, len(obj.dependencies))
    self.assertEquals('foo', obj.dependencies[0].name)
    self.assertEquals('bar', obj.dependencies[1].name)
    self.assertEquals('bar/empty', obj.dependencies[2].name)
    self._check_requirements(
        obj.dependencies[0],
        {
          'foo/dir1': ['bar', 'bar/empty', 'foo'],
          'foo/dir1/dir2/dir3':
              ['bar', 'bar/empty', 'foo', 'foo/dir1', 'foo/dir1/dir2'],
          'foo/dir1/dir2/dir3/dir4':
              [ 'bar', 'bar/empty', 'foo', 'foo/dir1', 'foo/dir1/dir2',
                'foo/dir1/dir2/dir3'],
          'foo/dir1/dir2/dir5/dir6':
              [ 'bar', 'bar/empty', 'foo', 'foo/dir1', 'foo/dir1/dir2',
                'foo/dir1/dir2/dir3/dir4'],
        })
    self._check_requirements(
        obj.dependencies[1],
        {
          'foo/dir1/dir2': ['bar', 'bar/empty', 'foo', 'foo/dir1'],
        })
    self._check_requirements(
        obj.dependencies[2],
        {})
    self._check_requirements(
        obj,
        {
          'foo': [],
          'bar': [],
          'bar/empty': ['bar'],
        })

  def _check_requirements(self, solution, expected):
    for dependency in solution.dependencies:
      e = expected.pop(dependency.name)
      a = sorted(dependency.requirements)
      self.assertEquals(e, a, (dependency.name, e, a))
    self.assertEquals({}, expected)

  def _get_processed(self):
    """Retrieves the item in the order they were processed."""
    items = []
    try:
      while True:
        items.append(self.processed.get_nowait())
    except Queue.Empty:
      pass
    return items

  def testAutofix(self):
    # Invalid urls causes pain when specifying requirements. Make sure it's
    # auto-fixed.
    d = gclient.Dependency(
        None, 'name', 'proto://host/path/@revision', None, None, None,
        None, '', True)
    self.assertEquals('proto://host/path@revision', d.url)

  def testStr(self):
    parser = gclient.Parser()
    options, _ = parser.parse_args([])
    obj = gclient.GClient('foo', options)
    obj.add_dependencies_and_close(
        [
          gclient.Dependency(
            obj, 'foo', 'url', None, None, None, None, 'DEPS', True),
          gclient.Dependency(
            obj, 'bar', 'url', None, None, None, None, 'DEPS', True),
        ],
        [])
    obj.dependencies[0].add_dependencies_and_close(
        [
          gclient.Dependency(
            obj.dependencies[0], 'foo/dir1', 'url', None, None, None, None,
            'DEPS', True),
          gclient.Dependency(
            obj.dependencies[0], 'foo/dir2',
            gclient.GClientKeywords.FromImpl('bar'), None, None, None, None,
            'DEPS', True),
          gclient.Dependency(
            obj.dependencies[0], 'foo/dir3',
            gclient.GClientKeywords.FileImpl('url'), None, None, None, None,
            'DEPS', True),
        ],
        [])
    # Make sure __str__() works fine.
    # pylint: disable=W0212
    obj.dependencies[0]._file_list.append('foo')
    str_obj = str(obj)
    self.assertEquals(471, len(str_obj), '%d\n%s' % (len(str_obj), str_obj))

  def testHooks(self):
    topdir = self.root_dir
    gclient_fn = os.path.join(topdir, '.gclient')
    fh = open(gclient_fn, 'w')
    print >> fh, 'solutions = [{"name":"top","url":"svn://svn.top.com/top"}]'
    fh.close()
    subdir_fn = os.path.join(topdir, 'top')
    os.mkdir(subdir_fn)
    deps_fn = os.path.join(subdir_fn, 'DEPS')
    fh = open(deps_fn, 'w')
    hooks = [{'pattern':'.', 'action':['cmd1', 'arg1', 'arg2']}]
    print >> fh, 'hooks = %s' % repr(hooks)
    fh.close()

    fh = open(os.path.join(subdir_fn, 'fake.txt'), 'w')
    print >> fh, 'bogus content'
    fh.close()

    os.chdir(topdir)

    parser = gclient.Parser()
    options, _ = parser.parse_args([])
    options.force = True
    client = gclient.GClient.LoadCurrentConfig(options)
    work_queue = gclient_utils.ExecutionQueue(options.jobs, None)
    for s in client.dependencies:
      work_queue.enqueue(s)
    work_queue.flush({}, None, [], options=options)
    self.assertEqual(client.GetHooks(options), [x['action'] for x in hooks])

  def testTargetOS(self):
    """Verifies that specifying a target_os pulls in all relevant dependencies.

    The target_os variable allows specifying the name of an additional OS which
    should be considered when selecting dependencies from a DEPS' deps_os. The
    value will be appended to the _enforced_os tuple.
    """

    write(
        '.gclient',
        'solutions = [\n'
        '  { "name": "foo",\n'
        '    "url": "svn://example.com/foo",\n'
        '  }]\n'
        'target_os = ["baz"]')
    write(
        os.path.join('foo', 'DEPS'),
        'deps = {\n'
        '  "foo/dir1": "/dir1",'
        '}\n'
        'deps_os = {\n'
        '  "unix": { "foo/dir2": "/dir2", },\n'
        '  "baz": { "foo/dir3": "/dir3", },\n'
        '}')

    parser = gclient.Parser()
    options, _ = parser.parse_args(['--jobs', '1'])
    options.deps_os = "unix"

    obj = gclient.GClient.LoadCurrentConfig(options)
    self.assertEqual(['baz', 'unix'], sorted(obj.enforced_os))
    

if __name__ == '__main__':
  sys.stdout = gclient_utils.MakeFileAutoFlush(sys.stdout)
  sys.stdout = gclient_utils.MakeFileAnnotated(sys.stdout, include_zero=True)
  sys.stderr = gclient_utils.MakeFileAutoFlush(sys.stderr)
  sys.stderr = gclient_utils.MakeFileAnnotated(sys.stderr, include_zero=True)
  logging.basicConfig(
      level=[logging.ERROR, logging.WARNING, logging.INFO, logging.DEBUG][
        min(sys.argv.count('-v'), 3)],
      format='%(relativeCreated)4d %(levelname)5s %(module)13s('
              '%(lineno)d) %(message)s')
  unittest.main()
