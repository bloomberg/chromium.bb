#! /usr/bin/python
# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A simple http server for running local integration tests.

This chooses a port dynamically and so can communicate that back to its spawner
via a named pipe at --fifo. Sources are served from the tree named at
--source_dir.
"""


import argparse
import cgi
import os.path
import logging
import re
import time
import wsgiref.simple_server


class ServerApp(object):
  """WSGI App.

  Dispatches by matching, in order, against GetPaths.
  """
  def __init__(self, source_dir):
    self._source_dir = source_dir

  def __call__(self, environ, start_response):
    """WSGI dispatch.

    Args:
      environ: environment list.
      start_response: WSGI response start.

    Returns:
      Iterable server result.
    """
    path = environ.get('PATH_INFO', '')
    while path.startswith('/'):
      path = path[1:]
    filename = os.path.join(self._source_dir, path)
    if not os.path.exists(filename):
      logging.info('%s not found', filename)
      start_response('404 Not Found', [('Content-Type', 'text/html')])
      return ["""<!DOCTYPE html>
<html>
<head>
<title>Not Found</title>
<body>%s not found</body>
</html>""" % path]

    logging.info('responding with %s', filename)
    suffix = path[path.rfind('.') + 1:]
    start_response('200 OK', [('Content-Type',
                               {'css': 'text/css',
                                'html': 'text/html',
                                'jpg': 'image/jpeg',
                                'js': 'text/javascript',
                                'png': 'image/png',
                                'ttf': 'font/ttf',
                              }[suffix])])
    return file(filename).read()


if __name__ == '__main__':
  logging.basicConfig(level=logging.INFO)
  parser = argparse.ArgumentParser()
  parser.add_argument('--fifo', default=None,
                      help='Named pipe used to communicate port')
  parser.add_argument('--source_dir', required=True,
                      help='Directory holding sources to serve.')
  args = parser.parse_args()
  server_app = ServerApp(args.source_dir)
  server = wsgiref.simple_server.make_server(
      'localhost', 0, server_app)
  ip, port = server.server_address
  logging.info('Listening on port %s at %s', port, args.source_dir)
  if args.fifo:
    fifo = file(args.fifo, 'w')
    fifo.write('%s\n' % port)
    fifo.flush()
    fifo.close()
  server.serve_forever()
