#! /usr/bin/python
# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import cgi
import json
import logging
import os
import subprocess
import sys
import tempfile
import time

_SRC_DIR = os.path.abspath(os.path.join(
    os.path.dirname(__file__), '..', '..', '..'))

sys.path.append(os.path.join(_SRC_DIR, 'third_party', 'catapult', 'devil'))
from devil.android import device_utils
from devil.android.sdk import intent

sys.path.append(os.path.join(_SRC_DIR, 'build', 'android'))
import devil_chromium
from pylib import constants

import activity_lens
import content_classification_lens
import device_setup
import frame_load_lens
import loading_model
import loading_trace
import model_graph
import options
import trace_recorder


# TODO(mattcary): logging.info isn't that useful, as the whole (tools) world
# uses logging info; we need to introduce logging modules to get finer-grained
# output. For now we just do logging.warning.


OPTIONS = options.OPTIONS


def _LoadPage(device, url):
  """Load a page on chrome on our device.

  Args:
    device: an AdbWrapper for the device on which to load the page.
    url: url as a string to load.
  """
  load_intent = intent.Intent(
      package=OPTIONS.ChromePackage().package,
      activity=OPTIONS.ChromePackage().activity,
      data=url)
  logging.warning('Loading ' + url)
  device.StartActivity(load_intent, blocking=True)


def _WriteJson(output, json_data):
  """Write JSON data in a nice way.

  Args:
    output: a file object
    json_data: JSON data as a dict.
  """
  json.dump(json_data, output, sort_keys=True, indent=2)


def _GetPrefetchHtml(graph, name=None):
  """Generate prefetch page for the resources in resource graph.

  Args:
    graph: a ResourceGraph.
    name: optional string used in the generated page.

  Returns:
    HTML as a string containing all the link rel=prefetch directives necessary
    for prefetching the given ResourceGraph.
  """
  if name:
    title = 'Prefetch for ' + cgi.escape(name)
  else:
    title = 'Generated prefetch page'
  output = []
  output.append("""<!DOCTYPE html>
<html>
<head>
<title>%s</title>
""" % title)
  for info in graph.ResourceInfo():
    output.append('<link rel="prefetch" href="%s">\n' % info.Url())
  output.append("""</head>
<body>%s</body>
</html>
  """ % title)
  return '\n'.join(output)


def _LogRequests(url, clear_cache_override=None):
  """Log requests for a web page.

  Args:
    url: url to log as string.
    clear_cache_override: if not None, set clear_cache different from OPTIONS.

  Returns:
    JSON dict of logged information (ie, a dict that describes JSON).
  """
  device = device_setup.GetFirstDevice() if not OPTIONS.local else None
  clear_cache = (clear_cache_override if clear_cache_override is not None
                 else OPTIONS.clear_cache)
  with device_setup.DeviceConnection(device) as connection:
    trace = trace_recorder.MonitorUrl(connection, url, clear_cache=clear_cache)
    return trace.ToJsonDict()


def _FullFetch(url, json_output, prefetch):
  """Do a full fetch with optional prefetching."""
  if not url.startswith('http') and not url.startswith('file'):
    url = 'http://' + url
  logging.warning('Cold fetch')
  cold_data = _LogRequests(url)
  assert cold_data, 'Cold fetch failed to produce data. Check your phone.'
  if prefetch:
    assert not OPTIONS.local
    logging.warning('Generating prefetch')
    prefetch_html = _GetPrefetchHtml(
        loading_model.ResourceGraph(cold_data), name=url)
    tmp = tempfile.NamedTemporaryFile()
    tmp.write(prefetch_html)
    tmp.flush()
    # We hope that the tmpfile name is unique enough for the device.
    target = os.path.join('/sdcard/Download', os.path.basename(tmp.name))
    device = device_setup.GetFirstDevice()
    device.adb.Push(tmp.name, target)
    logging.warning('Pushed prefetch %s to device at %s' % (tmp.name, target))
    _LoadPage(device, 'file://' + target)
    time.sleep(OPTIONS.prefetch_delay_seconds)
    logging.warning('Warm fetch')
    warm_data = _LogRequests(url, clear_cache_override=False)
    with open(json_output, 'w') as f:
      _WriteJson(f, warm_data)
    logging.warning('Wrote ' + json_output)
    with open(json_output + '.cold', 'w') as f:
      _WriteJson(f, cold_data)
    logging.warning('Wrote ' + json_output + '.cold')
  else:
    with open(json_output, 'w') as f:
      _WriteJson(f, cold_data)
    logging.warning('Wrote ' + json_output)


def _ProcessRequests(filename):
  with open(filename) as f:
    trace = loading_trace.LoadingTrace.FromJsonDict(json.load(f))
    content_lens = (
        content_classification_lens.ContentClassificationLens.WithRulesFiles(
            trace, OPTIONS.ad_rules, OPTIONS.tracking_rules))
    frame_lens = frame_load_lens.FrameLoadLens(trace)
    activity = activity_lens.ActivityLens(trace)
    graph = loading_model.ResourceGraph(
        trace, content_lens, frame_lens, activity)
    if OPTIONS.noads:
      graph.Set(node_filter=graph.FilterAds)
    return graph


def InvalidCommand(cmd):
  sys.exit('Invalid command "%s"\nChoices are: %s' %
           (cmd, ' '.join(COMMAND_MAP.keys())))


def DoPng(arg_str):
  OPTIONS.ParseArgs(arg_str, description='Generates a PNG from a trace',
                    extra=['request_json', ('--png_output', ''),
                           ('--eog', False)])
  graph = _ProcessRequests(OPTIONS.request_json)
  visualization = model_graph.GraphVisualization(graph)
  tmp = tempfile.NamedTemporaryFile()
  visualization.OutputDot(tmp)
  tmp.flush()
  png_output = OPTIONS.png_output
  if not png_output:
    if OPTIONS.request_json.endswith('.json'):
      png_output = OPTIONS.request_json[
          :OPTIONS.request_json.rfind('.json')] + '.png'
    else:
      png_output = OPTIONS.request_json + '.png'
  subprocess.check_call(['dot', '-Tpng', tmp.name, '-o', png_output])
  logging.warning('Wrote ' + png_output)
  if OPTIONS.eog:
    subprocess.Popen(['eog', png_output])
  tmp.close()


def DoCompare(arg_str):
  OPTIONS.ParseArgs(arg_str, description='Compares two traces',
                    extra=['g1_json', 'g2_json'])
  g1 = _ProcessRequests(OPTIONS.g1_json)
  g2 = _ProcessRequests(OPTIONS.g2_json)
  discrepancies = loading_model.ResourceGraph.CheckImageLoadConsistency(g1, g2)
  if discrepancies:
    print '%d discrepancies' % len(discrepancies)
    print '\n'.join([str(r) for r in discrepancies])
  else:
    print 'Consistent!'


def DoPrefetchSetup(arg_str):
  OPTIONS.ParseArgs(arg_str, description='Sets up prefetch',
                    extra=['request_json', 'target_html', ('--upload', False)])
  graph = _ProcessRequests(OPTIONS.request_json)
  with open(OPTIONS.target_html, 'w') as html:
    html.write(_GetPrefetchHtml(
        graph, name=os.path.basename(OPTIONS.request_json)))
  if OPTIONS.upload:
    device = device_setup.GetFirstDevice()
    destination = os.path.join('/sdcard/Download',
                               os.path.basename(OPTIONS.target_html))
    device.adb.Push(OPTIONS.target_html, destination)

    logging.warning(
        'Pushed %s to device at %s' % (OPTIONS.target_html, destination))


def DoLogRequests(arg_str):
  OPTIONS.ParseArgs(arg_str, description='Logs requests of a load',
                    extra=['--url', '--output', ('--prefetch', False)])
  _FullFetch(url=OPTIONS.url,
             json_output=OPTIONS.output,
             prefetch=OPTIONS.prefetch)


def DoFetch(arg_str):
  OPTIONS.ParseArgs(arg_str,
                    description=('Fetches SITE into DIR with '
                                 'standard naming that can be processed by '
                                 './cost_to_csv.py.  Both warm and cold '
                                 'fetches are done.  SITE can be a full url '
                                 'but the filename may be strange so better '
                                 'to just use a site (ie, domain).'),
                    extra=['--site', '--dir'])
  if not os.path.exists(OPTIONS.dir):
    os.makedirs(OPTIONS.dir)
  _FullFetch(url=OPTIONS.site,
             json_output=os.path.join(OPTIONS.dir, OPTIONS.site + '.json'),
             prefetch=True)


def DoLongPole(arg_str):
  OPTIONS.ParseArgs(arg_str, description='Calculates long pole',
                    extra='request_json')
  graph = _ProcessRequests(OPTIONS.request_json)
  path_list = []
  cost = graph.Cost(path_list=path_list)
  print '%s (%s)' % (path_list[-1], cost)


def DoNodeCost(arg_str):
  OPTIONS.ParseArgs(arg_str,
                    description='Calculates node cost',
                    extra='request_json')
  graph = _ProcessRequests(OPTIONS.request_json)
  print sum((n.NodeCost() for n in graph.Nodes()))


def DoCost(arg_str):
  OPTIONS.ParseArgs(arg_str,
                    description='Calculates total cost',
                    extra=['request_json', ('--path', False)])
  graph = _ProcessRequests(OPTIONS.request_json)
  path_list = []
  print 'Graph cost: %s' % graph.Cost(path_list)
  if OPTIONS.path:
    for p in path_list:
      print '  ' + p.ShortName()


COMMAND_MAP = {
    'png': DoPng,
    'compare': DoCompare,
    'prefetch_setup': DoPrefetchSetup,
    'log_requests': DoLogRequests,
    'longpole': DoLongPole,
    'nodecost': DoNodeCost,
    'cost': DoCost,
    'fetch': DoFetch,
}

def main():
  logging.basicConfig(level=logging.WARNING)
  OPTIONS.AddGlobalArgument(
      'local', False,
      'run against local desktop chrome rather than device '
      '(see also --local_binary and local_profile_dir)')
  OPTIONS.AddGlobalArgument(
      'noads', False, 'ignore ad resources in modeling')
  OPTIONS.AddGlobalArgument(
      'ad_rules', '', 'AdBlocker+ ad rules file.')
  OPTIONS.AddGlobalArgument(
      'tracking_rules', '', 'AdBlocker+ tracking rules file.')
  OPTIONS.AddGlobalArgument(
      'prefetch_delay_seconds', 5,
      'delay after requesting load of prefetch page '
      '(only when running full fetch)')

  parser = argparse.ArgumentParser(description='Analyzes loading')
  parser.add_argument('command', help=' '.join(COMMAND_MAP.keys()))
  parser.add_argument('rest', nargs=argparse.REMAINDER)
  args = parser.parse_args()
  devil_chromium.Initialize()
  COMMAND_MAP.get(args.command,
                  lambda _: InvalidCommand(args.command))(args.rest)


if __name__ == '__main__':
  main()
