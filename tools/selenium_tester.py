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
import signal
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

sys.path.append(
    os.path.join(_SELENIUM_PATH, 'selenium-python-client-driver-rev8252'))

import selenium

# NOTE: the selenium-server-standalone.jar reades /dev/random on
#       Linux which can cause problems with some vmware base setups
if 0:
  _DEFAULT_SELENIUM_JAR = os.path.join(_SELENIUM_PATH,
                                       'selenium-server-2.0a1',
                                       'selenium-server-standalone.jar'
                                       )
else:
  # TODO: phase out old server
  if sys.platform == 'linux2':
    # Selenium 1.0.3 does not work reliably on the Buildbots on Linux:
    # it fails to print "Started SocketListener on 0.0.0.0:4444".
    # TODO(mseaborn): Clean up this sorry mess.
    _DEFAULT_SELENIUM_JAR = os.path.join(
        _SELENIUM_PATH, 'selenium-server-1.0.1', 'selenium-server.jar')
  else:
    # Selenium 1.0.1 does not work with Firefox on Mac OS X 10.6.
    # See http://code.google.com/p/nativeclient/issues/detail?id=1150
    _DEFAULT_SELENIUM_JAR = os.path.join(
        _SELENIUM_PATH, 'selenium-server-1.0.3', 'selenium-server.jar')


def RunningOnWindows():
  return platform.system() == 'Windows'


def GetJavaExe():
  if RunningOnWindows():
    return 'java.exe'
  else:
    return 'java'

# Browsers to choose from (for browser flag).
# use --browser $BROWSER_NAME to run
# tests for that browser
_SELENIUM_BROWSERS = [
    '*firefox [path]',
    '*iexplore [path]',
    '*googlechrome [path]']

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
    default=10,
    help='controls tracing verbosity')

parser.add_option(
    '--java',
    default=GetJavaExe(),
    help='the name/path of the java loader')

parser.add_option(
    '--selenium_jar',
    default=_DEFAULT_SELENIUM_JAR,
    help='the name/path of the selenium rc server jar')

parser.add_option(
    '--port_fileserver',
    type='int',
    default=0,
    help='specifies the port number to be used by file server.\n'
    'A value zero means pick an arbitrary port on non-windows systems.')

# TODO(robertm): There is bug filed to resolve the port issue
# http://jira.openqa.org/browse/SRC-436
parser.add_option(
    '--port_rcserver',
    type='int',
    default='4444',
    help='specifies the port number to be used by Selenium RC.\n'
    'A value zero means pick an arbitrary port on non-windows systems.\n'
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
    '--start_selenium_server',
    type='int',
    default=1,
    help='automatically start selenium server')

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
    logging.debug(format % args)

  def SendData(self, data, type='text/html', code=200, send_body=1):
    try:
      self.send_response(code)
      self.send_header('Content-Type', type)
      self.send_header('Content-Length', len(data))
      self.end_headers()
      if send_body:
        self.wfile.write(data)
    except Exception, err:
      logging.error(str(err))
      self.send_error(404, str(err))
    return

  def SendFile(self, filename, send_body):
    type = GuessMimeType(filename)
    logging.info('sending %s (%s)', filename, type)
    try:
      data = open(filename).read()
      self.SendData(data, type=type, send_body=send_body)
    except Exception, err:
      logging.error(str(err))
      self.SendData(str(err), code=404)
    return

  def SendToc(self, files, send_body):
    logging.info("sending toc")
    try:
      s = ['<html><table border=1>']
      for f in files:
        s.append('<tr><td>%s<td>%s' % (f, os.path.basename(f)))
      s.append('</table></html>')
      self.SendData("".join(s), send_body=send_body)
    except Exception, err:
      logging.error(str(err))
      self.SendData(str(err), code=404)
    return


  def HandleGetOrHeadRequest(self, filename, send_body):
    # built-in page
    if filename == 'self.test.html':
      self.SendData(SPECIAL_PAGE_FOR_SELF_TEST, send_body=send_body)
      return

    # check against set of available
    for f in self.server._files:
      candidate = os.path.basename(f)
      if candidate == filename:
        self.SendFile(f, send_body)
        return

    # handle this special so we can add something nice eventually
    if filename.endswith('favicon.ico'):
      self.SendData('no favicon yet', code=404)
      return

    # send simple file listing
    if filename in ['', '/']:
      self.SendToc(self.server._files, send_body)
      return

    self.SendData('no such file', code=404)

  def do_HEAD(self):
    filename = os.path.basename(self.path)
    logging.info("HEAD request %s -> %s", self.path, filename)
    self.HandleGetOrHeadRequest(filename, 0)
    return

  def do_GET(self):
    filename = os.path.basename(self.path)
    logging.info("GET request %s -> %s", self.path, filename)
    self.HandleGetOrHeadRequest(filename, 1)
    return

class LocalFileHTTPServer(threading.Thread):
  """Minimal HTTP server that serves local files.

  Members:
    _port: the TCP port the server is using
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

    if port == 0 and RunningOnWindows():
      logging.fatal("zero ports do not work on windows")
      sys.exit(-1)

    self._keep_going = True
    threading.Thread.__init__(self, name='fileserver')
    for f in files:
      logging.info('serving file %s', f)
      if not os.access(f, os.R_OK):
        logging.fatal('cannot access file %s', f)
        sys.exit(-1)

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
def KillProcess(process, msg):
  try:
    process.terminate()
  except AttributeError, err:
    # NOTE: terminate only supported for python 2.6  and above
    pass
  except Exception, err:
    logging.error('problem terminating %s: %s', msg, str(err))


  # NOTE: here is what we do in desperation for older python versions
  try:
    import signal
    os.kill(process.pid, signal.SIGTERM)
  except Exception, err:
    logging.error('problem killing %s: %s', msg, str(err))



class SeleniumRemoteControl(threading.Thread):
  """A thread that launches the Selenium Remote Control server.

  The Remote Control server allows us to launch a browser and remotely
  control it from a script.

  Members:
    _port: the TCP port the server is using
    _process: the subprocess.Popen instance for the server
  """

  def __init__(self, java, selenium_jar, port):
    """Initializes the SeleniumRemoteControl class."""
    logging.info('#' * 60)
    logging.info('# creating selenium remote control server')
    logging.info('#' * 60)

    self._keep_going = True
    self._port = None

    if port == 0:
      logging.fatal("zero port does not work because of a selenium bug.")
      sys.exit(-1)
    if port == 0 and RunningOnWindows():
      logging.fatal("zero ports do not work on windows")
      sys.exit(-1)
    threading.Thread.__init__(self, name='sel_rc')

    cmd =  [java,
            # NOTE: keep java from using the *blocking* /dev/random device
            '-Djava.security.egd=file:/dev/urandom',
            '-jar', selenium_jar,
            '-multiWindow',
            '-port', str(port)]
    logging.info("launching rc server %s", repr(cmd))
    self._process = subprocess.Popen(cmd,
                                     stdout=subprocess.PIPE,
                                     stderr=subprocess.STDOUT)

  def StopServer(self):
    logging.info("shutting down selenium rc server")
    self._keep_going = False

    KillProcess(self._process, "selenium server")

    try:
      self._process.wait()
    except Exception, err:
      logging.error('problem shutting down selenium server: %s', str(err))
    self.join()

  def BaseUrl(self):
    # NOTE(robertm): the commented out version might be necessary for chrome
    #return 'http://%s:%s' % (socket.gethostname(), self._port)
    return 'http://%s:%s' % ('localhost', self._port)

  def GetPort(self):
    MAX_SECS = 40
    for i in range(MAX_SECS):
      if self._port == 0:
        logging.error("Failed to receive port number of Selenium server")
        KillProcess(self._process, "selenium server")
        sys.exit(-1)

      if self._port:
        logging.info("Received port number of Selenium server "
                     "within %d seconds", i)
        return self._port
      time.sleep(1)
    logging.error("Failed to receive port number of Selenium server "
                  "within %d seconds", MAX_SECS)
    KillProcess(self._process, "selenium server")
    sys.exit(-1)

  def run(self):
    """echo selenium server messages in this thread.
    Also, scan for a special regex to determine the port the server
    is listening on.
    """
    line_count = 0
    while self._process.poll() is None and self._keep_going:
      msg = self._process.stdout.readline().strip()
      if not msg:
        continue
      line_count += 1
      logging.debug('[%03d] sel_serv:' + msg, line_count)
      if self._port != None:
        continue
      if line_count > 20:
        # giving up
        self._port = 0
        continue
      # This status message indicates that the server has done
      # a bind successfully.
      # a message looks something like:
      # 15:00:43.821 INFO - Started SocketListener on 0.0.0.0:40292
      if msg.find('INFO - Started SocketListener') == -1:
          continue
      colon = msg.rfind(":")
      assert colon > 0
      self._port = int(msg[colon + 1:])

    logging.info("selenium thread terminating")

######################################################################
# poor man's atexit to be called from signal handler
######################################################################

GlobalCleanupList = []

def CleanupAlarmHandler(signum, frame):
  """Raises a RuntimeError if the alarm fires. Selenium sometimes hangs while
  trying to kill Firefox with a "Killing Firefox..." message as the last
  thing logged. Eventually the entire test gets killed with a failure status
  which closes the tree. Since we're just trying to shutdown Selenium at this
  point, it seems silly for errors to be considered so serious.
  """
  logging.error('Cleanup() timed out.')
  raise RuntimeError('Cleanup() timed out.')

def Cleanup():
  global GlobalCleanupList
  logging.info('#' * 60)
  logging.info('Cleanup')
  logging.info('#' * 60)

  old_handler = signal.signal(signal.SIGALRM, CleanupAlarmHandler)
  signal.alarm(30)

  GlobalCleanupList.reverse()
  for s in GlobalCleanupList:
    logging.info('Cleanup: %s' % str(s))
    try:
      s()
    except Exception, err:
      logging.info(str(err))

  signal.signal(signal.SIGALRM, old_handler)
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
  logging.debug('env is:')
  for t in os.environ.items():
    logging.debug('%s=%s' % t)
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
  if options.start_selenium_server:
    selenium_server = SeleniumRemoteControl(options.java,
                                            options.selenium_jar,
                                            options.port_rcserver)
    selenium_server.start()
    GlobalCleanupList.append(selenium_server.StopServer)
    selenium_server_port = selenium_server.GetPort()
    logging.info('found active selenium server on port %d',
                 selenium_server_port)
    logging.info('try it at %s', selenium_server.BaseUrl())
  else:
    selenium_server_port = options.port_rcserver

  logging.info('#' * 60)
  logging.info('creating session for browser %s', options.browser)
  logging.info('#' * 60)
  session = selenium.selenium('localhost',
                              selenium_server_port,
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
