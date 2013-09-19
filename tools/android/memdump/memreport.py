#!/usr/bin/env python
#
# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import collections
import json
import optparse
import os
import re
import threading
import time
import sys

from sets import Set
from string import Template

sys.path.append(os.path.join(sys.path[0], os.pardir, os.pardir, os.pardir,
                             'build','android'))
from pylib import android_commands
from pylib import constants


_ENTRIES = [
    ('Total', '.* r... '),
    ('Read-only', '.* r--. '),
    ('Read-write', '.* rw.. '),
    ('Executable', '.* ..x. '),
    ('Anonymous total', '.* ""'),
    ('Anonymous read-write', '.* rw.. .* ""'),
    ('Anonymous executable (JIT\'ed code)', '.* ..x. .* ""'),
    ('File total', '.* .... .* "/.*"'),
    ('File read-write', '.* rw.. .* "/.*"'),
    ('File executable', '.* ..x. .* "/.*"'),
    ('/dev files', '.* r... .* "/dev/.*"'),
    ('Dalvik', '.* rw.. .* "/.*dalvik.*"'),
    ('Dalvik heap', '.* rw.. .* "/.*dalvik-heap.*"'),
    ('Native heap (malloc)', '.* r... .* ".*malloc.*"'),
    ('Ashmem', '.* rw.. .* "/dev/ashmem '),
    ('Native library total', '.* r... .* "/data/app-lib/'),
    ('Native library read-only', '.* r--. .* "/data/app-lib/'),
    ('Native library read-write', '.* rw-. .* "/data/app-lib/'),
    ('Native library executable', '.* r.x. .* "/data/app-lib/'),
]


def _CollectMemoryStats(memdump, region_filters):
  processes = []
  mem_usage_for_regions = None
  regexps = {}
  for region_filter in region_filters:
    regexps[region_filter] = re.compile(region_filter)
  for line in memdump:
    if 'PID=' in line:
      mem_usage_for_regions = {}
      processes.append(mem_usage_for_regions)
      continue
    matched_regions = Set([])
    for region_filter in region_filters:
      if regexps[region_filter].match(line.rstrip('\r\n')):
        matched_regions.add(region_filter)
        if not region_filter in mem_usage_for_regions:
          mem_usage_for_regions[region_filter] = {
              'private_unevictable': 0,
              'private': 0,
              'shared_app': 0.0,
              'shared_app_unevictable': 0.0,
              'shared_other_unevictable': 0,
              'shared_other': 0,
          }
    for matched_region in matched_regions:
      mem_usage = mem_usage_for_regions[matched_region]
      for key in mem_usage:
        for token in line.split(' '):
          if (key + '=') in token:
            field = token.split('=')[1]
            if key != 'shared_app':
              mem_usage[key] += int(field)
            else:  # shared_app=[\d:\d,\d:\d...]
              array = field[1:-1].split(',')
              for i in xrange(len(array)):
                shared_app, shared_app_unevictable = array[i].split(':')
                mem_usage['shared_app'] += float(shared_app) / (i + 2)
                mem_usage['shared_app_unevictable'] += \
                    float(shared_app_unevictable) / (i + 2)
            break
  return processes


def _ConvertMemoryField(field):
  return str(field / (1024.0 * 1024))


def _DumpCSV(processes_stats):
  total_map = {}
  i = 0
  for process in processes_stats:
    i += 1
    print (',Process ' + str(i) + ',private,private_unevictable,shared_app,' +
           'shared_app_unevictable,shared_other,shared_other_unevictable,')
    for (k, v) in _ENTRIES:
      if not v in process:
        print ',' + k + ',0,0,0,0,0,'
        continue
      if not v in total_map:
        total_map[v] = {'resident':0, 'unevictable':0}
      total_map[v]['resident'] += (process[v]['private'] +
                                   process[v]['shared_app'])
      total_map[v]['unevictable'] += process[v]['private_unevictable'] + \
          process[v]['shared_app_unevictable']
      print (
          ',' + k + ',' +
          _ConvertMemoryField(process[v]['private']) + ',' +
          _ConvertMemoryField(process[v]['private_unevictable']) + ',' +
          _ConvertMemoryField(process[v]['shared_app']) + ',' +
          _ConvertMemoryField(process[v]['shared_app_unevictable']) + ',' +
          _ConvertMemoryField(process[v]['shared_other']) + ',' +
          _ConvertMemoryField(process[v]['shared_other_unevictable']) + ','
          )
    print ''

  for (k, v) in _ENTRIES:
    if not v in total_map:
      print ',' + k + ',0,0,'
      continue
    print (',' + k + ',' + _ConvertMemoryField(total_map[v]['resident']) + ',' +
           _ConvertMemoryField(total_map[v]['unevictable']) + ',')
  print ''


def _RunManualGraph(package_name, interval):
  _AREA_TYPES = ('private', 'private_unevictable', 'shared_app',
                 'shared_app_unevictable', 'shared_other',
                 'shared_other_unevictable')
  all_pids = {}
  legends = ['Seconds'] + [entry + '_' + area
                           for entry, _ in _ENTRIES
                           for area in _AREA_TYPES]
  should_quit = threading.Event()

  def _GenerateGraph():
    _HTML_TEMPLATE = """
<html>
  <head>
    <script type='text/javascript' src='https://www.google.com/jsapi'></script>
    <script type='text/javascript'>
      google.load('visualization', '1', {packages:['corechart', 'table']});
      google.setOnLoadCallback(createPidSelector);
      var pids = $JSON_PIDS;
      var pids_info = $JSON_PIDS_INFO;
      function drawVisualization(pid) {
        var data = google.visualization.arrayToDataTable(
          pids_info[pid]
        );

        var charOptions = {
          title: 'Memory Report (KB) for ' + pid,
          vAxis: {title: 'Time',  titleTextStyle: {color: 'red'}},
          isStacked : true
        };

        var chart = new google.visualization.BarChart(
            document.getElementById('chart_div'));
        chart.draw(data, charOptions);

        var table = new google.visualization.Table(
            document.getElementById('table_div'));
        table.draw(data);
      }

      function createPidSelector() {
        var pid_selector = document.getElementById('pid_selector');
        for (pid in pids) {
          var option = document.createElement('option');
          option.text = option.value = pids[pid];
          pid_selector.appendChild(option);
        }
        pid_selector.addEventListener('change',
          function() {
            drawVisualization(this.selectedOptions[0].value);
          }
        );
        drawVisualization(pids[0]);
      }
    </script>
  </head>
  <body>
    PIDS: <select id='pid_selector'></select>
    <div id='chart_div' style="width: 1024px; height: 800px;"></div>
    <div id='table_div' style="width: 1024px; height: 640px;"></div>
  </body>
</html>
"""
    pids = sorted(all_pids.keys())
    pids_info = dict(zip(pids,
                         [ [legends] +
                           all_pids[p] for p in pids
                         ]))
    print Template(_HTML_TEMPLATE).safe_substitute({
        'JSON_PIDS': json.dumps(pids),
        'JSON_PIDS_INFO': json.dumps(pids_info)
    })



  def _CollectStats(count):
    adb = android_commands.AndroidCommands()
    pid_list = adb.ExtractPid(package_name)
    memdump = adb.RunShellCommand('/data/local/tmp/memdump ' +
                                  ' '.join(pid_list))
    process_stats = _CollectMemoryStats(memdump,
                                        [value for (key, value) in _ENTRIES])
    for (pid, process) in zip(pid_list, process_stats):
      first_pid_entry = True
      for (k, v) in _ENTRIES:
        if v not in process:
          continue
        for area_type in _AREA_TYPES:
          legend = k + '_' + area_type
          if pid not in all_pids:
            all_pids[pid] = []
          if first_pid_entry:
            all_pids[pid].append(['%ds' % (count * interval)] +
                                 [0] * (len(legends) - 1))
            first_pid_entry = False
          mem_kb = process[v][area_type] / 1024
          all_pids[pid][-1][legends.index(legend)] = mem_kb

  def _Loop():
    count = 0
    while not should_quit.is_set():
      print >>sys.stderr, 'Collecting ', count
      _CollectStats(count)
      count += 1
      should_quit.wait(interval)

  t = threading.Thread(target=_Loop)


  print >>sys.stderr, 'Press enter or CTRL+C to stop'
  t.start()
  try:
    _ = raw_input()
  except KeyboardInterrupt:
    pass
  finally:
    should_quit.set()

  t.join()

  _GenerateGraph()


def main(argv):
  parser = optparse.OptionParser(usage='Usage: %prog [options]',
                                 description=__doc__)
  parser.add_option('-m',
                    '--manual-graph',
                    action='store_true',
                    help='Manually collect data and generate a graph.')
  parser.add_option('-p',
                    '--package',
                    default=constants.PACKAGE_INFO['chrome'].package,
                    help='Package name to collect.')
  parser.add_option('-i',
                    '--interval',
                    default=5,
                    type='int',
                    help='Interval in seconds for manual collections.')
  options, args = parser.parse_args(argv)
  if options.manual_graph:
    return _RunManualGraph(options.package, options.interval)
  _DumpCSV(_CollectMemoryStats(sys.stdin, [value for (key, value) in _ENTRIES]))


if __name__ == '__main__':
  main(sys.argv)
