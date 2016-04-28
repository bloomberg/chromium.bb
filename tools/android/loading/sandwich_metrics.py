# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Pull a sandwich run's output directory's metrics from traces into a CSV.

python pull_sandwich_metrics.py -h
"""

import collections
import logging
import os
import shutil
import sys
import tempfile

_SRC_DIR = os.path.abspath(os.path.join(
    os.path.dirname(__file__), '..', '..', '..'))

sys.path.append(os.path.join(_SRC_DIR, 'tools', 'perf'))
from chrome_telemetry_build import chromium_config

sys.path.append(chromium_config.GetTelemetryDir())
from telemetry.internal.image_processing import video
from telemetry.util import image_util
from telemetry.util import rgba_color

import loading_trace as loading_trace_module
import sandwich_runner
import tracing


CSV_FIELD_NAMES = [
    'repeat_id',
    'url',
    'total_load',
    'js_onload_event',
    'browser_malloc_avg',
    'browser_malloc_max',
    'speed_index',
    'net_emul.name', # Should be in emulation.NETWORK_CONDITIONS.keys()
    'net_emul.download',
    'net_emul.upload',
    'net_emul.latency']

_UNAVAILABLE_CSV_VALUE = 'unavailable'

_TRACKED_EVENT_NAMES = set(['requestStart', 'loadEventStart', 'loadEventEnd'])

# Points of a completeness record.
#
# Members:
#   |time| is in milliseconds,
#   |frame_completeness| value representing how complete the frame is at a given
#     |time|. Caution: this completeness might be negative.
CompletenessPoint = collections.namedtuple('CompletenessPoint',
    ('time', 'frame_completeness'))


def _GetBrowserPID(tracing_track):
  """Get the browser PID from a trace.

  Args:
    tracing_track: The tracing.TracingTrack.

  Returns:
    The browser's PID as an integer.
  """
  for event in tracing_track.GetEvents():
    if event.category != '__metadata' or event.name != 'process_name':
      continue
    if event.args['name'] == 'Browser':
      return event.pid
  raise ValueError('couldn\'t find browser\'s PID')


def _GetBrowserDumpEvents(tracing_track):
  """Get the browser memory dump events from a tracing track.

  Args:
    tracing_track: The tracing.TracingTrack.

  Returns:
    List of memory dump events.
  """
  assert sandwich_runner.MEMORY_DUMP_CATEGORY in tracing_track.Categories()
  browser_pid = _GetBrowserPID(tracing_track)
  browser_dumps_events = []
  for event in tracing_track.GetEvents():
    if event.category != 'disabled-by-default-memory-infra':
      continue
    if event.type != 'v' or event.name != 'periodic_interval':
      continue
    # Ignore dump events for processes other than the browser process
    if event.pid != browser_pid:
      continue
    browser_dumps_events.append(event)
  if len(browser_dumps_events) == 0:
    raise ValueError('No browser dump events found.')
  return browser_dumps_events


def _GetWebPageTrackedEvents(tracing_track):
  """Get the web page's tracked events from a tracing track.

  Args:
    tracing_track: The tracing.TracingTrack.

  Returns:
    Dictionary all tracked events.
  """
  main_frame = None
  tracked_events = {}
  for event in tracing_track.GetEvents():
    if event.category != 'blink.user_timing':
      continue
    event_name = event.name
    # Ignore events until about:blank's unloadEventEnd that give the main
    # frame id.
    if not main_frame:
      if event_name == 'unloadEventEnd':
        main_frame = event.args['frame']
        logging.info('found about:blank\'s event \'unloadEventEnd\'')
      continue
    # Ignore sub-frames events. requestStart don't have the frame set but it
    # is fine since tracking the first one after about:blank's unloadEventEnd.
    if 'frame' in event.args and event.args['frame'] != main_frame:
      continue
    if event_name in _TRACKED_EVENT_NAMES and event_name not in tracked_events:
      logging.info('found url\'s event \'%s\'' % event_name)
      tracked_events[event_name] = event
  assert len(tracked_events) == len(_TRACKED_EVENT_NAMES)
  return tracked_events


def _ExtractDefaultMetrics(loading_trace):
  """Extracts all the default metrics from a given trace.

  Args:
    loading_trace: loading_trace_module.LoadingTrace.

  Returns:
    Dictionary with all trace extracted fields set.
  """
  web_page_tracked_events = _GetWebPageTrackedEvents(
      loading_trace.tracing_track)
  return {
    'total_load': (web_page_tracked_events['loadEventEnd'].start_msec -
                   web_page_tracked_events['requestStart'].start_msec),
    'js_onload_event': (web_page_tracked_events['loadEventEnd'].start_msec -
                        web_page_tracked_events['loadEventStart'].start_msec)
  }


def _ExtractMemoryMetrics(loading_trace):
  """Extracts all the memory metrics from a given trace.

  Args:
    loading_trace: loading_trace_module.LoadingTrace.

  Returns:
    Dictionary with all trace extracted fields set.
  """
  if (sandwich_runner.MEMORY_DUMP_CATEGORY not in
          loading_trace.tracing_track.Categories()):
    return {
      'browser_malloc_avg': _UNAVAILABLE_CSV_VALUE,
      'browser_malloc_max': _UNAVAILABLE_CSV_VALUE
    }
  browser_dump_events = _GetBrowserDumpEvents(loading_trace.tracing_track)
  browser_malloc_sum = 0
  browser_malloc_max = 0
  for dump_event in browser_dump_events:
    attr = dump_event.args['dumps']['allocators']['malloc']['attrs']['size']
    assert attr['units'] == 'bytes'
    size = int(attr['value'], 16)
    browser_malloc_sum += size
    browser_malloc_max = max(browser_malloc_max, size)
  return {
    'browser_malloc_avg': browser_malloc_sum / float(len(browser_dump_events)),
    'browser_malloc_max': browser_malloc_max
  }


def _ExtractCompletenessRecordFromVideo(video_path):
  """Extracts the completeness record from a video.

  The video must start with a filled rectangle of orange (RGB: 222, 100, 13), to
  give the view-port size/location from where to compute the completeness.

  Args:
    video_path: Path of the video to extract the completeness list from.

  Returns:
    list(CompletenessPoint)
  """
  video_file = tempfile.NamedTemporaryFile()
  shutil.copy(video_path, video_file.name)
  video_capture = video.Video(video_file)

  histograms = [
      (time, image_util.GetColorHistogram(
          image, ignore_color=rgba_color.WHITE, tolerance=8))
      for time, image in video_capture.GetVideoFrameIter()
  ]

  start_histogram = histograms[1][1]
  final_histogram = histograms[-1][1]
  total_distance = start_histogram.Distance(final_histogram)

  def FrameProgress(histogram):
    if total_distance == 0:
      if histogram.Distance(final_histogram) == 0:
        return 1.0
      else:
        return 0.0
    return 1 - histogram.Distance(final_histogram) / total_distance

  return [(time, FrameProgress(hist)) for time, hist in histograms]


def ComputeSpeedIndex(completeness_record):
  """Computes the speed-index from a completeness record.

  Args:
    completeness_record: list(CompletenessPoint)

  Returns:
    Speed-index value.
  """
  speed_index = 0.0
  last_time = completeness_record[0][0]
  last_completness = completeness_record[0][1]
  for time, completeness in completeness_record:
    if time < last_time:
      raise ValueError('Completeness record must be sorted by timestamps.')
    elapsed = time - last_time
    speed_index += elapsed * (1.0 - last_completness)
    last_time = time
    last_completness = completeness
  return speed_index


def _ExtractMetricsFromRunDirectory(run_directory_path):
  """Extracts all the metrics from traces and video of a sandwich run.

  Args:
    run_directory_path: Path of the run directory.

  Returns:
    Dictionary of extracted metrics.
  """
  trace_path = os.path.join(run_directory_path, 'trace.json')
  logging.info('processing trace \'%s\'' % trace_path)
  loading_trace = loading_trace_module.LoadingTrace.FromJsonFile(trace_path)
  run_metrics = {'url': loading_trace.url}
  run_metrics.update(_ExtractDefaultMetrics(loading_trace))
  run_metrics.update(_ExtractMemoryMetrics(loading_trace))
  video_path = os.path.join(run_directory_path, 'video.mp4')
  if os.path.isfile(video_path):
    logging.info('processing speed-index video \'%s\'' % video_path)
    completeness_record = _ExtractCompletenessRecordFromVideo(video_path)
    run_metrics['speed_index'] = ComputeSpeedIndex(completeness_record)
  else:
    run_metrics['speed_index'] = _UNAVAILABLE_CSV_VALUE
  for key, value in loading_trace.metadata['network_emulation'].iteritems():
    run_metrics['net_emul.' + key] = value
  return run_metrics


def ExtractMetricsFromRunnerOutputDirectory(output_directory_path):
  """Extracts all the metrics from all the traces of a sandwich runner output
  directory.

  Args:
    output_directory_path: The sandwich runner's output directory to extract the
        metrics from.

  Returns:
    List of dictionaries.
  """
  assert os.path.isdir(output_directory_path)
  metrics = []
  for node_name in os.listdir(output_directory_path):
    if not os.path.isdir(os.path.join(output_directory_path, node_name)):
      continue
    try:
      repeat_id = int(node_name)
    except ValueError:
      continue
    run_directory_path = os.path.join(output_directory_path, node_name)
    run_metrics = _ExtractMetricsFromRunDirectory(run_directory_path)
    run_metrics['repeat_id'] = repeat_id
    assert set(run_metrics.keys()) == set(CSV_FIELD_NAMES)
    metrics.append(run_metrics)
  assert len(metrics) > 0, ('Looks like \'{}\' was not a sandwich runner ' +
                            'output directory.').format(output_directory_path)
  return metrics
