# Copyright 2016 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Prod host manifest reporting daemon.

This daemon reports metrics about the hosts on production and their
roles to Monarch.  This daemon should be run on ONE host, with access
to server_db and with Autotest installed.
"""

from __future__ import print_function

import collections
import subprocess
import sys
import time

from chromite.lib import commandline
from chromite.lib import cros_logging as logging
from chromite.lib import ts_mon_config
from chromite.lib import metrics

DEFAULT_LOG_LEVEL = 'DEBUG'
DEFAULT_INTERVAL = 600  # 10 minutes
METRIC_PREFIX = '/chrome/infra/chromeos/prod_hosts/'
ATEST_PROGRAM = '/usr/local/autotest/cli/atest'

logger = logging.getLogger(__name__)


def main(args):
  """Entry point.

  Args:
    args: Sequence of command arguments.
  """
  opts = ParseArguments(args)
  ts_mon_config.SetupTsMonGlobalState('prodmon')

  presence_metric = metrics.Boolean(METRIC_PREFIX + 'presence')
  roles_metric = metrics.String(METRIC_PREFIX + 'roles')
  reporter = ProdHostReporter(
      source=AtestSource(atest_program=ATEST_PROGRAM),
      sinks=[TsMonSink(presence_metric=presence_metric,
                       roles_metric=roles_metric),
             LoggingSink()])
  mainloop = MainLoop(interval=opts.interval,
                      func=reporter)
  mainloop.LoopForever()


def ParseArguments(args):
  """Parse command arguments.

  Args:
    args: Sequence of command arguments.
  """
  parser = commandline.ArgumentParser(
      description=__doc__,
      default_log_level=DEFAULT_LOG_LEVEL)
  parser.add_argument(
      '--interval',
      default=DEFAULT_INTERVAL,
      type=int,
      help='time (in seconds) between sampling system metrics')
  opts = parser.parse_args(args)
  opts.Freeze()
  return opts


class MainLoop(object):
  """Main daemon loop.

  MainLoop instances have a callable that is run on each loop.  The interval
  argument determines the number of seconds between each loop.
  """

  def __init__(self, interval, func):
    """Initialize instance.

    Args:
      interval: Seconds between loops.
      func: Function to call on each loop.
    """
    self._interval = interval
    self._func = func

  def LoopOnce(self):
    """Run loop once."""
    try:
      self._func()
    except Exception:
      logger.exception('Error during metric gathering.')

  def LoopForever(self):
    """Run loop forever."""
    while True:
      self.LoopOnce()
      time.sleep(self._interval)


class ProdHostReporter(object):
  """Prod host metrics reporter.

  ProdHostReporter takes source and sinks arguments.  Sources have a
  GetServers() method that returns prod host information as an iterable of
  Server instances.  Sinks have a WriteServers() method for reporting the
  server information.
  """

  def __init__(self, source, sinks=()):
    """Initialize instance.

    Args:
      source: Source for getting prod host information.
      sinks: Sinks for writing prod host information.
    """
    self._source = source
    self._sinks = sinks

  def __call__(self):
    """Report prod hosts."""
    servers = list(self._source.GetServers())
    for sink in self._sinks:
      sink.WriteServers(servers)


class _TableParser(object):
  """Parse simple tables.

  Each table row is one line of text, with each column in the row separated by
  a column separator character.  Any whitespace around the separators are
  ignored.
  """

  def __init__(self, sep):
    """Initialize instance.

    Args:
      sep: Separator character.
    """
    self._sep = sep

  def Parse(self, lines):
    """Parse table as output by atest.

    Args:
      lines: Iterable of table lines as strings.

    Returns:
      Generator of dicts mapping column names to row values.
    """
    rows = (self._SplitTableRow(line) for line in lines)
    col_names = next(rows)
    for row in rows:
      yield dict(zip(col_names, row))

  def _SplitTableRow(self, line):
    """Split up columns in one table row.

    Args:
      line: Line representing table row.

    Returns:
      List of column values as strings.
    """
    return [cell.strip() for cell in line.split(self._sep)]


class _AtestParser(object):
  """Parse `atest server list --table` output."""

  _table_parser = _TableParser(sep='|')

  def Parse(self, lines):
    """Parse output lines.

    Args:
      lines: Iterable of lines.

    Returns:
      Generator of Server instances.
    """
    for row in self._table_parser.Parse(lines):
      yield self._FormatRow(row)

  def _FormatRow(self, row):
    """Format a row into a Server instance.

    Args:
      row: A table row as a dict.

    Returns:
      Server instance.
    """
    return Server(
        hostname=row['Hostname'],
        status=row['Status'],
        roles=frozenset(row['Roles'].split(',')),
        # TODO(ayatane): Parse these into datetimes if/when these are used.
        created=row['Date Created'],
        modified=row['Date Modified'],
        note=row['Note'])


class AtestSource(object):
  """Source for prod host information, using atest."""

  _default_parser = _AtestParser()

  def __init__(self, atest_program, parser=_default_parser):
    """Initialize instance.

    Args:
      atest_program: atest program as full path or name on the search path.
      parser: Parser to use for atest output.
    """
    self._atest_program = atest_program
    self._parser = parser

  def _QueryAtestForServers(self):
    """Run atest to get host information.

    Returns:
      atest output as a string (specifically, a bytestring)
    """
    return subprocess.check_output(
        [self._atest_program, 'server', 'list', '--table'])

  def GetServers(self):
    """Get server information from this source.

    Returns:
      Iterable of Server instances.
    """
    output = self._QueryAtestForServers()
    output_lines = output.splitlines()
    hosts = self._parser.Parse(output_lines)
    return hosts


Server = collections.namedtuple('Server', [
    'hostname', 'status', 'roles', 'created', 'modified', 'note',
])


class TsMonSink(object):
  """Sink using ts_mon to report Monarch metrics."""

  def __init__(self, presence_metric, roles_metric):
    """Initialize instance.

    Args:
      presence_metric: BooleanMetric instance for reporting host presence.
      roles_metric: RolesMetric instance for reporting host roles.
    """
    self._presence_metric = presence_metric
    self._roles_metric = roles_metric

  def WriteServers(self, servers):
    """Write server information.

    Args:
      servers: Iterable of Server instances.
    """
    for server in servers:
      fields = {
          'target_hostname': server.hostname,
      }
      self._presence_metric.set(True, fields)
      self._roles_metric.set(self._FormatRoles(server.roles), fields)

  def _FormatRoles(self, roles):
    return ','.join(sorted(roles))


class LoggingSink(object):
  """Sink using Python's logging facilities."""

  def WriteServers(self, servers):
    """Write server information.

    Args:
      servers: Iterable of Server instances.
    """
    for server in servers:
      logger.debug('Server: %r', server)


if __name__ == '__main__':
  main(sys.argv[1:])
