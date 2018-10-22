# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Functions for fetching and updating a list of top-level tests."""

import collections
import logging

from google.appengine.api import datastore_errors
from google.appengine.ext import ndb

from dashboard.common import datastore_hooks
from dashboard.common import descriptor
from dashboard.common import request_handler
from dashboard.common import stored_object
from dashboard.common import namespaced_stored_object
from dashboard.common import utils
from dashboard.models import graph_data

# TestMetadata suite cache key.
_LIST_SUITES_CACHE_KEY = 'list_tests_get_test_suites'

TEST_SUITES_2_CACHE_KEY = 'test_suites_2'


def FetchCachedTestSuites2():
  return namespaced_stored_object.Get(TEST_SUITES_2_CACHE_KEY)


def FetchCachedTestSuites():
  """Fetches cached test suite data."""
  cached = namespaced_stored_object.Get(_LIST_SUITES_CACHE_KEY)
  if cached is None:
    # If the cache test suite list is not set, update it before fetching.
    # This is for convenience when testing sending of data to a local instance.
    UpdateTestSuites(datastore_hooks.GetNamespace())
    cached = namespaced_stored_object.Get(_LIST_SUITES_CACHE_KEY)
  return cached


class UpdateTestSuitesHandler(request_handler.RequestHandler):
  """A simple request handler to refresh the cached test suites info."""

  def get(self):
    """Refreshes the cached test suites list."""
    self.post()

  def post(self):
    """Refreshes the cached test suites list."""
    if self.request.get('internal_only') == 'true':
      logging.info('Going to update internal-only test suites data.')
      # Update internal-only test suites data.
      datastore_hooks.SetPrivilegedRequest()
      UpdateTestSuites(datastore_hooks.INTERNAL)
    else:
      logging.info('Going to update externally-visible test suites data.')
      # Update externally-visible test suites data.
      UpdateTestSuites(datastore_hooks.EXTERNAL)


def UpdateTestSuites(permissions_namespace):
  """Updates test suite data for either internal or external users."""
  logging.info('Updating test suite data for: %s', permissions_namespace)
  suite_dict = _CreateTestSuiteDict()
  key = namespaced_stored_object.NamespaceKey(
      _LIST_SUITES_CACHE_KEY, permissions_namespace)
  stored_object.Set(key, suite_dict)

  stored_object.Set(namespaced_stored_object.NamespaceKey(
      TEST_SUITES_2_CACHE_KEY, permissions_namespace), _ListTestSuites())


@ndb.tasklet
def _ListTestSuitesAsync(test_suites, partial_tests, parent_test=None):
  # Some test suites are composed of multiple test path components. See
  # Descriptor. When a TestMetadata key doesn't contain enough test path
  # components to compose a full test suite, add its key to partial_tests so
  # that the caller can run another query with parent_test.
  query = graph_data.TestMetadata.query()
  query = query.filter(graph_data.TestMetadata.parent_test == parent_test)
  query = query.filter(graph_data.TestMetadata.deprecated == False)
  keys = yield query.fetch_async(keys_only=True)
  for key in keys:
    test_path = utils.TestPath(key)
    desc = yield descriptor.Descriptor.FromTestPathAsync(test_path)
    if desc.test_suite:
      test_suites.add(desc.test_suite)
    elif partial_tests is not None:
      partial_tests.add(key)
    else:
      logging.error('Unable to parse "%s"', test_path)


@ndb.synctasklet
def _ListTestSuites():
  test_suites = set()
  partial_tests = set()
  yield _ListTestSuitesAsync(test_suites, partial_tests)
  yield [_ListTestSuitesAsync(test_suites, None, key) for key in partial_tests]
  test_suites = list(test_suites)
  test_suites.sort()
  raise ndb.Return(test_suites)


def _CreateTestSuiteDict():
  """Returns a dictionary with information about top-level tests.

  This method is used to generate the global JavaScript variable TEST_SUITES
  for the report page. This variable is used to initially populate the select
  menus.

  Note that there will be multiple top level TestMetadata entities for each
  suite name, since each suite name appears under multiple bots.

  Returns:
    A dictionary of the form:
      {
          'my_test_suite': {
              'mas': {'ChromiumPerf': {'mac': False, 'linux': False}},
              'dep': True,
              'des': 'A description.'
          },
          ...
      }

    Where 'mas', 'dep', and 'des' are abbreviations for 'masters',
    'deprecated', and 'description', respectively.
  """
  suites = _FetchSuites()
  result = collections.defaultdict(lambda: {'suites': []})

  for s in suites:
    result[s.test_name]['suites'].append(s)

  # Don't need suites anymore since they've been binned by test_name in result,
  # so we can drop the reference and they'll be freed in the loop.
  suites = None

  # Should have a dict of {suite: [all suites]}
  # Now generate masters
  for k, v in result.iteritems():
    current_suites = v['suites']
    v['mas'] = {}

    if current_suites:
      if current_suites[0].description:
        v['des'] = current_suites[0].description

    if all(s.deprecated for s in current_suites):
      v['dep'] = True

    for s in current_suites:
      master_name = s.master_name
      bot_name = s.bot_name
      if not master_name in v['mas']:
        v['mas'][master_name] = {}
      if not bot_name in v['mas'][master_name]:
        v['mas'][master_name][bot_name] = s.deprecated

    # We don't need these suites anymore so free them.
    del result[k]['suites']

  return dict(result)


def _FetchSuites():
  """Fetches Tests with deprecated and description projections."""
  suite_query = graph_data.TestMetadata.query(
      graph_data.TestMetadata.parent_test == None)
  suites = []
  cursor = None
  more = True
  try:
    while more:
      some_suites, cursor, more = suite_query.fetch_page(
          2000, start_cursor=cursor,
          projection=['deprecated', 'description'],
          use_cache=False, use_memcache=False)
      suites.extend(some_suites)
  except datastore_errors.Timeout:
    logging.error('Timeout after fetching %d test suites.', len(suites))
  return suites


def _GetTestSubPath(key):
  """Gets the part of the test path after the suite, for the given test key.

  For example, for a test with the test path 'MyMaster/bot/my_suite/foo/bar',
  this should return 'foo/bar'.

  Args:
    key: The key of the TestMetadata entity.

  Returns:
    Slash-separated test path part after master/bot/suite.
  """
  return '/'.join(p for p in key.string_id().split('/')[3:])
