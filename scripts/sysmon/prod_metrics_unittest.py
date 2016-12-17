# Copyright 2016 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for summarize_build_stats."""

from __future__ import print_function

import mock

from chromite.lib import cros_test_lib
from chromite.scripts.sysmon import prod_metrics

# pylint: disable=protected-access


class TestProdHostReporter(cros_test_lib.TestCase):
  """Tests for ProdHostReporter."""

  def test_call(self):
    """Test ProdHostReporter reporting correctly."""
    servers = [
        prod_metrics.Server(hostname='sharanohiar',
                            status='primary',
                            roles='shard',
                            created='2016-01-01 00:00:00',
                            modified='2016-01-02 00:00:00',
                            note=''),
    ]
    source = mock.NonCallableMock(['get_servers'])
    source.get_servers.side_effect = lambda: iter(servers)
    sink1 = mock.NonCallableMock(['write_servers'])
    sink2 = mock.NonCallableMock(['write_servers'])

    reporter = prod_metrics._ProdHostReporter(source, [sink1, sink2])
    reporter()

    sink1.write_servers.assert_called_once_with(servers)
    sink2.write_servers.assert_called_once_with(servers)


class TestAtestSource(cros_test_lib.TestCase):
  """Tests for AtestSource."""

  def test_query_atest_for_servers(self):
    """Test _QueryAtestForServer()."""
    source = prod_metrics._AtestSource('atest')
    with mock.patch('subprocess.check_output') as check_output:
      check_output.return_value = 'dummy atest output'
      source._query_atest_for_servers()
      check_output.assert_called_once_with(
          ['atest', 'server', 'list', '--json'])

  def test_get_servers(self):
    """Test get_servers()."""
    source = prod_metrics._AtestSource(atest_program='atest')
    with mock.patch.object(source, '_query_atest_for_servers') as query:
      query.return_value = (
          '[{"status": "primary", "roles": ["shard"],'
          ' "date_modified": "2016-12-13 20:41:54",'
          ' "hostname": "chromeos-server71.cbf.corp.google.com",'
          ' "note": null,'' "date_created": "2016-12-13 20:41:54"}]')
      got = list(source.get_servers())
    self.assertEqual(got,
                     [prod_metrics.Server(
                         hostname=u'chromeos-server71.cbf.corp.google.com',
                         status=u'primary',
                         roles=(u'shard',),
                         created=u'2016-12-13 20:41:54',
                         modified=u'2016-12-13 20:41:54',
                         note=None)])


class TestTsMonSink(cros_test_lib.TestCase):
  """Tests for TsMonSink."""

  def test_write_servers(self):
    """Test write_servers()."""
    servers = [
        prod_metrics.Server(
            hostname='harvestasha-xp',
            status='primary',
            roles=frozenset(('scheduler', 'host_scheduler', 'suite_scheduler',
                             'afe')),
            created='2014-12-11 22:48:43',
            modified='2014-12-11 22:48:43',
            note=''),
        prod_metrics.Server(
            hostname='harvestasha-vista',
            status='primary',
            roles=frozenset(('devserver',)),
            created='2015-01-05 13:32:49',
            modified='2015-01-05 13:32:49',
            note=''),
    ]
    sink = prod_metrics._TsMonSink('prod_hosts/')

    presence_patch = mock.patch.object(type(sink), '_presence_metric',
                                       autospec=True)
    roles_patch = mock.patch.object(type(sink), '_roles_metric', autospec=True)
    with presence_patch as presence_metric, roles_patch as roles_metric:
      sink.write_servers(servers)

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
