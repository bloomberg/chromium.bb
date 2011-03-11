#!/usr/bin/python
# Copyright 2010 The Native Client Authors.  All rights reserved.
# Use of this source code is governed by a BSD-style license that can
# be found in the LICENSE file.

import optparse
import os.path
import sys

# Allow the import of third party modules
script_dir = os.path.dirname(__file__)
sys.path.append(os.path.join(script_dir, '../../../third_party/pylib/'))

import browsertester.browserlauncher
import browsertester.rpclistener
import browsertester.server


def BuildArgParser():
  usage = 'usage: %prog [options]'
  parser = optparse.OptionParser(usage)

  parser.add_option('-p', '--port', dest='port', action='store', type='int',
                    default='0', help='The TCP port the server will bind to. '
                    'The default is to pick an unused port number.')
  parser.add_option('--browser_path', dest='browser_path', action='store',
                    type='string', default=None,
                    help='Use the browser located here.')
  parser.add_option('-f', '--file', dest='files', action='append',
                    type='string', default=[],
                    help='Add a file to serve from the HTTP server.')
  parser.add_option('-u', '--url', dest='url', action='store',
                    type='string', default=None,
                    help='The webpage to load.')
  parser.add_option('--ppapi_plugin', dest='ppapi_plugin', action='store',
                    type='string', default=None,
                    help='Use the browser plugin located here.')
  parser.add_option('--sel_ldr', dest='sel_ldr', action='store',
                    type='string', default=None,
                    help='Use the sel_ldr located here.')
  parser.add_option('--interactive', dest='interactive', action='store_true',
                    default=False, help='Do not quit after testing is done. '
                    'Handy for iterative development.  Disables timeout.')
  parser.add_option('--debug', dest='debug', action='store_true', default=False,
                    help='Request debugging output from browser.')
  parser.add_option('--timeout', dest='timeout', action='store', type='float',
                    default=5.0,
                    help='The maximum amount of time to wait for the browser to'
                    ' make a request. The timer resets with each request.')
  parser.add_option('--allow_404', dest='allow_404', action='store_true',
                    default=False,
                    help='Allow 404s to occur without failing the test.')

  return parser


def Run(url, options):
  # Create server
  server = browsertester.server.Create('localhost', options.port)

  # If port 0 has been requested, an arbitrary port will be bound so we need to
  # query it.  Older version of Python do not set server_address correctly when
  # The requested port is 0 so we need to break encapsulation and query the
  # socket directly.
  host, port = server.socket.getsockname()

  listener = browsertester.rpclistener.RPCListener(server.TestingEnded)
  server.Configure(options, listener)

  browser = browsertester.browserlauncher.ChromeLauncher(options.browser_path,
                                                         options.debug)
  if options.ppapi_plugin is not None:
    browser.SetPPAPIPlugin(options.ppapi_plugin)
  if options.sel_ldr is not None:
    browser.SetSelLdr(options.sel_ldr)

  browser.Run('http://%s:%d/%s' % (host, port, url))
  server.TestingBegun(0.125)

  try:
    while server.test_in_progress or options.interactive:
      if not browser.IsRunning():
        listener.ServerError('Browser process ended during test')
        break
      elif not options.interactive and server.TimedOut(options.timeout):
        listener.ServerError('Did not hear from the browser for %.1f seconds' %
                             options.timeout)
        break
      else:
        server.handle_request()
  finally:
    browser.Cleanup()
    server.server_close()

  if listener.ever_failed:
    return 1
  else:
    return 0


def RunFromCommandLine():
  parser = BuildArgParser()
  options, args = parser.parse_args()

  if len(args) != 0:
    parser.error('Invalid arguments')

  # Validate the URL
  url = options.url
  if url is None:
    parser.error('Must specify a URL')
  if not (url.endswith('.html') or url.endswith('.htm')):
    parser.error('URL must be a HTML file.')

  # Look for files in the browserdata directory as a last resort
  options.files.append(os.path.join(script_dir, 'browserdata', 'nacltest.js'))

  # Validate the roots
  for root_file in options.files:
    if not os.path.exists(root_file):
      parser.error('\'%s\' does not exist.' % root_file)

  sys.exit(Run(url, options))


if __name__ == '__main__':
  RunFromCommandLine()
