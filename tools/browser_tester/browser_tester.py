#!/usr/bin/python
# Copyright (c) 2011 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import glob
import optparse
import os.path
import sys
import thread
import time
import urllib

# Allow the import of third party modules
script_dir = os.path.dirname(__file__)
sys.path.append(os.path.join(script_dir, '../../../third_party/pylib/'))
sys.path.append(os.path.join(script_dir, '../../../tools/valgrind/'))

import browsertester.browserlauncher
import browsertester.rpclistener
import browsertester.server

import memcheck_analyze
import tsan_analyze

def BuildArgParser():
  usage = 'usage: %prog [options]'
  parser = optparse.OptionParser(usage)

  parser.add_option('-p', '--port', dest='port', action='store', type='int',
                    default='0', help='The TCP port the server will bind to. '
                    'The default is to pick an unused port number.')
  parser.add_option('--browser_path', dest='browser_path', action='store',
                    type='string', default=None,
                    help='Use the browser located here.')
  parser.add_option('--map_file', dest='map_files', action='append',
                    type='string', nargs=2, default=[],
                    metavar='DEST SRC',
                    help='Add file SRC to be served from the HTTP server, '
                    'to be made visible under the path DEST.')
  parser.add_option('--extra_serving_dir', dest='extra_dirs', action='append',
                    type='string', default=[],
                    metavar='DIRNAME',
                    help='Add directory DIRNAME to be served from the HTTP '
                    'server to be made visible under the root.')
  parser.add_option('--test_arg', dest='test_args', action='append',
                    type='string', nargs=2, default=[],
                    metavar='KEY VALUE',
                    help='Parameterize the test with a key/value pair.')
  parser.add_option('--redirect_url', dest='map_redirects', action='append',
                    type='string', nargs=2, default=[],
                    metavar='DEST SRC',
                    help='Add a redirect to the HTTP server, '
                    'requests for SRC will result in a redirect (302) to DEST.')
  parser.add_option('--enable_experimental_js', dest='enable_experimental_js',
                    action='store_true', default=False,
                    help='Allow use of experimental JavaScript APIs')
  parser.add_option('--prefer_portable_in_manifest',
                    dest='prefer_portable_in_manifest',
                    action='store_true', default=False,
                    help='Use portable programs in manifest if available.')
  parser.add_option('-f', '--file', dest='files', action='append',
                    type='string', default=[],
                    metavar='FILENAME',
                    help='Add a file to serve from the HTTP server, to be '
                    'made visible in the root directory.  '
                    '"--file path/to/foo.html" is equivalent to '
                    '"--map_file foo.html path/to/foo.html"')
  parser.add_option('-u', '--url', dest='url', action='store',
                    type='string', default=None,
                    help='The webpage to load.')
  parser.add_option('--ppapi_plugin', dest='ppapi_plugin', action='store',
                    type='string', default=None,
                    help='Use the browser plugin located here.')
  parser.add_option('--sel_ldr', dest='sel_ldr', action='store',
                    type='string', default=None,
                    help='Use the sel_ldr located here.')
  parser.add_option('--irt_library', dest='irt_library', action='store',
                    type='string', default=None,
                    help='Use the integrated runtime (IRT) library '
                    'located here.')
  parser.add_option('--interactive', dest='interactive', action='store_true',
                    default=False, help='Do not quit after testing is done. '
                    'Handy for iterative development.  Disables timeout.')
  parser.add_option('--debug', dest='debug', action='store_true', default=False,
                    help='Request debugging output from browser.')
  parser.add_option('--timeout', dest='timeout', action='store', type='float',
                    default=5.0,
                    help='The maximum amount of time to wait, in seconds, for '
                    'the browser to make a request. The timer resets with each '
                    'request.')
  parser.add_option('--hard_timeout', dest='hard_timeout', action='store',
                    type='float', default=None,
                    help='The maximum amount of time to wait, in seconds, for '
                    'the entire test.  This will kill runaway tests. ')
  parser.add_option('--allow_404', dest='allow_404', action='store_true',
                    default=False,
                    help='Allow 404s to occur without failing the test.')
  parser.add_option('-b', '--bandwidth', dest='bandwidth', action='store',
                    type='float', default='0.0',
                    help='The amount of bandwidth (megabits / second) to '
                    'simulate between the client and the server. This used for '
                    'replies with file payloads. All other responses are '
                    'assumed to be short. Bandwidth values <= 0.0 are assumed '
                    'to mean infinite bandwidth.')
  parser.add_option('--extension', dest='browser_extensions', action='append',
                    type='string', default=[],
                    help='Load the browser extensions located at the list of '
                    'paths. Note: this currently only works with the Chrome '
                    'browser.')
  parser.add_option('--tool', dest='tool', action='store',
                    type='string', default=None,
                    help='Run tests under a tool.')
  parser.add_option('--browser_flag', dest='browser_flags', action='append',
                    type='string', default=[],
                    help='Additional flags for the chrome command.')
  parser.add_option('--enable_ppapi_dev', dest='enable_ppapi_dev',
                    action='store', type='int', default=1,
                    help='Enable/disable PPAPI Dev interfaces while testing.')

  return parser


def ProcessToolLogs(options, logs_dir):
  if options.tool == 'memcheck':
    analyzer = memcheck_analyze.MemcheckAnalyzer('', use_gdb=True)
    logs_wildcard = 'xml.*'
  elif options.tool == 'tsan':
    analyzer = tsan_analyze.TsanAnalyzer('', use_gdb=True)
    logs_wildcard = 'log.*'
  files = glob.glob(os.path.join(logs_dir, logs_wildcard))
  retcode = analyzer.Report(files)
  return retcode


def Run(url, options):
  options.files.append(os.path.join(script_dir, 'browserdata', 'nacltest.js'))

  # Create server
  server = browsertester.server.Create('localhost', options.port)

  # If port 0 has been requested, an arbitrary port will be bound so we need to
  # query it.  Older version of Python do not set server_address correctly when
  # The requested port is 0 so we need to break encapsulation and query the
  # socket directly.
  host, port = server.socket.getsockname()

  file_mapping = dict(options.map_files)
  for filename in options.files:
    file_mapping[os.path.basename(filename)] = filename
  for server_path, real_path in file_mapping.iteritems():
    if not os.path.exists(real_path):
      raise AssertionError('\'%s\' does not exist.' % real_path)

  def ShutdownCallback():
    server.TestingEnded()
    close_browser = options.tool is not None and not options.interactive
    return close_browser

  listener = browsertester.rpclistener.RPCListener(ShutdownCallback)
  server.Configure(file_mapping,
                   dict(options.map_redirects),
                   options.allow_404,
                   options.bandwidth,
                   listener,
                   options.extra_dirs)

  browser = browsertester.browserlauncher.ChromeLauncher(options)

  full_url = 'http://%s:%d/%s' % (host, port, url)
  if len(options.test_args) > 0:
    full_url += '?' + urllib.urlencode(options.test_args)
  browser.Run(full_url, port)
  server.TestingBegun(0.125)

  # In Python 2.5, server.handle_request may block indefinitely.  Serving pages
  # is done in its own thread so the main thread can time out as needed.
  def Serve():
    while server.test_in_progress or options.interactive:
      server.handle_request()
  thread.start_new_thread(Serve, ())

  tool_failed = False
  time_started = time.time()

  def HardTimeout(total_time):
    return total_time >= 0.0 and time.time() - time_started >= total_time

  try:
    while server.test_in_progress or options.interactive:
      if not browser.IsRunning():
        listener.ServerError('Browser process ended during test '
                             '(return code %r)' % browser.GetReturnCode())
        break
      elif not options.interactive and server.TimedOut(options.timeout):
        js_time = server.TimeSinceJSHeartbeat()
        err = 'Did not hear from the test for %.1f seconds.' % options.timeout
        err += '\nHeard from Javascript %.1f seconds ago.' % js_time
        if js_time > 2.0:
          err += '\nThe renderer probably hung or crashed.'
        else:
          err += '\nThe test probably did not get a callback that it expected.'
        listener.ServerError(err)
        break
      elif not options.interactive and HardTimeout(options.hard_timeout):
        listener.ServerError('The test took over %.1f seconds.  This is '
                             'probably a runaway test.' % options.hard_timeout)
        break
      else:
        # If Python 2.5 support is dropped, stick server.handle_request() here.
        time.sleep(0.125)
    if options.tool:
      browser.WaitForProcessDeath()
      tool_failed = ProcessToolLogs(options, browser.tool_log_dir)
  finally:
    browser.Cleanup()
    server.server_close()

  if tool_failed:
    return 2
  elif listener.ever_failed:
    return 1
  else:
    return 0


def RunFromCommandLine():
  parser = BuildArgParser()
  options, args = parser.parse_args()

  if len(args) != 0:
    print args
    parser.error('Invalid arguments')

  # Validate the URL
  url = options.url
  if url is None:
    parser.error('Must specify a URL')

  if options.hard_timeout is None:
    options.hard_timeout = options.timeout * 3

  return Run(url, options)


if __name__ == '__main__':
  sys.exit(RunFromCommandLine())
