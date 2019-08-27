#!/usr/bin/env python2
# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This script is used to generate trace from build_packages eventlog.

Usage:
  $ ./build_packages --withevents --board=${BOARD}
  $ build_trace /mnt/host/source/src/build/events/${date}.json > trace.json
  $ # load trace.json from chrome://tracing
"""

from __future__ import print_function

import argparse
import json
import sys


def get_name(event):
  name = ''
  def append(event, name, key, sep='/'):
    if key in event:
      if name != '':
        name += sep
      name += event[key]
    return name

  name = append(event, name, 'task_name')
  name = append(event, name, 'category')
  name = append(event, name, 'name')
  name = append(event, name, 'version', '-')

  return name


def main(argv):
  parser = argparse.ArgumentParser()
  parser.add_argument('eventlogfile')
  args = parser.parse_args(argv)

  with open(args.eventlogfile) as f:
    events = [json.loads(x) for x in f]

  events.sort(key=lambda event: -event['finish_time'])
  threads_starttime = []

  trace_events = []

  for event in events:
    name = get_name(event)
    start_time = int(event['start_time'] * 1e6)
    finish_time = int(event['finish_time'] * 1e6)
    dur = finish_time - start_time

    candidate = [x for x in list(enumerate(threads_starttime))
                 if finish_time <= x[1]]
    candidate.sort(key=lambda x: x[0])
    if not candidate:
      tid = len(threads_starttime)
      threads_starttime.append(start_time)
    else:
      tid = candidate[0][0]
      threads_starttime[tid] = start_time

    trace_events.append({
        'name': name,
        'ph': 'X',
        'ts': str(start_time),
        'dur': str(dur),
        'pid': '0',
        'tid': str(tid),
    })

  json.dump(trace_events, sys.stdout)


if __name__ == '__main__':
  main(sys.argv[1:])
