# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import base64
import hashlib
import heapq
import json
import logging
import os
import re
import select
import socket
import subprocess
import sys
import time
import urlparse

import BaseHTTPServer

_unittests_running = False

class ChromeNotFoundException(Exception):
  pass

class _PossibleDesktopBrowser(object):
  def __init__(self, browser_type, executable):
    self.browser_type = browser_type
    self.local_executable = executable

  def __repr__(self):
    return '_PossibleDesktopBrowser(browser_type=%s)' % self.browser_type

def _samefile(a, b):
  if sys.platform != 'win32':
    return os.path.samefile(a, b)
  return os.path.abspath(a) == os.path.abspath(b)

def _FindAllAvailableBrowsers():
  """Finds all the desktop browsers available on this machine."""
  browsers = []

  has_display = True
  if (sys.platform.startswith('linux') and
      os.getenv('DISPLAY') == None):
    has_display = False

  def AddIfFound(browser_type, browser_dir, app_name):
    app = os.path.join(browser_dir, app_name)
    if os.path.exists(app):
      browsers.append(_PossibleDesktopBrowser(browser_type,
                                              app))
      return True
    return False

  # Look for a browser in the standard chrome build locations.
  if sys.platform == 'darwin':
    chromium_app_name = 'Chromium.app/Contents/MacOS/Chromium'
  elif sys.platform.startswith('linux'):
    chromium_app_name = 'chrome'
  elif sys.platform.startswith('win'):
    chromium_app_name = 'chrome.exe'
  else:
    raise Exception('Platform not recognized')

  # Mac-specific options.
  if sys.platform == 'darwin':
    mac_canary = ('/Applications/Google Chrome Canary.app/'
                 'Contents/MacOS/Google Chrome Canary')
    mac_system = '/Applications/Google Chrome.app/Contents/MacOS/Google Chrome'
    if os.path.exists(mac_canary):
      browsers.append(_PossibleDesktopBrowser('canary',
                                             mac_canary))

    if os.path.exists(mac_system):
      browsers.append(_PossibleDesktopBrowser('system',
                                             mac_system))

  # Linux specific options.
  if sys.platform.startswith('linux'):
    # Look for a google-chrome instance.
    found = False
    try:
      with open(os.devnull, 'w') as devnull:
        found = subprocess.call(['google-chrome', '--version'],
                                stdout=devnull, stderr=devnull) == 0
    except OSError:
      pass
    if found:
      browsers.append(
          _PossibleDesktopBrowser('system',
                                 'google-chrome'))

  # Win32-specific options.
  if sys.platform.startswith('win'):
    system_path = os.path.join('Google', 'Chrome', 'Application')
    canary_path = os.path.join('Google', 'Chrome SxS', 'Application')

    win_search_paths = [os.getenv('PROGRAMFILES(X86)'),
                        os.getenv('PROGRAMFILES'),
                        os.getenv('LOCALAPPDATA')]

    for path in win_search_paths:
      if not path:
        continue
      if AddIfFound('canary', os.path.join(path, canary_path),
                    chromium_app_name):
        break

    for path in win_search_paths:
      if not path:
        continue
      if AddIfFound('system', os.path.join(path, system_path),
                    chromium_app_name):
        break

  if len(browsers) and not has_display:
    logging.warning(
      'Found (%s), but you do not have a DISPLAY environment set.' %
      ','.join([b.browser_type for b in browsers]))
    return []

  return browsers



_ExceptionNames = {
  404: 'Not found',
  500: 'Internal exception',
}

class _RequestException(Exception):
  def __init__(self, number):
    super(_RequestException, self).__init__('_RequestException')
    self.number = number

class _RequestHandler(BaseHTTPServer.BaseHTTPRequestHandler):
  def __init__(self, request, client_address, server):
    self._server = server
    BaseHTTPServer.BaseHTTPRequestHandler.__init__(self, request, client_address, server)

  def _SendJSON(self, obj, resp_code=200, resp_code_str='OK'):
    text = json.dumps(obj)
    try:
      self.send_response(resp_code, resp_code_str)
      self.send_header('Cache-Control', 'no-cache')
      self.send_header('Content-Type', 'application/json')
      self.send_header('Content-Length', len(text))
      self.end_headers()
      self.wfile.write(text)
    except IOError:
      return

  def log_message(self, format, *args):
    pass

  def do_POST(self):
    self._HandleRequest('POST')

  def do_GET(self):
    self._HandleRequest('GET')

  def _HandleRequest(self, method):
    if 'Content-Length' in self.headers:
      cl = int(self.headers['Content-Length'])
      text = self.rfile.read(cl).encode('utf8')
      try:
        if text != '':
          content = json.loads(text)
        else:
          content = ''
      except ValueError:
        raise Exception('Payload was unparseable: [%s]' % text)
    else:
      content = None

    if self.path == '/ping':
      self._SendJSON('pong')
      return

    try:
      res = self._server.HandleRequest(method, self.path, content)
      self._SendJSON(res)
    except _RequestException, ex:
      self.send_response(ex.number, _ExceptionNames[ex.number])
      self.send_header('Content-Length', 0)
      self.end_headers()
    except:
      import traceback
      traceback.print_exc()
      self.send_response(500, 'ServerError')
      self.send_header('Content-Length', 0)
      self.end_headers()

class _TimeoutTask(object):
  def __init__(self, cb, deadline, args):
    self.cb = cb
    self.deadline = deadline
    self.args = args

  def __cmp__(self, that):
    return cmp(self.deadline, that.deadline)

class _Daemon(BaseHTTPServer.HTTPServer):
  def __init__(self, server_address):
    BaseHTTPServer.HTTPServer.__init__(self, server_address, _RequestHandler)
    self._port = server_address[1]
    self._is_running = False
    self._pending_timeout_heap = []

  @property
  def port(self):
    return self._port

  @property
  def is_running(self):
    return self._is_running

  def AddDelayedTask(self, cb, delay, *args):
    deadline = time.time() + delay
    to = _TimeoutTask(cb, deadline, args)
    heapq.heappush(self._pending_timeout_heap, to)

  def serve_forever(self):
    self._is_running = True
    try:
      while self._is_running:
        now = time.time()
        while True:
          if len(self._pending_timeout_heap):
            deadline = self._pending_timeout_heap[0].deadline
            if now > deadline:
              item = heapq.heappop(self._pending_timeout_heap)
              item.cb(*item.args)
            else:
              next_deadline = deadline
              break
          else:
            next_deadline = now + 0.2
            break

        now = time.time()
        delay = max(0.0,next_deadline - now)
        delay = min(0.25,delay)
        r, w, e = select.select([self], [], [], delay)
        if r:
          self.handle_request()
    finally:
      self._is_running = False

  def HandleRequest(self, method, path, content):
    if self.handler:
      return self.handler(method, path, content)
    raise _RequestException('404')

  def Stop(self):
    assert self._is_running
    self._is_running = False

  def Run(self):
    logging.debug('Starting chromeapp._Daemon on port %d', self._port)
    self.serve_forever()
    logging.debug('Shut down chromeapp._Daemon on port %d', self._port)

class _TimeoutException(Exception):
  pass

def _WaitFor(condition,
             timeout, poll_interval=0.1,
             pass_time_left_to_func=False):
  assert isinstance(condition, type(lambda: None))  # is function
  start_time = time.time()
  while True:
    if pass_time_left_to_func:
      res = condition(max((start_time + timeout) - time.time(), 0.0))
    else:
      res = condition()
    if res:
      break
    if time.time() - start_time > timeout:
      if condition.__name__ == '<lambda>':
        try:
          condition_string = inspect.getsource(condition).strip()
        except IOError:
          condition_string = condition.__name__
      else:
        condition_string = condition.__name__
      raise _TimeoutException('Timed out while waiting %ds for %s.' %
                             (timeout, condition_string))
    time.sleep(poll_interval)

def IsChromeInstalled():
  """Returns whether chromeapp works on this system."""
  browsers = _FindAllAvailableBrowsers()
  return len(browsers) > 0

def _HexToMPDecimal(hex_chars):
  """ Convert bytes to an MPDecimal string. Example \x00 -> "aa"
      This gives us the AppID for a chrome extension.
  """
  result = ''
  base = ord('a')
  for i in xrange(len(hex_chars)):
    value = ord(hex_chars[i])
    dig1 = value / 16
    dig2 = value % 16
    result += chr(dig1 + base)
    result += chr(dig2 + base)
  return result

def _GetPublicKeyFromPath(filepath):
  # Normalize the path for windows to have capital drive letters.
  # We intentionally don't check if sys.platform == 'win32' and just
  # check if this looks like drive letter so that we can test this
  # even on posix systems.
  if (len(filepath) >= 2 and
      filepath[0].islower() and
      filepath[1] == ':'):
      return filepath[0].upper() + filepath[1:]
  return filepath

def _GetPublicKeyUnpacked(filepath):
  assert os.path.isdir(filepath)
  f = open(os.path.join(filepath, 'manifest.json'), 'rb')
  manifest = json.load(f)
  if 'key' not in manifest:
    # Use the path as the public key.
    # See Extension::GenerateIdForPath in extension.cc
    return _GetPublicKeyFromPath(os.path.abspath(filepath))
  else:
    return base64.standard_b64decode(manifest['key'])

def _GetCRXAppID(filepath):
  pub_key = _GetPublicKeyUnpacked(filepath)
  pub_key_hash = hashlib.sha256(pub_key).digest()
  # AppID is the MPDecimal of only the first 128 bits of the hash.
  return _HexToMPDecimal(pub_key_hash[:128/8])

class AppInstance(object):
  def __init__(self, app, args=None):
    self._app = app
    self._proc = None
    self._devnull = None
    self._cur_chromeapp_js = None
    self._event_listeners = {}
    if args:
      self._args = args
    else:
      self._args = []

    self._exit_code = None
    self._exiting_run_loop = False

  def __enter__(self):
    if not self.is_started:
      self.Start()
    return self

  def __exit__(self, *args):
    if self.is_started:
      self._CloseBrowserProcess()

  @property
  def is_started(self):
    return self._proc != None

  def Start(self):
    tmp = socket.socket()
    tmp.bind(('', 0))
    port = tmp.getsockname()[1]
    tmp.close()

    self._daemon = _Daemon(('localhost', port))
    self._daemon.handler = self._HandleRequest

    browsers = _FindAllAvailableBrowsers()
    if len(browsers) == 0:
      raise ChromeNotFoundException('Could not find Chrome. Cannot start app.')
    browser = browsers[0]


    if self._GetAppID() == None:
      if not _unittests_running:
        sys.stderr.write("""
Installing Chrome App for %s.

You will see chrome appear as this happens.

***DO NOT CLOSE IT***
""" % self._app.stable_app_name)
        sys.stderr.flush()
      self._Install(browser)
      if not _unittests_running:
        sys.stderr.write("\nApp installed. Thanks for waiting.\n")

    app_id = self._GetAppID()

    # Temporary staging: we now know how to compute crx ids by hand.
    # Verify that we are getting it right.
    raw_id = _GetCRXAppID(self._app.manifest_dirname)
    assert raw_id == app_id

    if self._app.debug_mode:
      print "chromeapp: app_id is %s" % app_id

    browser_args = [browser.local_executable]
    browser_args.extend(self._app._GetBrowserStartupArgs())
    browser_args.append('--app-id=%s' % app_id)
    logging.info('Launching %s as %s', self._app.stable_app_name, app_id)
    self._CreateLaunchJS()
    try:
      self._Launch(browser_args)
    except:
      if self.is_started:
        self._CloseBrowserProcess()

  def _Install(self, browser):
    logging.info('Installing %s for first time...', self._app.stable_app_name)
    browser_args = [browser.local_executable]
    browser_args.extend(self._app._GetBrowserStartupArgs())
    browser_args.append(
      '--load-extension=%s' % self._app.manifest_dirname
      )
    try:
      self._Launch(browser_args)
      def IsAppInstalled():
        return self._GetAppID() != None
      # We may have to a wait for a while, it seems like chrome takes a while
      # to flush its preferences.
      _WaitFor(IsAppInstalled, 60, poll_interval=0.5)
      logging.info('Installed %s', self._app.stable_app_name)
    finally:
      if self.is_started:
        self._CloseBrowserProcess()

  def _GetAppID(self):
    prefs = self._app._ReadPreferences()
    if 'extensions' not in prefs:
      return None
    if 'settings' not in prefs['extensions']:
      return None
    settings = prefs['extensions']['settings']
    for app_id, app_settings in settings.iteritems():
      if 'path' not in app_settings:
        continue
      if not os.path.exists(app_settings['path']):
        continue
      if not _samefile(app_settings['path'],
                          self._app.manifest_dirname):
        continue

      if 'events' not in app_settings:
        return None

      return app_id

    return None

  def _CreateLaunchJS(self):
    js_template = """
'use strict';

// DO NOT COMMIT! This template is automatically created and updated by
// py-chrome-ui just before launch, with the necessary
// parameters for communicating back to the hosting python code.
(function() {
  var BASE_URL = "__CHROMEAPP_REPLY_URL__"; // Note: chromeapp will set this up during launch.
  var DEBUG_MODE = __CHROMEAPP_DEBUG_MODE__;  // Note: chromeapp will set this up during launch.lj

  function reqAsync(method, path, data, opt_response_cb, opt_err_cb) {
    if (path[0] != '/')
      throw new Error('Must start with /');
    var req = new XMLHttpRequest();
    req.open(method, BASE_URL + path, true);
    req.addEventListener('load', function() {
      if (req.status == 200) {
        if (opt_response_cb)
          opt_response_cb(JSON.parse(req.responseText));
        return;
      }
      if (opt_err_cb)
        opt_err_cb();
      else
        console.log('reqAsync ' + path, req);
    });
    req.addEventListener('error', function() {
      if (opt_err_cb)
        opt_err_cb();
      else
        console.log('reqAsync ' + path, req);
    });
    if (data)
      req.send(JSON.stringify(data));
    else
      req.send(null);
  }

  function Event(type, opt_bubbles, opt_preventable) {
    var e = document.createEvent('Event');
    e.initEvent(type, !!opt_bubbles, !!opt_preventable);
    e.__proto__ = window.Event.prototype;
    return e;
  };

  function dispatchSimpleEvent(target, type, opt_bubbles, opt_cancelable) {
    var e = new Event(type, opt_bubbles, opt_cancelable);
    return target.dispatchEvent(e);
  }

  // chromeapp derives from a div in order to get basic dispatchEvent
  // capabilities. Lame but effective.
  var chromeapp = document.createElement('div');

  // Ask the server for startup args.
  // TODO(nduca): crbug.com/168085 causes breakpoints to be ineffective
  // during the early stages of page load. In debug mode, we delay the launch
  // event for a bit so that breakpoints work.
  if (!DEBUG_MODE) {
    reqAsync('GET', '/launch_args', null, gotLaunchArgs);
  } else {
    reqAsync('GET', '/launch_args', null, function(args) {
      setTimeout(function() {
         gotLaunchArgs(args);
      }, 1000);
    });
  }
  function gotLaunchArgs(args) {
    var e = new Event('launch', false, false);
    e.args = args;
    chromeapp.launch_args = args;
    chromeapp.dispatchEvent(e);
  }

  var oldOnError = window.onerror;
  function onUncaughtError(error, url, line_number) {
    reqAsync('POST', '/uncaught_error', {
      error: error,
      url: url,
      line_number: line_number});
    if (oldOnError) oldOnError(error, url, line_number);
  }
  window.onerror = onUncaughtError;

  function print() {
    var messages = [];
    for (var i = 0; i < arguments.length; i++) {
      try {
        // See if it even stringifies.
        JSON.stringify(arguments[i]);
        messages.push(arguments[i]);
      } catch(ex) {
        messages.push('Argument ' + i + ' not convertible to JSON');
      }
    }
    reqAsync('POST', '/print', messages);
  }

  function sendEvent(event_name, args, opt_callback, opt_err_callback) {
    if (args === undefined)
      throw new Error('args is required');
    reqAsync('POST', '/send_event', {
        event_name: event_name,
        args: args},
        opt_callback,
        opt_err_callback);
  }

  var exiting = false;
  function exit(opt_exitCode) {
    var exitCode = opt_exitCode;
    if (opt_exitCode === undefined)
      exitCode = 0;
    if (typeof(exitCode) != 'number')
      throw new Error('exit code must be a number or undefined');
    if (exiting)
       throw new Error('chromeapp.exit() was a already called');
    exiting = true;

    reqAsync('POST', '/exit', {exitCode: exitCode}, function() { });

    // Busy wait for a bit to try to give the xhr a chance to hit
    // python.
    var start = Date.now();
    while(Date.now() < start + 150);
  }

  window.chromeapp = chromeapp;
  window.chromeapp.launch_args = undefined;
  window.chromeapp.sendEvent = sendEvent;
  window.chromeapp.print = print;
  window.chromeapp.exit = exit;
})();
"""

    assert self._daemon
    self._cur_chromeapp_js = os.path.join(
      self._app.manifest_dirname,
      'chromeapp.js')
    js = js_template
    js = js.replace('__CHROMEAPP_REPLY_URL__',
                             'http://localhost:%i' % self._daemon.port)
    js = js.replace('__CHROMEAPP_DEBUG_MODE__',
                             '%s' % json.dumps(self._app.debug_mode))
    with open(self._cur_chromeapp_js, 'w') as f:
      f.write(js)

  def _CleanupLaunchJS(self):
    if self._cur_chromeapp_js == None:
      return
    assert os.path.exists(self._cur_chromeapp_js)
    os.unlink(self._cur_chromeapp_js)
    self._cur_chromeapp_js = None

  def _Launch(self, browser_args):
    if not self._app.debug_mode:
      self._devnull = open(os.devnull, 'w')
      self._proc = subprocess.Popen(
        browser_args, stdout=self._devnull, stderr=self._devnull)
    else:
      self._devnull = None
      self._proc = subprocess.Popen(browser_args)

  def _StartCheckingForBrowserAliveness(self):
    self._daemon.AddDelayedTask(self._CheckForBrowserAliveness, 0.25)

  def _CheckForBrowserAliveness(self):
    if not self._proc:
      return
    def IsStopped():
      return self._proc.poll() != None
    if IsStopped():
      if not self._exiting_run_loop:
        sys.stderr.write("Browser closed without notifying us. Exiting...\n")
        self.ExitRunLoop(1)
      return
    self._StartCheckingForBrowserAliveness()

  def Run(self):
    assert self._exit_code == None
    assert self.is_started
    self._StartCheckingForBrowserAliveness()
    try:
      self._daemon.Run()
    finally:
      self._exiting_run_loop = False
      exit_code = self._exit_code
      self._exit_code = None
    return exit_code

  def _OnUncaughtError(self, error):
    m = re.match('chrome-extension:\/\/(.+)\/(.*)', error['url'], re.DOTALL)
    assert m
    sys.stderr.write("Uncaught error: %s:%i: %s\n" % (
        m.group(2), error['line_number'],
        error['error']))

  def _OnPrint(self, content):
    print "%s" % ' '.join([str(x) for x in content])

  def AddListener(self, event_name, callback):
    if event_name in self._event_listeners:
      raise Exception('Event listener already registered')
    self._event_listeners[event_name] = callback

  def RemoveListener(self, event_name, callback):
    if event_name not in self._event_listeners:
      raise Exception("Not found")
    del self._event_listeners[event_name]

  def HasListener(self, event_name, callback):
    return event_name in self._event_listeners

  def _OnSendEvent(self, content):
    event_name = content["event_name"]
    args = content["args"]
    listener = self._event_listeners.get(event_name, None)
    if not listener:
      sys.stderr.write('No listener for %s\n' % event_name)
      raise _RequestException(500)

    try:
      return listener(args)
    except:
      import traceback
      traceback.print_exc()
      raise _RequestException(500)

  def _HandleRequest(self, method, path, content):
    parsed_result = urlparse.urlparse(path)
    if path == '/launch_args':
      return self._args

    if path == '/uncaught_error':
      self._OnUncaughtError(content)
      return

    if path == '/print':
      self._OnPrint(content)
      return

    if path == '/send_event':
      return self._OnSendEvent(content)

    if path == '/exit':
      self.ExitRunLoop(content['exitCode'])
      return True

    raise _RequestException(404)

  def ExitRunLoop(self, exit_code):
    """Forces the app out of its run loop."""
    assert self._daemon.is_running
    if self._exiting_run_loop:
      logging.warning("Multiple calls to exit. First return value will be chosen.")
      return
    self._exiting_run_loop = True
    self._exit_code = exit_code
    self._daemon.Stop()

  def _CloseBrowserProcess(self):
    assert self.is_started
    assert not self._daemon.is_running

    if self._proc:

      def IsStopped():
        if not self._proc:
          return True
        return self._proc.poll() != None

      # Try to politely shutdown, first.
      if not IsStopped():
        try:
          self._proc.terminate()
          try:
            _WaitFor(IsStopped, timeout=5)
            self._proc = None
          except _TimeoutException:
            pass
        except:
          pass

      # Kill it.
      if not IsStopped():
        self._proc.kill()
        try:
          _WaitFor(IsStopped, timeout=5)
          self._proc = None
        except _TimeoutException:
          self._proc = None
          raise Exception('Could not shutdown the browser.')

    if self._devnull:
      self._devnull.close()
      self._devnull = None

    self._CleanupLaunchJS()

class ManifestError(Exception):
  pass

class App(object):
  def __init__(self, stable_app_name, manifest_filename,
               debug_mode=False,
               chromeapp_profiles_dir=False):
    self._stable_app_name = stable_app_name
    self._profile_dir = None

    self._manifest_filename = manifest_filename
    self._debug_mode = debug_mode

    with open(self._manifest_filename, 'r') as f:
      manifest_text = f.read()
    self._manifest = json.loads(manifest_text)

    if 'permissions' not in self._manifest:
      raise ManifestError('You need to have permissions: "http://localhost:*/" in your manifest.')
    if 'http://localhost:*/' not in self._manifest['permissions']:
      raise ManifestError('You need to have permissions: "http://localhost:*/" in your manifest.')

    if not chromeapp_profiles_dir:
      chromeapp_profiles_dir = os.path.expanduser('~/.chromeapp')
    if not os.path.exists(chromeapp_profiles_dir):
      os.mkdir(chromeapp_profiles_dir)

    self._profile_dir = os.path.join(chromeapp_profiles_dir,
                                     stable_app_name)

  def Run(self, args=None):
    """Launches and runs instance of the application. Returns its exit code.

    This is shorthand for creating an AppInstance against this app and running it:
       with AppInstance(app, args) as instance:
          ret_val = instance.Run()
    """
    with AppInstance(self, args) as instance:
      return instance.Run()

  @property
  def stable_app_name(self):
    return self._stable_app_name

  @property
  def debug_mode(self):
    return self._debug_mode

  @property
  def manifest_filename(self):
    return self._manifest_filename

  @property
  def manifest_dirname(self):
    return os.path.abspath(os.path.dirname(self._manifest_filename))

  def _GetBrowserStartupArgs(self):
    args = []
    args.append('--user-data-dir=%s' % self._profile_dir)
    args.append('--no-first-run')
    args.append('--noerrdialogs')
    args.append('--enable-experimental-extension-apis')
    return args

  def _ReadPreferences(self):
    prefs_file = os.path.join(self._profile_dir,
                              'Default', 'Preferences')
    try:
      with open(prefs_file, 'r') as f:
        contents = f.read()
    except:
      contents = """{
   "extensions": {
      "settings": {
      }
   }
}"""
    return json.loads(contents)
