#!/usr/bin/python
# Copyright 2009 The Native Client Authors.  All rights reserved.
# Use of this source code is governed by a BSD-style license that can
# be found in the LICENSE file.

# A simple selenium tester for nacl.
# You should be able to self test the selenium setup using:
# ./selenium_tester.py  --url self.test.html

import logging
import optparse
import os
import platform
import SimpleHTTPServer
import select
import socket
import SocketServer
import subprocess
import sys
import threading
import time

######################################################################
# Config
######################################################################

# NOTE: this is the third_party dir which is a sibling of native_client/
_SELENIUM_PATH = os.path.join(os.path.dirname(sys.argv[0]),
                             '../../third_party/selenium')
_SELENIUM_PATH = os.path.abspath(_SELENIUM_PATH)

sys.path.append(os.path.join(_SELENIUM_PATH,
                             'selenium-python-client-driver-1.0.1'))
import selenium


_DEFAULT_SELENIUM_JAR = os.path.join(_SELENIUM_PATH,
                                     'selenium-server-1.0.1',
                                     'selenium-server.jar'
                                     )

if platform.system() == 'Windows':
  _DEFAULT_JAVA_ = 'java.exe'
else:
  _DEFAULT_JAVA_ = 'java'

# Browsers to choose from (for browser flag).
# use --browser $BROWSER_NAME to run
# tests for that browser
_SELENIUM_BROWSERS = [
    '*firefox',
    '*iexplore',
    '*googlechrome']

######################################################################
# Comandline Parsing
######################################################################

parser = optparse.OptionParser()
parser.add_option(
    '--url',
    default='index.html',
    metavar='FILE',
    help='write report to FILE')


parser.add_option(
    '--log_level',
    type='int',
    default=20,
    help='controls tracing verbosity')

parser.add_option(
    '--java',
    default=_DEFAULT_JAVA_,
    help='the name/path of the java loader')

parser.add_option(
    '--selenium_jar',
    default=_DEFAULT_SELENIUM_JAR,
    help='the name/path of the selenium rc server jar')

parser.add_option(
    '--port_fileserver',
    type='int',
    default=0,
    help='specifies the port number to be used by file server.'
    'A value zero means pick an arbitrary port.')

# TODO(robertm): There is bug filed to resolve the port issue
# http://jira.openqa.org/browse/SRC-436
parser.add_option(
    '--port_rcserver',
    type='int',
    default='4444',
    help='specifies the port number to be used by Selenium RC.\n'
    'A value zero means pick an arbitrary port'
    '- THIS FEATURE IS CURRENTLY BROKEN.')

parser.add_option(
    '--timeout',
    type='int',
    default=20000,
    help='timeout in msec')

parser.add_option(
    '--shutdown_delay',
    type='int',
    default=0,
    help='time to wait before tear down')

parser.add_option(
    '--browser',
    metavar='BROWSER',
    default='*firefox',
    help='\n'.join(['browsers to test. Options:'] +
                   _SELENIUM_BROWSERS))

parser.add_option(
    '--file',
    default=[],
    action='append',
    help='')

######################################################################
# logging
######################################################################

LOGGER_FORMAT = ('%(levelname)s:'
                 '%(module)s:'
                 '%(lineno)d:'
                 '%(threadName)s '
                 '%(message)s')

logging.basicConfig(level=logging.DEBUG,
                    format=LOGGER_FORMAT)

######################################################################
# http server for html and nacl modules
######################################################################
SPECIAL_PAGE_FOR_SELF_TEST = """\
<!DOCTYPE html>
<html>
DUMMY PAGE
<script type="text/javascript">
//<![CDATA[

function NaclLib() {
  this.count = 0;
 };


 NaclLib.prototype.getStatus = function() {
   this.count += 1;
   if (this.count < 5) return "WAIT";
   return "SUCCESS";
 };


 NaclLib.prototype.getMessage = function() {
   return "GAME OVER";
 };

var nacllib = new NaclLib();

//]]>
</script>
</html>
"""

def GuessMimeType(filename):
  if filename.endswith('.html'):
    return 'text/html'
  elif filename.endswith('.nexe'):
    return 'application/x-object'
    #return 'application/x-octet-stream'
  else:
    return 'text/plain'

class MyRequestHandler(SimpleHTTPServer.SimpleHTTPRequestHandler):
  """Hook to handle HTTP server requests.

  Functions as a handler for logging and other utility functions.
  """

  def log_message(self, format, *args):
    """Logging hook for HTTP server."""
    logging.info(format % args)

  def SendData(self, data, type='text/html', code=200):
    try:
      self.send_response(code)
      self.send_header('Content-Type', type)
      self.send_header('Content-Length', len(data))
      self.end_headers()
      self.wfile.write(data)
    except Exception, err:
      logging.error(str(err))
      self.send_error(404, str(err))
    return

  def SendFile(self, filename):
    type = GuessMimeType(filename)
    logging.info('sending %s (%s)', filename, type)
    try:
      data = open(filename).read()
      self.SendData(data, type=type)
    except Exception, err:
      logging.error(str(err))
      self.SendData(str(err), code=404)
    return

  def SendToc(self, files):
    logging.info("sending toc")
    try:
      s = ['<html><table border=1>']
      for f in files:
        s.append('<tr><td>%s<td>%s' % (f, os.path.basename(f)))
      s.append('</table></html>')
      self.SendData("".join(s))
    except Exception, err:
      logging.error(str(err))
      self.SendData(str(err), code=404)
    return

  def do_GET(self):
    files = self.server._files
    filename = os.path.basename(self.path)
    logging.info("request %s -> %s", self.path, filename)

    # built-in page
    if filename == 'self.test.html':
      self.SendData(SPECIAL_PAGE_FOR_SELF_TEST)
      return

    # check against set of available
    for f in files:
      candidate = os.path.basename(f)
      if candidate == filename:
        self.SendFile(f)
        return

    # default is the file listing
    self.SendToc(files)
    return

class LocalFileHTTPServer(threading.Thread):
  """Minimal HTTP server that serves local files.

  Members:
    _http_alive: event to signal that http server is up and running
    http_port: the TCP port the server is using
  """

  def __init__(self, files, port):
    """Initializes the server.

    Initializes the HTTP server to serve static files from the
    local client3d directory

    Args:
      local_root: all files below this path are served.
    """
    logging.info('#' * 60)
    logging.info('# creating file/page http server')
    logging.info('#' * 60)

    self._keep_going = True
    threading.Thread.__init__(self, name='fileserver')
    for f in files:
      logging.info('serving file %s', f)

    try:
      self._httpd = SocketServer.TCPServer(('', port), MyRequestHandler)
      # hack to communicate the fileset to MyRequestHandler
      self._httpd._files = files

    except Exception, err:
      logging.error('Could not create http server: %s', str(err))
      logging.error('on unix try to figure out who is blocking the port with')
      logging.error('lsof -i :%d', port)
      logging.error('Also look for zombies using:')
      logging.error('ps -ef | grep selenium')
      sys.exit(-1)

    # if port == 0, we need to figure out the actual port
    self._port = self._httpd.server_address[1]
    logging.info('created server on port %d', self._port)
    logging.info('try it at %s', self.BaseUrl())

  def StopServer(self):
    self._keep_going = False
    logging.info("shutting down file server")
    self.join()

  def BaseUrl(self):
    # NOTE(robertm): the commented out version might be necessary for chrome
    #return 'http://%s:%s' % (socket.gethostname(), self._port)
    return 'http://%s:%s' % ('localhost', self._port)

  def run(self):
    """Runs the HTTP server."""
    logging.info('starting http server thread')
    while self._keep_going:
      # poll every second to avoid having a blocking request
      r, dummy_w, dummy_e = select.select([self._httpd], [], [], 1.0)
      if r:
        self._httpd.handle_request()
    logging.info('stopping http server thread')
    try:
      self._httpd.socket.shutdown(2)
      self._httpd.socket.close()
    except Exception, err:
      logging.info('error on http server shutdown %s', err)

######################################################################
#  browser remote control server
######################################################################

class SeleniumRemoteControl(threading.Thread):
  """A thread that launches the Selenium Remote Control server.

  The Remote Control server allows us to launch a browser and remotely
  control it from a script.

  Members:
    _selenium_alive: event to signal that selenium server is up and running
    selenium_port: the TCP port the server is using
    _process: the subprocess.Popen instance for the server
  """

  def __init__(self, java, selenium_jar, port):
    """Initializes the SeleniumRemoteControl class."""
    logging.info('#' * 60)
    logging.info('# creating selenium remote control server')
    logging.info('#' * 60)

    self._alive = threading.Event()
    self._keep_going = True
    if port == 0:
      logging.fatal("selenium is broken and does not support this yet")
      sys.exit(-1)
    threading.Thread.__init__(self, name='sel_rc')

    self._process = subprocess.Popen(
        [java,
         '-jar', selenium_jar,
         '-multiWindow',
         '-port', str(port)],
        stdout=subprocess.PIPE, stderr=subprocess.STDOUT)

    self._port = self._GetPortNumber(self._process)
    if self._port is None:
        logging.error("cannot launch selenium rc server")
        sys.exit(-1)

    logging.info('found active selenium server on port %d', self._port)
    logging.info('try it at %s', self.BaseUrl())

  def StopServer(self):
    logging.info("shutting down selenium rc server")
    self._keep_going = False
    # NOTE: this only supported for python 2.6  and above
    try:
      self._process.terminate()
    except Exception, err:
      logging.error('problem terminating selenium server: %s', str(err))
      logging.info('This is normal for python version < 2.6')

    # NOTE: here is what we do in desperation for older python versions
    try:
      import signal
      os.kill(self._process.pid, signal.SIGTERM)
    except Exception, err:
      logging.error('problem killing selenium server: %s', str(err))

    try:
      self._process.wait()
    except Exception, err:
      logging.error('problem shutting down selenium server: %s', str(err))


    self.join()

  def BaseUrl(self):
    # NOTE(robertm): the commented out version might be necessary for chrome
    #return 'http://%s:%s' % (socket.gethostname(), self._port)
    return 'http://%s:%s' % ('localhost', self._port)

  def _GetPortNumber(self, process):
    # Go through the log messages emitted by the server
    # we expect the ready message to be within the first 10 lines
    for _ in range(1, 10):
        msg = process.stdout.readline().strip()
        if not msg: continue

        logging.debug('sel_serv:' + msg)

        # This status message indicates that the server has done
        # a bind successfully.
        # a message looks something like:
        # 15:00:43.821 INFO - Started SocketListener on 0.0.0.0:40292
        if msg.find('INFO - Started SocketListener') == -1:
          continue

        colon = msg.rfind(":")
        assert colon > 0
        return int(msg[colon + 1:])
    return None

  def run(self):
    """echo selenium server messages in this thread."""
    while self._process.poll() is None and self._keep_going:
      msg = self._process.stdout.readline().strip()
      if msg:
        logging.debug('sel_serv:' + msg)
    logging.info("seleniun thread terminating")

######################################################################
# poor man's atexit to be called from signal handler
######################################################################

GlobalCleanupList = []

def Cleanup():
  global GlobalCleanupList
  logging.info('#' * 60)
  logging.info('Cleanup')
  logging.info('#' * 60)

  GlobalCleanupList.reverse()
  for s in GlobalCleanupList:
    try:
      s()
    except Exception, err:
      logging.info(str(err))
  return

######################################################################
# test
######################################################################

def RunTest(session, url, max_wait):
  logging.info('#' * 60)
  logging.info('Testing %s', url)
  logging.info('#' * 60)
  start_time = time.time()

  session.open(url)
  try:
    # NOTE: these three commands represent our entire interaction with the page
    session.wait_for_condition("window.nacllib.getStatus() != 'WAIT'", max_wait)
    status = session.get_eval("window.nacllib.getStatus()");
    message = session.get_eval("window.nacllib.getMessage()");
  except Exception, err:
    status = "FAILURE"
    message = str(err)

  duration = time.time() - start_time
  logging.info("STATUS: %s", status)
  logging.info("MESSAGE: %s", message)

  if status != "SUCCESS":
    status = "FAILURE"

  # NOTE: this string is being parsed via pulse.xml
  logging.info("RESULT:%s  STATUS:%s  TIME:%f  DETAILS:%s",
               url, status, duration, message)

  if status == "SUCCESS":
    logging.info("************* SUCCESS *********")
    return 0
  else:
    logging.warning("************* FAILURE *********")
    return 1

######################################################################
# main
######################################################################

def main(options):
  logging.info('browser list is %s:', options.browser)

  # install a cleanup handler if possible
  try:
    import signal
    signal.signal(signal.SIGTERM, Cleanup)
  except Exception, err:
    logging.warning("could not install signal handler %s",
                    str(err))

  # Start up a static file server, to serve the test pages.
  # Serve from the nacl staging directory - that's where all the html
  # and nexe files can be found.
  http_server = LocalFileHTTPServer(options.file,
                                    options.port_fileserver)
  http_server.start()
  GlobalCleanupList.append(http_server.StopServer)
  # Start up the Selenium Remote Control Server
  selenium_server = SeleniumRemoteControl(options.java,
                                          options.selenium_jar,
                                          options.port_rcserver)
  selenium_server.start()
  GlobalCleanupList.append(selenium_server.StopServer)

  logging.info('#' * 60)
  logging.info('creating session for browser %s', options.browser)
  logging.info('#' * 60)
  session = selenium.selenium('localhost',
                              selenium_server._port,
                              options.browser,
                              http_server.BaseUrl())
  logging.info('starting session')
  session.start()
  GlobalCleanupList.append(session.stop)
  result = RunTest(session,
                   session.browserURL + "/"  + options.url,
                   options.timeout)

  if options.shutdown_delay > 0:
    logging.info('shutdown delay active')
    time.sleep(options.shutdown_delay)

  Cleanup()
  return result


if __name__ == '__main__':
  options, args = parser.parse_args()
  assert not args
  logging.getLogger().setLevel(int(options.log_level))
  sys.exit(main(options))
