# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Command line frontend for Memory Inspector"""

import memory_inspector
import optparse
import time

from memory_inspector.core import backends


def main(argv):
  usage = '%prog [options] devices | ps | stats'
  parser = optparse.OptionParser(usage=usage)
  parser.add_option('-b', '--backend', help='Backend name '
                    '(e.g., Android)', type='string', default='Android')
  parser.add_option('-s', '--device_id', help='Device '
                    'id (e.g., Android serial)', type='string',)
  parser.add_option('-p', '--process_id', help='Target process id',
                    type='int',)
  parser.add_option('-m', '--filter_process_name', help='Process '
                    'name to match', type='string')
  (options, args) = parser.parse_args()

  memory_inspector.RegisterAllBackends()

  if not args:
    parser.print_help()
    return -1
  if args[0] == 'devices':
    _ListDevices(options.backend)
  elif args[0] == 'ps':
    if not options.device_id:
      print 'You need to specify a DEVICE'
      return -1
    else:
      _ListProcesses(options.backend, options.device_id,
                     options.filter_process_name)
  elif args[0] == 'stats':
    if not options.device_id or not options.process_id:
      print 'You need to specify a DEVICE-ID and PROCESS-ID'
      return -1
    else:
      _ListProcessStats(options.backend, options.device_id,
                        options.process_id)
  else:
    print 'Invalid command entered'
    return -1
  print ''


def _ListDevices(backend_name):
  print 'Device list:'
  print ''
  for device in backends.ListDevices():
    if device.backend.name == backend_name:
      print '%-16s : %s' % (device.id, device.name)


def _ListProcesses(backend_name, device_id, filter_process_name):
  device = backends.GetDevice(backend_name, device_id)
  if not device:
    print 'Device', device_id, 'does not exist'
  else:
    if not filter_process_name:
      print 'Listing all processes'
    else:
      print 'Listing processes matching ' + filter_process_name.lower()
    print ''
    device.Initialize()
    _PrintProcessStatsHeadingLine()
    for process in device.ListProcesses():
      if (not filter_process_name or
          filter_process_name.lower() in process.name.lower()):
        stats = process.GetStats()
        _PrintProcessStats(process, stats)


def _ListProcessStats(backend_name, device_id, process_id):
  """Prints process stats periodically and displays an error if the
     process or device does not exist
  """
  device = backends.GetDevice(backend_name, device_id)
  if not device:
    print 'Device', device_id, 'does not exist'
  else:
    device.Initialize()
    process = device.GetProcess(process_id)
    if not process:
      print 'Cannot find process [%d] on device %s' % (
            process_id, device_id)
      return
    print 'Stats for process: [%d] %s' % (process_id, process.name)
    _PrintProcessStatsHeadingLine()
    while True:
      stats = process.GetStats()
      _PrintProcessStats(process, stats)
      time.sleep(1)


def _PrintProcessStatsHeadingLine():
  print '%-10s : %-50s : %12s %12s %13s %10s %14s' % (
        'Process ID', 'Process Name', 'RUN_TIME', 'THREADS',
        'CPU_USAGE', 'MEM_RSS_KB', 'PAGE_FAULTS')
  print ''


def _PrintProcessStats(process, stats):
  run_time_min, run_time_sec = divmod(stats.run_time, 60)
  print '%10s : %-50s : %6s m %2s s %8s %12s %12s %11s' % (
        process.pid, process.name, run_time_min, run_time_sec,
        stats.threads, stats.cpu_usage, stats.vm_rss, stats.page_faults)


