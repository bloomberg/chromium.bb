# Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for summarize_build_stats."""

from __future__ import print_function

import mock

from chromite.lib import cros_test_lib
from chromite.scripts import prodmon

# pylint: disable=protected-access


class TestMainLoop(cros_test_lib.TestCase):
  """Tests for MainLoop."""

  def test_LoopOnce_except(self):
    """Test LoopOnce() catching exceptions."""
    loop = prodmon.MainLoop(interval=0, func=lambda: 1 / 0)
    try:
      loop.LoopOnce()
    except ZeroDivisionError:
      self.fail('Exception in loop not caught.')


class TestProdHostReporter(cros_test_lib.TestCase):
  """Tests for ProdHostReporter."""

  def test_call(self):
    """Test ProdHostReporter reporting correctly."""
    servers = [
        prodmon.Server(hostname='sharanohiar',
                       status='primary',
                       roles='shard',
                       created='2016-01-01 00:00:00',
                       modified='2016-01-02 00:00:00',
                       note=''),
    ]
    source = mock.NonCallableMock(['GetServers'])
    source.GetServers.side_effect = lambda: iter(servers)
    sink1 = mock.NonCallableMock(['WriteServers'])
    sink2 = mock.NonCallableMock(['WriteServers'])

    reporter = prodmon.ProdHostReporter(source, [sink1, sink2])
    reporter()

    sink1.WriteServers.assert_called_once_with(servers)
    sink2.WriteServers.assert_called_once_with(servers)


class TestTableParser(cros_test_lib.TestCase):
  """Tests for _TableParser."""

  def test_Parse(self):
    """Test Parse()."""
    parser = prodmon._TableParser('|')
    got = parser.Parse(iter([
        '   first name    |   last name  ',
        '   sophie    |   neuenmuller  ',
        'prachta |   ',
    ]))
    self.assertEqual(list(got), [
        {'first name': 'sophie',
         'last name': 'neuenmuller'},
        {'first name': 'prachta',
         'last name': ''},
    ])


class TestAtestParser(cros_test_lib.TestCase):
  """Tests for _AtestParser."""

  def test_Parse(self):
    """Test Parse()."""
    parser = prodmon._AtestParser()
    got = parser.Parse(iter([
        # pylint: disable=line-too-long
        'Hostname                       | Status  | Roles                | Date Created        | Date Modified       | Note',
        'harvestasha-xp | primary | scheduler,host_scheduler,suite_scheduler,afe | 2014-12-11 22:48:43 | 2014-12-11 22:48:43 | ',
        'harvestasha-vista | primary | devserver            | 2015-01-05 13:32:49 | 2015-01-05 13:32:49 | ',
    ]))
    self.assertEqual(list(got), [
        prodmon.Server(
            hostname='harvestasha-xp',
            status='primary',
            roles=frozenset(('scheduler', 'host_scheduler', 'suite_scheduler',
                             'afe')),
            created='2014-12-11 22:48:43',
            modified='2014-12-11 22:48:43',
            note=''),
        prodmon.Server(
            hostname='harvestasha-vista',
            status='primary',
            roles=frozenset(('devserver',)),
            created='2015-01-05 13:32:49',
            modified='2015-01-05 13:32:49',
            note=''),
    ])


class TestAtestSource(cros_test_lib.TestCase):
  """Tests for AtestSource."""

  def test_QueryAtestForServers(self):
    """Test _QueryAtestForServer()."""
    source = prodmon.AtestSource('atest')
    with mock.patch('subprocess.check_output') as check_output:
      check_output.return_value = 'dummy atest output'
      source._QueryAtestForServers()
      check_output.assert_called_once_with(
          ['atest', 'server', 'list', '--table'])

  def test_GetServers(self):
    """Test GetServers()."""
    parser = mock.NonCallableMock(['Parse'])
    parser.Parse.side_effect = lambda lines: iter(['parsed ' + line
                                                   for line in lines])
    source = prodmon.AtestSource(atest_program='atest', parser=parser)
    with mock.patch.object(source, '_QueryAtestForServers') as query:
      query.return_value = 'host1\nhost2\nhost3\n'
      got = source.GetServers()
    self.assertEqual(list(got),
                     ['parsed host1', 'parsed host2', 'parsed host3'])


class TestTsMonSink(cros_test_lib.TestCase):
  """Tests for TsMonSink."""

  def test_WriteServers(self):
    """Test WriteServers()."""
    servers = [
        prodmon.Server(
            hostname='harvestasha-xp',
            status='primary',
            roles=frozenset(('scheduler', 'host_scheduler', 'suite_scheduler',
                             'afe')),
            created='2014-12-11 22:48:43',
            modified='2014-12-11 22:48:43',
            note=''),
        prodmon.Server(
            hostname='harvestasha-vista',
            status='primary',
            roles=frozenset(('devserver',)),
            created='2015-01-05 13:32:49',
            modified='2015-01-05 13:32:49',
            note=''),
    ]
    presence_metric = mock.NonCallableMock(['set'])
    roles_metric = mock.NonCallableMock(['set'])
    sink = prodmon.TsMonSink(
        presence_metric=presence_metric,
        roles_metric=roles_metric)

    sink.WriteServers(servers)

    self.assertEqual(
        presence_metric.set.call_args_list,
        [
            mock.call(True, {'target_hostname': 'harvestasha-xp'}),
            mock.call(True, {'target_hostname': 'harvestasha-vista'}),
        ])
    self.assertEqual(
        roles_metric.set.call_args_list,
        [
            mock.call('afe,host_scheduler,scheduler,suite_scheduler',
                      {'target_hostname': 'harvestasha-xp'}),
            mock.call('devserver', {'target_hostname': 'harvestasha-vista'}),
        ])
