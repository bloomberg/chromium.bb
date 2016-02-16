#! /usr/bin/env python
# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Pull a sandwich run's output directory's metrics from traces into a CSV.

python pull_sandwich_metrics.py -h
"""

import argparse
import csv
import json
import logging
import os
import sys


CATEGORIES = ['blink.user_timing', 'disabled-by-default-memory-infra']

_CSV_FIELD_NAMES = [
    'id',
    'url',
    'total_load',
    'onload',
    'browser_malloc_avg',
    'browser_malloc_max']

_TRACKED_EVENT_NAMES = set(['requestStart', 'loadEventStart', 'loadEventEnd'])


def _GetBrowserPID(trace):
  """Get the browser PID from a trace.

  Args:
    trace: The cached trace.

  Returns:
    The browser's PID as an integer.
  """
  for event in trace['traceEvents']:
    if event['cat'] != '__metadata' or event['name'] != 'process_name':
      continue
    if event['args']['name'] == 'Browser':
      return event['pid']
  raise ValueError('couldn\'t find browser\'s PID')


def _GetBrowserDumpEvents(trace):
  """Get the browser memory dump events from a trace.

  Args:
    trace: The cached trace.

  Returns:
    List of memory dump events.
  """
  browser_pid = _GetBrowserPID(trace)
  browser_dumps_events = []
  for event in trace['traceEvents']:
    if event['cat'] != 'disabled-by-default-memory-infra':
      continue
    if event['ph'] != 'v' or event['name'] != 'periodic_interval':
      continue
    # Ignore dump events for processes other than the browser process
    if event['pid'] != browser_pid:
      continue
    browser_dumps_events.append(event)
  if len(browser_dumps_events) == 0:
    raise ValueError('No browser dump events found.')
  return browser_dumps_events


def _GetWebPageTrackedEvents(trace):
  """Get the web page's tracked events from a trace.

  Args:
    trace: The cached trace.

  Returns:
    Dictionary all tracked events.
  """
  main_frame = None
  tracked_events = {}
  for event in trace['traceEvents']:
    if event['cat'] != 'blink.user_timing':
      continue
    event_name = event['name']
    # Ignore events until about:blank's unloadEventEnd that give the main
    # frame id.
    if not main_frame:
      if event_name == 'unloadEventEnd':
        main_frame = event['args']['frame']
        logging.info('found about:blank\'s event \'unloadEventEnd\'')
      continue
    # Ignore sub-frames events. requestStart don't have the frame set but it
    # is fine since tracking the first one after about:blank's unloadEventEnd.
    if 'frame' in event['args'] and event['args']['frame'] != main_frame:
      continue
    if event_name in _TRACKED_EVENT_NAMES and event_name not in tracked_events:
      logging.info('found url\'s event \'%s\'' % event_name)
      tracked_events[event_name] = event
  assert len(tracked_events) == len(_TRACKED_EVENT_NAMES)
  return tracked_events


def _PullMetricsFromTrace(trace):
  """Pulls all the metrics from a given trace.

  Args:
    trace: The cached trace.

  Returns:
    Dictionary with all _CSV_FIELD_NAMES's field set (except the 'id').
  """
  browser_dump_events = _GetBrowserDumpEvents(trace)
  web_page_tracked_events = _GetWebPageTrackedEvents(trace)

  browser_malloc_sum = 0
  browser_malloc_max = 0
  for dump_event in browser_dump_events:
    attr = dump_event['args']['dumps']['allocators']['malloc']['attrs']['size']
    assert attr['units'] == 'bytes'
    size = int(attr['value'], 16)
    browser_malloc_sum += size
    browser_malloc_max = max(browser_malloc_max, size)

  return {
    'total_load': (web_page_tracked_events['loadEventEnd']['ts'] -
                   web_page_tracked_events['requestStart']['ts']),
    'onload': (web_page_tracked_events['loadEventEnd']['ts'] -
               web_page_tracked_events['loadEventStart']['ts']),
    'browser_malloc_avg': browser_malloc_sum / float(len(browser_dump_events)),
    'browser_malloc_max': browser_malloc_max
  }


def _PullMetricsFromOutputDirectory(output_directory_path):
  """Pulls all the metrics from all the traces of a sandwich run directory.

  Args:
    output_directory_path: The sandwich run's output directory to pull the
        metrics from.

  Returns:
    List of dictionaries with all _CSV_FIELD_NAMES's field set.
  """
  assert os.path.isdir(output_directory_path)
  run_infos = None
  with open(os.path.join(output_directory_path, 'run_infos.json')) as f:
    run_infos = json.load(f)
  assert run_infos
  metrics = []
  for node_name in os.listdir(output_directory_path):
    if not os.path.isdir(os.path.join(output_directory_path, node_name)):
      continue
    try:
      page_id = int(node_name)
    except ValueError:
      continue
    trace_path = os.path.join(output_directory_path, node_name, 'trace.json')
    if not os.path.isfile(trace_path):
      continue
    logging.info('processing \'%s\'' % trace_path)
    with open(trace_path) as trace_file:
      trace = json.load(trace_file)
      trace_metrics = _PullMetricsFromTrace(trace)
      trace_metrics['id'] = page_id
      trace_metrics['url'] = run_infos['urls'][page_id]
      metrics.append(trace_metrics)
  assert len(metrics) > 0, ('Looks like \'{}\' was not a sandwich ' +
                            'run directory.').format(output_directory_path)
  return metrics


def main():
  logging.basicConfig(level=logging.INFO)

  parser = argparse.ArgumentParser()
  parser.add_argument('output', type=str,
                      help='Output directory of run_sandwich.py command.')
  args = parser.parse_args()

  trace_metrics_list = _PullMetricsFromOutputDirectory(args.output)
  trace_metrics_list.sort(key=lambda e: e['id'])
  cs_file_path = os.path.join(args.output, 'trace_analysis.csv')
  with open(cs_file_path, 'w') as csv_file:
    writer = csv.DictWriter(csv_file, fieldnames=_CSV_FIELD_NAMES)
    writer.writeheader()
    for trace_metrics in trace_metrics_list:
      writer.writerow(trace_metrics)
  return 0


if __name__ == '__main__':
  sys.exit(main())
