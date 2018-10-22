# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import contextlib
import pandas  # pylint: disable=import-error
import sqlite3

from soundwave import dashboard_api
from soundwave import pandas_sqlite
from soundwave import tables
from soundwave import worker_pool


def _FetchBugsWorker(args):
  api = dashboard_api.PerfDashboardCommunicator(args)
  con = sqlite3.connect(args.database_file, timeout=10)

  def Process(bug_id):
    bugs = tables.bugs.DataFrameFromJson(api.GetBugData(bug_id))
    pandas_sqlite.InsertOrReplaceRecords(con, 'bugs', bugs)

  worker_pool.Process = Process


@contextlib.contextmanager
def _ApiAndDbSession(args):
  """Context manage a session with API and DB connections.

  Ensures API has necessary credentials and DB tables have been initialized.
  """
  api = dashboard_api.PerfDashboardCommunicator(args)
  con = sqlite3.connect(args.database_file)

  # Tell sqlite to use a write-ahead log, which drastically increases its
  # concurrency capabilities. This helps prevent 'database is locked' exceptions
  # when we have many workers writing to a single database. This mode is sticky,
  # so we only need to set it once and future connections will automatically
  # use the log. More details are available at https://www.sqlite.org/wal.html.
  con.execute('PRAGMA journal_mode=WAL')

  try:
    tables.CreateIfNeeded(con)
    yield api, con
  finally:
    con.close()


def FetchAlertsData(args):
  with _ApiAndDbSession(args) as (api, con):
    # Get alerts.
    num_alerts = 0
    bug_ids = set()
    # TODO: This loop may be slow when fetching thousands of alerts, needs a
    # better progress indicator.
    for data in api.IterAlertData(args.benchmark, args.sheriff, args.days):
      alerts = tables.alerts.DataFrameFromJson(data)
      pandas_sqlite.InsertOrReplaceRecords(con, 'alerts', alerts)
      num_alerts += len(alerts)
      bug_ids.update(alerts['bug_id'].unique())
    print '%d alerts found!' % num_alerts

    # Get set of bugs associated with those alerts.
    bug_ids.discard(0)  # A bug_id of 0 means untriaged.
    print '%d bugs found!' % len(bug_ids)

    # Filter out bugs already in cache.
    if args.use_cache:
      known_bugs = set(
          b for b in bug_ids if tables.bugs.Get(con, b) is not None)
      if known_bugs:
        print '(skipping %d bugs already in the database)' % len(known_bugs)
        bug_ids.difference_update(known_bugs)

  # Use worker pool to fetch bug data.
  total_seconds = worker_pool.Run(
      'Fetching data of %d bugs: ' % len(bug_ids),
      _FetchBugsWorker, args, bug_ids)
  print '[%.1f bugs per second]' % (len(bug_ids) / total_seconds)


def _IterStaleTestPaths(con, test_paths):
  """Iterate over test_paths yielding only those with stale or absent data.

  A test_path is considered to be stale if the most recent data point we have
  for it in the db is more than a day older.
  """
  a_day_ago = pandas.Timestamp.utcnow() - pandas.Timedelta(days=1)
  a_day_ago = a_day_ago.tz_convert(tz=None)

  for test_path in test_paths:
    latest = tables.timeseries.GetMostRecentPoint(con, test_path)
    if latest is None or latest['timestamp'] < a_day_ago:
      yield test_path


def _FetchTimeseriesWorker(args):
  api = dashboard_api.PerfDashboardCommunicator(args)
  con = sqlite3.connect(args.database_file, timeout=10)

  def Process(test_path):
    data = api.GetTimeseries(test_path, days=args.days)
    if data:
      timeseries = tables.timeseries.DataFrameFromJson(data)
      pandas_sqlite.InsertOrReplaceRecords(con, 'timeseries', timeseries)

  worker_pool.Process = Process


def _ReadTestPathsFromFile(filename):
  with open(filename, 'rU') as f:
    for line in f:
      line = line.strip()
      if line and not line.startswith('#'):
        yield line


def FetchTimeseriesData(args):
  def _MatchesAllFilters(test_path):
    return all(f in test_path for f in args.filters)

  with _ApiAndDbSession(args) as (api, con):
    # Get test_paths.
    if args.benchmark is not None:
      api = dashboard_api.PerfDashboardCommunicator(args)
      test_paths = api.ListTestPaths(args.benchmark, sheriff=args.sheriff)
    elif args.input_file is not None:
      test_paths = list(_ReadTestPathsFromFile(args.input_file))
    else:
      raise NotImplementedError('Expected --benchmark or --input-file')

    # Apply --filter's to test_paths.
    if args.filters:
      test_paths = filter(_MatchesAllFilters, test_paths)
    num_found = len(test_paths)
    print '%d test paths found!' % num_found

    # Filter out test_paths already in cache.
    if args.use_cache:
      test_paths = list(_IterStaleTestPaths(con, test_paths))
      num_skipped = num_found - len(test_paths)
      if num_skipped:
        print '(skipping %d test paths already in the database)' % num_skipped

  # Use worker pool to fetch test path data.
  total_seconds = worker_pool.Run(
      'Fetching data of %d timeseries: ' % len(test_paths),
      _FetchTimeseriesWorker, args, test_paths)
  print '[%.1f test paths per second]' % (len(test_paths) / total_seconds)
