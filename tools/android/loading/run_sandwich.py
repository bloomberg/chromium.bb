#! /usr/bin/env python
# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Instructs Chrome to load series of web pages and reports results.

When running Chrome is sandwiched between preprocessed disk caches and
WepPageReplay serving all connections.

TODO(pasko): implement cache preparation and WPR.
"""

import argparse
import logging
import os
import sys

_SRC_DIR = os.path.abspath(os.path.join(
    os.path.dirname(__file__), '..', '..', '..'))

sys.path.append(os.path.join(_SRC_DIR, 'third_party', 'catapult', 'devil'))
from devil.android import device_utils

sys.path.append(os.path.join(_SRC_DIR, 'build', 'android'))
import devil_chromium

import device_setup
import devtools_monitor
import json
import page_track
import tracing


_JOB_SEARCH_PATH = 'sandwich_jobs'


def _ReadUrlsFromJobDescription(job_name):
  """Retrieves the list of URLs associated with the job name."""
  try:
    # Extra sugar: attempt to load from a relative path.
    json_file_name = os.path.join(os.path.dirname(__file__), _JOB_SEARCH_PATH,
        job_name)
    with open(json_file_name) as f:
      json_data = json.load(f)
  except IOError:
    # Attempt to read by regular file name.
    with open(job_name) as f:
      json_data = json.load(f)

  key = 'urls'
  if json_data and key in json_data:
    url_list = json_data[key]
    if isinstance(url_list, list) and len(url_list) > 0:
      return url_list
  raise Exception('Job description does not define a list named "urls"')


def _SaveChromeTrace(events, directory, subdirectory):
  """Saves the trace events, ignores IO errors.

  Args:
    events: a dict as returned by TracingTrack.ToJsonDict()
    directory: directory name contining all traces
    subdirectory: directory name to create this particular trace in
  """
  target_directory = os.path.join(directory, subdirectory)
  file_name = os.path.join(target_directory, 'trace.json')
  try:
    os.makedirs(target_directory)
    with open(file_name, 'w') as f:
      json.dump({'traceEvents': events['events'], 'metadata': {}}, f)
  except IOError:
    logging.warning('Could not save a trace: %s' % file_name)
    # Swallow the exception.


def main():
  logging.basicConfig(level=logging.INFO)
  devil_chromium.Initialize()

  parser = argparse.ArgumentParser()
  parser.add_argument('--job', required=True,
                      help='JSON file with job description.')
  parser.add_argument('--output', required=True,
                      help='Name of output directory to create.')
  parser.add_argument('--repeat', default=1, type=int,
                      help='How many times to run the job')
  args = parser.parse_args()

  try:
    os.makedirs(args.output)
  except OSError:
    logging.error('Cannot create directory for results: %s' % args.output)
    raise

  job_urls = _ReadUrlsFromJobDescription(args.job)
  device = device_utils.DeviceUtils.HealthyDevices()[0]
  pages_loaded = 0
  for _ in xrange(args.repeat):
    for url in job_urls:
      with device_setup.DeviceConnection(device) as connection:
        page_track.PageTrack(connection)
        tracing_track = tracing.TracingTrack(connection,
            categories='blink,cc,netlog,renderer.scheduler,toplevel,v8')
        connection.SetUpMonitoring()
        connection.SendAndIgnoreResponse('Page.navigate', {'url': url})
        connection.StartMonitoring()
        pages_loaded += 1
        _SaveChromeTrace(tracing_track.ToJsonDict(), args.output,
            str(pages_loaded))


if __name__ == '__main__':
  sys.exit(main())
