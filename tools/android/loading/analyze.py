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

file_dir = os.path.dirname(__file__)
sys.path.append(os.path.join(file_dir, '..', '..', '..', 'build', 'android'))

from pylib import constants
from pylib.device import device_utils
from pylib.device import intent

import log_parser
import log_requests
import loading_model


# TODO(mattcary): logging.info isn't that useful; we need something finer
# grained. For now we just do logging.warning.


# TODO(mattcary): probably we want this piped in through a flag.
CHROME = constants.PACKAGE_INFO['chrome']


def _SetupAndGetDevice():
  """Gets an android device, set up the way we like it.

  Returns:
    An AdbWrapper for the first device found.
  """
  device = device_utils.DeviceUtils.HealthyDevices()[0]
  device.EnableRoot()
  device.KillAll(CHROME.package, quiet=True)
  return device


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

  TODO(mattcary): loading.log_requests probably needs to be refactored as we're
  using private methods, also there's ugliness like _ResponseDataToJson return a
  json.dumps that we immediately json.loads.

  Args:
    url: url to log as string.
    clear_cache: optional flag to clear the cache.
    local: log from local (desktop) chrome session.

  Returns:
    JSON of logged information (ie, a dict that describes JSON).
  """
  device = _SetupAndGetDevice() if not local else None
  request_logger = log_requests.AndroidRequestsLogger(device)
  logging.warning('Logging %scached %s' % ('un' if clear_cache else '', url))
  response_data = request_logger.LogPageLoad(
      url, clear_cache, 'chrome')
  return json.loads(log_requests._ResponseDataToJson(response_data))


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
    prefetch_html = _GetPrefetchHtml(_ProcessJson(cold_data), name=url)
    tmp = tempfile.NamedTemporaryFile()
    tmp.write(prefetch_html)
    tmp.flush()
    # We hope that the tmpfile name is unique enough for the device.
    target = os.path.join('/sdcard/Download', os.path.basename(tmp.name))
    device = _SetupAndGetDevice()
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
def _ProcessRequests(filename):
  requests = log_parser.FilterRequests(log_parser.ParseJsonFile(filename))
  return loading_model.ResourceGraph(requests)


def _ProcessJson(json_data):
  assert json_data
  return loading_model.ResourceGraph(log_parser.FilterRequests(
      [log_parser.RequestData.FromDict(r) for r in json_data]))


def InvalidCommand(cmd):
  sys.exit('Invalid command "%s"\nChoices are: %s' %
           (cmd, ' '.join(COMMAND_MAP.keys())))


def DoCost(arg_str):
  parser = argparse.ArgumentParser(usage='cost [--parameter ...] REQUEST_JSON')
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
      usage='png [--eog] [--highlight X[,...] REQUEST_JSON [PNG_OUTPUT]')
  parser.add_argument('request_json')
  parser.add_argument('png_output', nargs='?')
  parser.add_argument('--eog', action='store_true')
  parser.add_argument('--highlight')
  parser.add_argument('--noads', action='store_true')
  args = parser.parse_args(arg_str)
  graph = _ProcessRequests(args.request_json)
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
  parser = argparse.ArgumentParser(usage='compare REQUEST_JSON REQUEST_JSON')
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
  parser = argparse.ArgumentParser(
      usage='prefetch_setup [--upload] REQUEST_JSON TARGET_HTML')
  parser.add_argument('request_json')
  parser.add_argument('target_html')
  parser.add_argument('--upload', action='store_true')
  args = parser.parse_args(arg_str)
  graph = _ProcessRequests(args.request_json)
  with open(args.target_html, 'w') as html:
    html.write(_GetPrefetchHtml(
        graph, name=os.path.basename(args.request_json)))
  if args.upload:
    device = _SetupAndGetDevice()
    destination = os.path.join('/sdcard/Download',
                               os.path.basename(args.target_html))
    device.adb.Push(args.target_html, destination)

    logging.warning(
        'Pushed %s to device at %s' % (args.target_html, destination))


def DoLogRequests(arg_str):
  parser = argparse.ArgumentParser(
      usage='log_requests [--prefetch] --site URL --output JSON_OUTPUT')
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
  parser = argparse.ArgumentParser(usage='fetch --site SITE --dir DIR\n'
                                   'Fetches SITE into DIR with standard naming '
                                   'that can be processed by ./cost_to_csv.py. '
                                   'Both warm and cold fetches are done. '
                                   'SITE can be a full url but the filename '
                                   'may be strange so better to just use a '
                                   'site (ie, domain).')
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


def DoTracing(arg_str):
  parser = argparse.ArgumentParser(
      usage='tracing URL JSON_OUTPUT')
  parser.add_argument('url')
  parser.add_argument('json_output')
  args = parser.parse_args(arg_str)
  device = _SetupAndGetDevice()
  request_logger = log_requests.AndroidRequestsLogger(device)
  tracing = request_logger.LogTracing(args.url)
  with open(args.json_output, 'w') as f:
    _WriteJson(f, tracing)
  logging.warning('Wrote ' + args.json_output)


def DoLongPole(arg_str):
  parser = argparse.ArgumentParser(usage='longpole [--noads] REQUEST_JSON')
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
  parser = argparse.ArgumentParser(usage='nodecost [--noads] REQUEST_JSON')
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
    'tracing': DoTracing,
    'longpole': DoLongPole,
    'nodecost': DoNodeCost,
    'fetch': DoFetch,
}

def main():
  logging.basicConfig(level=logging.WARNING)
  parser = argparse.ArgumentParser(usage=' '.join(COMMAND_MAP.keys()))
  parser.add_argument('command')
  parser.add_argument('rest', nargs=argparse.REMAINDER)
  args = parser.parse_args()
  COMMAND_MAP.get(args.command,
                  lambda _: InvalidCommand(args.command))(args.rest)


if __name__ == '__main__':
  main()
