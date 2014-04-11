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
                    'id (e.g., Android serial)', type='string')
  parser.add_option('-p', '--process_id', help='Target process id',
                    type='int')
  parser.add_option('-m', '--filter_process_name', help='Process '
                    'name to match', type='string')
  (options, args) = parser.parse_args()

  memory_inspector.RegisterAllBackends()

  if not args:
    parser.print_help()
    return -1
  if args[0] == 'devices':
    _ListDevices(options.backend)
    return 0

  number_of_devices = 0
  if options.device_id:
    device_id = options.device_id
    number_of_devices = 1
  else:
    for device in backends.ListDevices():
      if device.backend.name == options.backend:
        number_of_devices += 1
        device_id = device.id

  if number_of_devices == 0:
    print "No devices connected"
    return -1

  if number_of_devices > 1:
    print ('More than 1 device connected. You need to provide'
        ' --device_id')
    return -1

  if args[0] == 'ps':
    _ListProcesses(options.backend, device_id,
                     options.filter_process_name)
    return 0

  if args[0] == 'stats':
    if not options.process_id:
      print 'You need to provide --process_id'
      return -1
    else:
      _ListProcessStats(options.backend, device_id,
                        options.process_id)
    return 0
  else:
    print 'Invalid command entered'
    return -1


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
    return
  if not filter_process_name:
    print 'Listing all processes'
  else:
    print 'Listing processes matching ' + filter_process_name.lower()
  print ''
  device.Initialize()
  _PrintProcessHeadingLine()
  for process in device.ListProcesses():
    if (not filter_process_name or
        filter_process_name.lower() in process.name.lower()):
      stats = process.GetStats()
      _PrintProcess(process, stats)


def _ListProcessStats(backend_name, device_id, process_id):
  """Prints process stats periodically and displays an error if the
     process or device does not exist.
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


def _PrintProcessHeadingLine():
  print '%-10s : %-50s : %12s %12s %12s' % (
      'Process ID', 'Process Name', 'RUN_TIME', 'THREADS','MEM_RSS_KB')
  print ''


def _PrintProcess(process, stats):
  run_time_min, run_time_sec = divmod(stats.run_time, 60)
  print '%10s : %-50s : %6s m %2s s %8s %12s' % (
      process.pid, process.name, run_time_min, run_time_sec,
      stats.threads, stats.vm_rss)


def _PrintProcessStatsHeadingLine():
  print '%-10s : %-50s : %12s %12s %13s %12s %14s' % (
        'Process ID', 'Process Name', 'RUN_TIME', 'THREADS',
        'CPU_USAGE', 'MEM_RSS_KB', 'PAGE_FAULTS')
  print ''


def _PrintProcessStats(process, stats):
  run_time_min, run_time_sec = divmod(stats.run_time, 60)
  print '%10s : %-50s : %6s m %2s s %8s %12s %13s %11s' % (
        process.pid, process.name, run_time_min, run_time_sec,
        stats.threads, stats.cpu_usage, stats.vm_rss, stats.page_faults)


