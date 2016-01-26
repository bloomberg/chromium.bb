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

import content_classification_lens
import device_setup
import loading_model
import loading_trace
import trace_recorder


# TODO(mattcary): logging.info isn't that useful, as the whole (tools) world
# uses logging info; we need to introduce logging modules to get finer-grained
# output. For now we just do logging.warning.


# TODO(mattcary): probably we want this piped in through a flag.
CHROME = constants.PACKAGE_INFO['chrome']


def _LoadPage(device, url):
  """Load a page on chrome on our device.

  Args:
    device: an AdbWrapper for the device on which to load the page.
    url: url as a string to load.
  """
  load_intent = intent.Intent(
      package=CHROME.package, activity=CHROME.activity, data=url)
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


def _LogRequests(url, clear_cache=True, local=False):
  """Log requests for a web page.

  Args:
    url: url to log as string.
    clear_cache: optional flag to clear the cache.
    local: log from local (desktop) chrome session.

  Returns:
    JSON dict of logged information (ie, a dict that describes JSON).
  """
  device = device_setup.GetFirstDevice() if not local else None
  with device_setup.DeviceConnection(device) as connection:
    trace = trace_recorder.MonitorUrl(connection, url, clear_cache=clear_cache)
    return trace.ToJsonDict()


def _FullFetch(url, json_output, prefetch, local, prefetch_delay_seconds):
  """Do a full fetch with optional prefetching."""
  if not url.startswith('http'):
    url = 'http://' + url
  logging.warning('Cold fetch')
  cold_data = _LogRequests(url, local=local)
  assert cold_data, 'Cold fetch failed to produce data. Check your phone.'
  if prefetch:
    assert not local
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
    time.sleep(prefetch_delay_seconds)
    logging.warning('Warm fetch')
    warm_data = _LogRequests(url, clear_cache=False)
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


# TODO(mattcary): it would be nice to refactor so the --noads flag gets dealt
# with here.
def _ProcessRequests(filename, ad_rules_filename='',
                     tracking_rules_filename=''):
  with open(filename) as f:
    trace = loading_trace.LoadingTrace.FromJsonDict(json.load(f))
    content_lens = (
        content_classification_lens.ContentClassificationLens.WithRulesFiles(
            trace, ad_rules_filename, tracking_rules_filename))
    return loading_model.ResourceGraph(trace, content_lens)


def InvalidCommand(cmd):
  sys.exit('Invalid command "%s"\nChoices are: %s' %
           (cmd, ' '.join(COMMAND_MAP.keys())))


def DoCost(arg_str):
  parser = argparse.ArgumentParser(description='Tabulates cost')
  parser.add_argument('request_json')
  parser.add_argument('--parameter', nargs='*', default=[])
  parser.add_argument('--path', action='store_true')
  parser.add_argument('--noads', action='store_true')
  args = parser.parse_args(arg_str)
  graph = _ProcessRequests(args.request_json)
  for p in args.parameter:
    graph.Set(**{p: True})
  path_list = []
  if args.noads:
    graph.Set(node_filter=graph.FilterAds)
  print 'Graph cost: ' + str(graph.Cost(path_list))
  if args.path:
    for p in path_list:
      print '  ' + p.ShortName()


def DoPng(arg_str):
  parser = argparse.ArgumentParser(
      description='Generates a PNG from a trace')
  parser.add_argument('request_json')
  parser.add_argument('png_output', nargs='?')
  parser.add_argument('--eog', action='store_true')
  parser.add_argument('--highlight')
  parser.add_argument('--noads', action='store_true')
  parser.add_argument('--ad_rules', default='')
  parser.add_argument('--tracking_rules', default='')
  args = parser.parse_args(arg_str)
  graph = _ProcessRequests(
      args.request_json, args.ad_rules, args.tracking_rules)
  if args.noads:
    graph.Set(node_filter=graph.FilterAds)
  tmp = tempfile.NamedTemporaryFile()
  graph.MakeGraphviz(
      tmp,
      highlight=args.highlight.split(',') if args.highlight else None)
  tmp.flush()
  png_output = args.png_output
  if not png_output:
    if args.request_json.endswith('.json'):
      png_output = args.request_json[:args.request_json.rfind('.json')] + '.png'
    else:
      png_output = args.request_json + '.png'
  subprocess.check_call(['dot', '-Tpng', tmp.name, '-o', png_output])
  logging.warning('Wrote ' + png_output)
  if args.eog:
    subprocess.Popen(['eog', png_output])
  tmp.close()


def DoCompare(arg_str):
  parser = argparse.ArgumentParser(description='Compares two traces')
  parser.add_argument('g1_json')
  parser.add_argument('g2_json')
  args = parser.parse_args(arg_str)
  g1 = _ProcessRequests(args.g1_json)
  g2 = _ProcessRequests(args.g2_json)
  discrepancies = loading_model.ResourceGraph.CheckImageLoadConsistency(g1, g2)
  if discrepancies:
    print '%d discrepancies' % len(discrepancies)
    print '\n'.join([str(r) for r in discrepancies])
  else:
    print 'Consistent!'


def DoPrefetchSetup(arg_str):
  parser = argparse.ArgumentParser(description='Sets up prefetch')
  parser.add_argument('request_json')
  parser.add_argument('target_html')
  parser.add_argument('--upload', action='store_true')
  args = parser.parse_args(arg_str)
  graph = _ProcessRequests(args.request_json)
  with open(args.target_html, 'w') as html:
    html.write(_GetPrefetchHtml(
        graph, name=os.path.basename(args.request_json)))
  if args.upload:
    device = device_setup.GetFirstDevice()
    destination = os.path.join('/sdcard/Download',
                               os.path.basename(args.target_html))
    device.adb.Push(args.target_html, destination)

    logging.warning(
        'Pushed %s to device at %s' % (args.target_html, destination))


def DoLogRequests(arg_str):
  parser = argparse.ArgumentParser(description='Logs requests of a load')
  parser.add_argument('--url', required=True)
  parser.add_argument('--output', required=True)
  parser.add_argument('--prefetch', action='store_true')
  parser.add_argument('--prefetch_delay_seconds', type=int, default=5)
  parser.add_argument('--local', action='store_true')
  args = parser.parse_args(arg_str)
  _FullFetch(url=args.url,
             json_output=args.output,
             prefetch=args.prefetch,
             prefetch_delay_seconds=args.prefetch_delay_seconds,
             local=args.local)


def DoFetch(arg_str):
  parser = argparse.ArgumentParser(description='Fetches SITE into DIR with '
                                   'standard naming that can be processed by '
                                   './cost_to_csv.py.  Both warm and cold '
                                   'fetches are done.  SITE can be a full url '
                                   'but the filename may be strange so better '
                                   'to just use a site (ie, domain).')
  # Arguments are flags as it's easy to get the wrong order of site vs dir.
  parser.add_argument('--site', required=True)
  parser.add_argument('--dir', required=True)
  parser.add_argument('--prefetch_delay_seconds', type=int, default=5)
  args = parser.parse_args(arg_str)
  if not os.path.exists(args.dir):
    os.makedirs(args.dir)
  _FullFetch(url=args.site,
             json_output=os.path.join(args.dir, args.site + '.json'),
             prefetch=True,
             prefetch_delay_seconds=args.prefetch_delay_seconds,
             local=False)


def DoLongPole(arg_str):
  parser = argparse.ArgumentParser(description='Calculates long pole')
  parser.add_argument('request_json')
  parser.add_argument('--noads', action='store_true')
  args = parser.parse_args(arg_str)
  graph = _ProcessRequests(args.request_json)
  if args.noads:
    graph.Set(node_filter=graph.FilterAds)
  path_list = []
  cost = graph.Cost(path_list=path_list)
  print '%s (%s)' % (path_list[-1], cost)


def DoNodeCost(arg_str):
  parser = argparse.ArgumentParser(description='Calculates node cost')
  parser.add_argument('request_json')
  parser.add_argument('--noads', action='store_true')
  args = parser.parse_args(arg_str)
  graph = _ProcessRequests(args.request_json)
  if args.noads:
    graph.Set(node_filter=graph.FilterAds)
  print sum((n.NodeCost() for n in graph.Nodes()))


COMMAND_MAP = {
    'cost': DoCost,
    'png': DoPng,
    'compare': DoCompare,
    'prefetch_setup': DoPrefetchSetup,
    'log_requests': DoLogRequests,
    'longpole': DoLongPole,
    'nodecost': DoNodeCost,
    'fetch': DoFetch,
}

def main():
  logging.basicConfig(level=logging.WARNING)
  parser = argparse.ArgumentParser(description='Analyzes loading')
  parser.add_argument('command', help=' '.join(COMMAND_MAP.keys()))
  parser.add_argument('rest', nargs=argparse.REMAINDER)
  args = parser.parse_args()
  devil_chromium.Initialize()
  COMMAND_MAP.get(args.command,
                  lambda _: InvalidCommand(args.command))(args.rest)


if __name__ == '__main__':
  main()
