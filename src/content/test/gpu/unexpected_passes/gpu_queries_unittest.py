#!/usr/bin/env vpython3
# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import print_function

import json
import subprocess
import sys
import unittest

if sys.version_info[0] == 2:
  import mock
else:
  import unittest.mock as mock

from unexpected_passes import gpu_queries
from unexpected_passes import gpu_unittest_utils as gpu_uu
from unexpected_passes_common import builders
from unexpected_passes_common import constants
from unexpected_passes_common import data_types
from unexpected_passes_common import unittest_utils as uu


class QueryBuilderUnittest(unittest.TestCase):
  def setUp(self):
    self._patcher = mock.patch.object(subprocess, 'Popen')
    self._popen_mock = self._patcher.start()
    self.addCleanup(self._patcher.stop)

    builders.ClearInstance()
    uu.RegisterGenericBuildersImplementation()

  def testWebGlVersion(self):
    """Tests that only results for the correct WebGL version are returned."""
    query_results = [
        {
            'id':
            'build-1234',
            'test_id': ('ninja://chrome/test:telemetry_gpu_integration_test/'
                        'gpu_tests.webgl_conformance_integration_test.'
                        'WebGLConformanceIntegrationTest.test_name'),
            'status':
            'FAIL',
            'typ_expectations': [
                'RetryOnFailure',
            ],
            'typ_tags': [
                'webgl-version-1',
            ],
            'step_name':
            'step_name',
        },
        {
            'id':
            'build-2345',
            'test_id': ('ninja://chrome/test:telemetry_gpu_integration_test/'
                        'gpu_tests.webgl_conformance_integration_test.'
                        'WebGLConformanceIntegrationTest.test_name'),
            'status':
            'FAIL',
            'typ_expectations': [
                'RetryOnFailure',
            ],
            'typ_tags': [
                'webgl-version-2',
            ],
            'step_name':
            'step_name',
        },
    ]
    querier = gpu_uu.CreateGenericGpuQuerier(suite='webgl_conformance1')
    self._popen_mock.return_value = uu.FakeProcess(
        stdout=json.dumps(query_results))
    results, expectation_files = querier.QueryBuilder(
        data_types.BuilderEntry('builder', constants.BuilderTypes.CI, False))
    self.assertEqual(len(results), 1)
    self.assertIsNone(expectation_files)
    self.assertEqual(
        results[0],
        data_types.Result('test_name', ['webgl-version-1'], 'Failure',
                          'step_name', '1234'))

    querier = gpu_uu.CreateGenericGpuQuerier(suite='webgl_conformance2')
    results, expectation_files = querier.QueryBuilder(
        data_types.BuilderEntry('builder', constants.BuilderTypes.CI, False))
    self.assertEqual(len(results), 1)
    self.assertIsNone(expectation_files)
    self.assertEqual(
        results[0],
        data_types.Result('test_name', ['webgl-version-2'], 'Failure',
                          'step_name', '2345'))

  def testSuiteExceptionMap(self):
    """Tests that the suite passed to the query changes for some suites."""

    def assertSuiteInQuery(suite, call_args):
      query = call_args[0][0][0]
      s = 'r"gpu_tests\\.%s\\."' % suite
      self.assertIn(s, query)

    # Non-special cased suite.
    querier = gpu_uu.CreateGenericGpuQuerier()
    with mock.patch.object(querier,
                           '_RunBigQueryCommandsForJsonOutput') as query_mock:
      _ = querier.QueryBuilder(
          data_types.BuilderEntry('builder', constants.BuilderTypes.CI, False))
      assertSuiteInQuery('pixel_integration_test', query_mock.call_args)

    # Special-cased suites.
    querier = gpu_uu.CreateGenericGpuQuerier(suite='info_collection')
    with mock.patch.object(querier,
                           '_RunBigQueryCommandsForJsonOutput') as query_mock:
      _ = querier.QueryBuilder(
          data_types.BuilderEntry('builder', constants.BuilderTypes.CI, False))
      assertSuiteInQuery('info_collection_test', query_mock.call_args)

    querier = gpu_uu.CreateGenericGpuQuerier(suite='power')
    with mock.patch.object(querier,
                           '_RunBigQueryCommandsForJsonOutput') as query_mock:
      _ = querier.QueryBuilder(
          data_types.BuilderEntry('builder', constants.BuilderTypes.CI, False))
      assertSuiteInQuery('power_measurement_integration_test',
                         query_mock.call_args)

    querier = gpu_uu.CreateGenericGpuQuerier(suite='trace_test')
    with mock.patch.object(querier,
                           '_RunBigQueryCommandsForJsonOutput') as query_mock:
      _ = querier.QueryBuilder(
          data_types.BuilderEntry('builder', constants.BuilderTypes.CI, False))
      assertSuiteInQuery('trace_integration_test', query_mock.call_args)


class GetQueryGeneratorForBuilderUnittest(unittest.TestCase):
  def setUp(self):
    self._querier = gpu_uu.CreateGenericGpuQuerier()
    self._query_patcher = mock.patch.object(
        self._querier, '_RunBigQueryCommandsForJsonOutput')
    self._query_mock = self._query_patcher.start()
    self.addCleanup(self._query_patcher.stop)

  def testNoLargeQueryMode(self):
    """Tests that the expected clause is returned in normal mode."""
    test_filter = self._querier._GetQueryGeneratorForBuilder(
        data_types.BuilderEntry('builder', 'builder_type', False))
    self.assertEqual(len(test_filter.GetClauses()), 1)
    self.assertEqual(
        test_filter.GetClauses()[0], """\
        AND REGEXP_CONTAINS(
          test_id,
          r"gpu_tests\.pixel_integration_test\.")""")
    self.assertIsInstance(test_filter, gpu_queries.GpuFixedQueryGenerator)
    self._query_mock.assert_not_called()

  def testLargeQueryModeNoTests(self):
    """Tests that a special value is returned if no tests are found."""
    querier = gpu_uu.CreateGenericGpuQuerier(large_query_mode=True)
    with mock.patch.object(querier,
                           '_RunBigQueryCommandsForJsonOutput',
                           return_value=[]) as query_mock:
      test_filter = querier._GetQueryGeneratorForBuilder(
          data_types.BuilderEntry('builder', constants.BuilderTypes.CI, False))
      self.assertIsNone(test_filter)
      query_mock.assert_called_once()

  def testLargeQueryModeFoundTests(self):
    """Tests that a clause containing found tests is returned."""
    querier = gpu_uu.CreateGenericGpuQuerier(large_query_mode=True)
    with mock.patch.object(querier,
                           '_RunBigQueryCommandsForJsonOutput') as query_mock:
      query_mock.return_value = [{
          'test_id': 'foo_test'
      }, {
          'test_id': 'bar_test'
      }]
      test_filter = querier._GetQueryGeneratorForBuilder(
          data_types.BuilderEntry('builder', constants.BuilderTypes.CI, False))
      self.assertEqual(test_filter.GetClauses(),
                       ['AND test_id IN UNNEST(["foo_test", "bar_test"])'])
      self.assertIsInstance(test_filter, gpu_queries.GpuSplitQueryGenerator)


class GetActiveBuilderQueryUnittest(unittest.TestCase):
  def setUp(self):
    self.querier = gpu_uu.CreateGenericGpuQuerier()

  def testPublicCi(self):
    """Tests that the active query for public CI is as expected."""
    expected_query = """\
WITH
  builders AS (
    SELECT
      (
        SELECT value
        FROM tr.variant
        WHERE key = "builder") as builder_name
    FROM
      `chrome-luci-data.chromium.gpu_ci_test_results` tr

  )
SELECT DISTINCT builder_name
FROM builders
"""
    self.assertEqual(
        self.querier._GetActiveBuilderQuery(constants.BuilderTypes.CI, False),
        expected_query)

  def testInternalCi(self):
    """Tests that the active query for internal CI is as expected."""
    expected_query = """\
WITH
  builders AS (
    SELECT
      (
        SELECT value
        FROM tr.variant
        WHERE key = "builder") as builder_name
    FROM
      `chrome-luci-data.chromium.gpu_ci_test_results` tr
    UNION ALL
    SELECT
      (
        SELECT value
        FROM tr.variant
        WHERE key = "builder") as builder_name
    FROM
      `chrome-luci-data.chrome.gpu_ci_test_results` tr
  )
SELECT DISTINCT builder_name
FROM builders
"""
    self.assertEqual(
        self.querier._GetActiveBuilderQuery(constants.BuilderTypes.CI, True),
        expected_query)

  def testPublicTry(self):
    """Tests that the active query for public try is as expected."""
    expected_query = """\
WITH
  builders AS (
    SELECT
      (
        SELECT value
        FROM tr.variant
        WHERE key = "builder") as builder_name
    FROM
      `chrome-luci-data.chromium.gpu_try_test_results` tr

  )
SELECT DISTINCT builder_name
FROM builders
"""
    self.assertEqual(
        self.querier._GetActiveBuilderQuery(constants.BuilderTypes.TRY, False),
        expected_query)

  def testInternalTry(self):
    """Tests that the active query for internal try is as expected."""
    expected_query = """\
WITH
  builders AS (
    SELECT
      (
        SELECT value
        FROM tr.variant
        WHERE key = "builder") as builder_name
    FROM
      `chrome-luci-data.chromium.gpu_try_test_results` tr
    UNION ALL
    SELECT
      (
        SELECT value
        FROM tr.variant
        WHERE key = "builder") as builder_name
    FROM
      `chrome-luci-data.chrome.gpu_try_test_results` tr
  )
SELECT DISTINCT builder_name
FROM builders
"""
    self.assertEqual(
        self.querier._GetActiveBuilderQuery(constants.BuilderTypes.TRY, True),
        expected_query)


class GeneratedQueryUnittest(unittest.TestCase):
  maxDiff = None

  def testPublicCi(self):
    """Tests that the generated public CI query is as expected."""
    expected_query = """\
WITH
  builds AS (
    SELECT
      DISTINCT exported.id build_inv_id,
      partition_time
    FROM
      `chrome-luci-data.chromium.gpu_ci_test_results` tr
    WHERE
      exported.realm = "chromium:ci"
      AND STRUCT("builder", @builder_name) IN UNNEST(variant)
    ORDER BY partition_time DESC
    LIMIT @num_builds
  ),
  results AS (
    SELECT
      exported.id,
      test_id,
      status,
      (
        SELECT value
        FROM tr.tags
        WHERE key = "step_name") as step_name,
      ARRAY(
        SELECT value
        FROM tr.tags
        WHERE key = "typ_tag") as typ_tags,
      ARRAY(
        SELECT value
        FROM tr.tags
        WHERE key = "raw_typ_expectation") as typ_expectations
    FROM
      `chrome-luci-data.chromium.gpu_ci_test_results` tr,
      builds b
    WHERE
      exported.id = build_inv_id
      AND status != "SKIP"
      tfc
  )
SELECT *
FROM results
WHERE
  "Failure" IN UNNEST(typ_expectations)
  OR "RetryOnFailure" IN UNNEST(typ_expectations)
"""
    self.assertEqual(
        gpu_queries.GPU_CI_BQ_QUERY_TEMPLATE.format(builder_project='chromium',
                                                    test_filter_clause='tfc'),
        expected_query)

  def testInternalCi(self):
    """Tests that the generated internal CI query is as expected."""
    expected_query = """\
WITH
  builds AS (
    SELECT
      DISTINCT exported.id build_inv_id,
      partition_time
    FROM
      `chrome-luci-data.chrome.gpu_ci_test_results` tr
    WHERE
      exported.realm = "chrome:ci"
      AND STRUCT("builder", @builder_name) IN UNNEST(variant)
    ORDER BY partition_time DESC
    LIMIT @num_builds
  ),
  results AS (
    SELECT
      exported.id,
      test_id,
      status,
      (
        SELECT value
        FROM tr.tags
        WHERE key = "step_name") as step_name,
      ARRAY(
        SELECT value
        FROM tr.tags
        WHERE key = "typ_tag") as typ_tags,
      ARRAY(
        SELECT value
        FROM tr.tags
        WHERE key = "raw_typ_expectation") as typ_expectations
    FROM
      `chrome-luci-data.chrome.gpu_ci_test_results` tr,
      builds b
    WHERE
      exported.id = build_inv_id
      AND status != "SKIP"
      tfc
  )
SELECT *
FROM results
WHERE
  "Failure" IN UNNEST(typ_expectations)
  OR "RetryOnFailure" IN UNNEST(typ_expectations)
"""
    self.assertEqual(
        gpu_queries.GPU_CI_BQ_QUERY_TEMPLATE.format(builder_project='chrome',
                                                    test_filter_clause='tfc'),
        expected_query)

  def testPublicTry(self):
    """Tests that the generated public try query is as expected."""
    expected_query = """\
WITH
  submitted_builds AS (
    SELECT
      CONCAT("build-", CAST(unnested_builds.id AS STRING)) as id
    FROM
      `commit-queue.chromium.attempts`,
      UNNEST(builds) as unnested_builds,
      UNNEST(gerrit_changes) as unnested_changes
    WHERE
      unnested_builds.host = "cr-buildbucket.appspot.com"
      AND unnested_changes.submit_status = "SUCCESS"
      AND start_time > TIMESTAMP_SUB(CURRENT_TIMESTAMP(),
                                     INTERVAL 30 DAY)
  ),
  builds AS (
    SELECT
      DISTINCT exported.id build_inv_id,
      partition_time
    FROM
      `chrome-luci-data.chromium.gpu_try_test_results` tr,
      submitted_builds sb
    WHERE
      exported.realm = "chromium:try"
      AND STRUCT("builder", @builder_name) IN UNNEST(variant)
      AND exported.id = sb.id
    ORDER BY partition_time DESC
    LIMIT @num_builds
  ),
  results AS (
    SELECT
      exported.id,
      test_id,
      status,
      (
        SELECT value
        FROM tr.tags
        WHERE key = "step_name") as step_name,
      ARRAY(
        SELECT value
        FROM tr.tags
        WHERE key = "typ_tag") as typ_tags,
      ARRAY(
        SELECT value
        FROM tr.tags
        WHERE key = "raw_typ_expectation") as typ_expectations
    FROM
      `chrome-luci-data.chromium.gpu_try_test_results` tr,
      builds b
    WHERE
      exported.id = build_inv_id
      AND status != "SKIP"
      tfc
  )
SELECT *
FROM results
WHERE
  "Failure" IN UNNEST(typ_expectations)
  OR "RetryOnFailure" IN UNNEST(typ_expectations)
"""
    self.assertEqual(
        gpu_queries.GPU_TRY_BQ_QUERY_TEMPLATE.format(builder_project='chromium',
                                                     test_filter_clause='tfc'),
        expected_query)

  def testInternalTry(self):
    """Tests that the generated internal try query is as expected."""
    expected_query = """\
WITH
  submitted_builds AS (
    SELECT
      CONCAT("build-", CAST(unnested_builds.id AS STRING)) as id
    FROM
      `commit-queue.chromium.attempts`,
      UNNEST(builds) as unnested_builds,
      UNNEST(gerrit_changes) as unnested_changes
    WHERE
      unnested_builds.host = "cr-buildbucket.appspot.com"
      AND unnested_changes.submit_status = "SUCCESS"
      AND start_time > TIMESTAMP_SUB(CURRENT_TIMESTAMP(),
                                     INTERVAL 30 DAY)
  ),
  builds AS (
    SELECT
      DISTINCT exported.id build_inv_id,
      partition_time
    FROM
      `chrome-luci-data.chrome.gpu_try_test_results` tr,
      submitted_builds sb
    WHERE
      exported.realm = "chrome:try"
      AND STRUCT("builder", @builder_name) IN UNNEST(variant)
      AND exported.id = sb.id
    ORDER BY partition_time DESC
    LIMIT @num_builds
  ),
  results AS (
    SELECT
      exported.id,
      test_id,
      status,
      (
        SELECT value
        FROM tr.tags
        WHERE key = "step_name") as step_name,
      ARRAY(
        SELECT value
        FROM tr.tags
        WHERE key = "typ_tag") as typ_tags,
      ARRAY(
        SELECT value
        FROM tr.tags
        WHERE key = "raw_typ_expectation") as typ_expectations
    FROM
      `chrome-luci-data.chrome.gpu_try_test_results` tr,
      builds b
    WHERE
      exported.id = build_inv_id
      AND status != "SKIP"
      tfc
  )
SELECT *
FROM results
WHERE
  "Failure" IN UNNEST(typ_expectations)
  OR "RetryOnFailure" IN UNNEST(typ_expectations)
"""
    self.assertEqual(
        gpu_queries.GPU_TRY_BQ_QUERY_TEMPLATE.format(builder_project='chrome',
                                                     test_filter_clause='tfc'),
        expected_query)


class QueryGeneratorImplUnittest(unittest.TestCase):
  def testPublicCi(self):
    """Tests that public CI builders use the correct query."""
    q = gpu_queries.QueryGeneratorImpl(['tfc'],
                                       data_types.BuilderEntry(
                                           'builder', constants.BuilderTypes.CI,
                                           False))
    self.assertEqual(len(q), 1)
    expected_query = gpu_queries.GPU_CI_BQ_QUERY_TEMPLATE.format(
        builder_project='chromium', test_filter_clause='tfc')
    self.assertEqual(q[0], expected_query)

  def testInternalCi(self):
    """Tests that internal CI builders use the correct query."""
    q = gpu_queries.QueryGeneratorImpl(['tfc'],
                                       data_types.BuilderEntry(
                                           'builder', constants.BuilderTypes.CI,
                                           True))
    self.assertEqual(len(q), 1)
    expected_query = gpu_queries.GPU_CI_BQ_QUERY_TEMPLATE.format(
        builder_project='chrome', test_filter_clause='tfc')
    self.assertEqual(q[0], expected_query)

  def testPublicTry(self):
    """Tests that public try builders use the correct query."""
    q = gpu_queries.QueryGeneratorImpl(['tfc'],
                                       data_types.BuilderEntry(
                                           'builder',
                                           constants.BuilderTypes.TRY, False))
    self.assertEqual(len(q), 1)
    expected_query = gpu_queries.GPU_TRY_BQ_QUERY_TEMPLATE.format(
        builder_project='chromium', test_filter_clause='tfc')
    self.assertEqual(q[0], expected_query)

  def testInternalTry(self):
    """Tests that internal try builders use the correct query."""
    q = gpu_queries.QueryGeneratorImpl(['tfc'],
                                       data_types.BuilderEntry(
                                           'builder',
                                           constants.BuilderTypes.TRY, True))
    self.assertEqual(len(q), 1)
    expected_query = gpu_queries.GPU_TRY_BQ_QUERY_TEMPLATE.format(
        builder_project='chrome', test_filter_clause='tfc')
    self.assertEqual(q[0], expected_query)

  def testUnknownBuilderType(self):
    """Tests that an exception is raised for unknown builder types."""
    with self.assertRaises(RuntimeError):
      gpu_queries.QueryGeneratorImpl(['tfc'],
                                     data_types.BuilderEntry(
                                         'unknown_builder', 'unknown_type',
                                         False))


class GetSuiteFilterClauseUnittest(unittest.TestCase):
  def testNonWebGl(self):
    """Tests that no filter is returned for non-WebGL suites."""
    for suite in [
        'context_lost',
        'depth_capture',
        'hardware_accelerated_feature',
        'gpu_process',
        'info_collection',
        'maps',
        'pixel',
        'power',
        'screenshot_sync',
        'trace_test',
    ]:
      querier = gpu_uu.CreateGenericGpuQuerier(suite=suite)
      self.assertEqual(querier._GetSuiteFilterClause(), '')

  def testWebGl(self):
    """Tests that filters are returned for WebGL suites."""
    querier = gpu_uu.CreateGenericGpuQuerier(suite='webgl_conformance1')
    expected_filter = 'AND "webgl-version-1" IN UNNEST(typ_tags)'
    self.assertEqual(querier._GetSuiteFilterClause(), expected_filter)

    querier = gpu_uu.CreateGenericGpuQuerier(suite='webgl_conformance2')
    expected_filter = 'AND "webgl-version-2" IN UNNEST(typ_tags)'
    self.assertEqual(querier._GetSuiteFilterClause(), expected_filter)


class HelperMethodUnittest(unittest.TestCase):
  def setUp(self):
    self.instance = gpu_uu.CreateGenericGpuQuerier()

  def testStripPrefixFromTestIdValidId(self):
    test_name = 'conformance/programs/program-handling.html'
    prefix = ('ninja://chrome/test:telemetry_gpu_integration_test/'
              'gpu_tests.webgl_conformance_integration_test.'
              'WebGLConformanceIntegrationTest.')
    test_id = prefix + test_name
    self.assertEqual(self.instance._StripPrefixFromTestId(test_id), test_name)

  def testStripPrefixFromTestIdInvalidId(self):
    test_name = 'conformance/programs/program-handling_html'
    prefix = ('ninja://chrome/test:telemetry_gpu_integration_test/'
              'gpu_testse.webgl_conformance_integration_test.')
    test_id = prefix + test_name
    with self.assertRaises(AssertionError):
      self.instance._StripPrefixFromTestId(test_id)


if __name__ == '__main__':
  unittest.main(verbosity=2)
