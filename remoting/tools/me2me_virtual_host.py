#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Virtual Me2Me implementation.  This script runs and manages the processes
# required for a Virtual Me2Me desktop, which are: X server, X desktop
# session, and Host process.
# This script is intended to run continuously as a background daemon
# process, running under an ordinary (non-root) user account.

import atexit
import base64
import errno
import getpass
import hashlib
import hmac
import json
import logging
import optparse
import os
import random
import signal
import socket
import subprocess
import sys
import tempfile
import time
import urllib2
import uuid

# Local modules
sys.path.insert(0, "/usr/share/chrome-remote-desktop")
import gaia_auth
import keygen

# By default this script will try to determine the most appropriate X session
# command for the system.  To use a specific session instead, set this variable
# to the executable filename, or a list containing the executable and any
# arguments, for example:
# XSESSION_COMMAND = "/usr/bin/gnome-session-fallback"
# XSESSION_COMMAND = ["/usr/bin/gnome-session", "--session=ubuntu-2d"]
XSESSION_COMMAND = None

REMOTING_COMMAND = "remoting_me2me_host"

REMOTING_LOG_FILE = "REMOTING_ME2ME_LOG_FILE"

# Needs to be an absolute path, since the current working directory is changed
# when this process self-daemonizes.
SCRIPT_PATH = os.path.dirname(sys.argv[0])
if SCRIPT_PATH:
  SCRIPT_PATH = os.path.abspath(SCRIPT_PATH)
else:
  SCRIPT_PATH = os.getcwd()

# These are relative to SCRIPT_PATH.
EXE_PATHS_TO_TRY = [
    ".",
    "../../out/Debug",
    "../../out/Release",
    "/usr/lib/chrome-remote-desktop",
]

CONFIG_DIR = os.path.expanduser("~/.config/chrome-remote-desktop")
HOME_DIR = os.environ["HOME"]

X_LOCK_FILE_TEMPLATE = "/tmp/.X%d-lock"
FIRST_X_DISPLAY_NUMBER = 20

X_AUTH_FILE = os.path.expanduser("~/.Xauthority")
os.environ["XAUTHORITY"] = X_AUTH_FILE


# Globals needed by the atexit cleanup() handler.
g_desktops = []
g_pidfile = None

class Config:
  def __init__(self, path):
    self.path = path
    self.data = {}
    self.changed = False

  def load(self):
    try:
      settings_file = open(self.path, 'r')
      self.data = json.load(settings_file)
      self.changed = False
      settings_file.close()
    except:
      return False
    return True

  def save(self):
    if not self.changed:
      return True
    try:
      old_umask = os.umask(0066)
      settings_file = open(self.path, 'w')
      settings_file.write(json.dumps(self.data, indent=2))
      settings_file.close()
      os.umask(old_umask)
    except:
      return False
    self.changed = False
    return True

  def get(self, key):
    return self.data.get(key)

  def __getitem__(self, key):
    return self.data[key]

  def __setitem__(self, key, value):
    self.data[key] = value
    self.changed = True

  def clear_auth(self):
    del self.data["xmpp_login"]
    del self.data["chromoting_auth_token"]
    del self.data["xmpp_auth_token"]
    del self.data["oauth_refresh_token"]

  def clear_host_info(self):
    del self.data["host_id"]
    del self.data["host_name"]
    del self.data["host_secret_hash"]
    del self.data["private_key"]

class Authentication:
  """Manage authentication tokens for Chromoting/xmpp"""

  def __init__(self):
    self.login = None
    self.chromoting_auth_token = None
    self.xmpp_auth_token = None
    self.oauth_refresh_token = None

  def generate_tokens(self):
    """Prompt for username/password and use them to generate new authentication
    tokens.

    Raises:
      Exception: Failed to get new authentication tokens.
    """
    print "Email:",
    self.login = raw_input()
    password = getpass.getpass("App-specific password: ")

    chromoting_auth = gaia_auth.GaiaAuthenticator('chromoting')
    self.chromoting_auth_token = chromoting_auth.authenticate(self.login,
                                                              password)

    xmpp_authenticator = gaia_auth.GaiaAuthenticator('chromiumsync')
    self.xmpp_auth_token = xmpp_authenticator.authenticate(self.login,
                                                           password)

  def has_chromoting_credentials(self):
    """Returns True if we have credentials for the directory"""
    return self.chromoting_auth_token != None

  def has_xmpp_credentials(self):
    """Returns True if we have credentials to authenticate XMPP connection"""
    # XMPP connection can be authenticated using either OAuth or XMPP token.
    return self.oauth_refresh_token != None or self.xmpp_auth_token != None

  def load_config(self, config):
    """Loads the config and returns false if the config is invalid. After
    config is loaded caller must use has_xmpp_credentials() and
    has_chromoting_credentials() to check that the credentials it needs are
    present in the config."""

    # Host can use different types of auth tokens depending on how the config
    # was generated. E.g. if the config was created by the webapp it will have
    # only oauth token. We still consider config to be valid in this case.
    self.chromoting_auth_token = config.get("chromoting_auth_token")
    self.oauth_refresh_token = config.get("oauth_refresh_token")
    self.xmpp_auth_token = config.get("xmpp_auth_token")

    try:
      self.login = config["xmpp_login"]
    except KeyError:
      return False
    return True

  def save_config(self, config):
    config["xmpp_login"] = self.login
    if self.chromoting_auth_token:
      config["chromoting_auth_token"] = self.chromoting_auth_token
    if self.xmpp_auth_token:
      config["xmpp_auth_token"] = self.xmpp_auth_token


class Host:
  """This manages the configuration for a host.

  Callers should instantiate a Host object (passing in a filename where the
  config will be kept), then should call either of the methods:

  * register(auth): Create a new Host configuration and register it
  with the Directory Service (the "auth" parameter is used to
  authenticate with the Service).
  * load_config(): Load a config from disk, with details of an existing Host
  registration.

  After calling register() (or making any config changes) the method
  save_config() should be called to save the details to disk.
  """

  server = 'www.googleapis.com'
  url = 'https://' + server + '/chromoting/v1/@me/hosts'

  def __init__(self, auth):
    """
    Args:
      config: Host configuration object
      auth: Authentication object with credentials for authenticating with the
        Directory service.
    """
    self.auth = auth
    self.host_id = str(uuid.uuid1())
    self.host_name = socket.gethostname()
    self.host_secret_hash = None
    self.private_key = None

  def register(self):
    """Generates a private key for the stored |host_id|, and registers it with
    the Directory service.

    Raises:
      urllib2.HTTPError: An error occurred talking to the Directory server
        (for example, if the |auth| credentials were rejected).
    """

    logging.info("HostId: " + self.host_id)
    logging.info("HostName: " + self.host_name)

    logging.info("Generating RSA key pair...")
    (self.private_key, public_key) = keygen.generateRSAKeyPair()
    logging.info("Done")

    json_data = {
        "data": {
            "hostId": self.host_id,
            "hostName": self.host_name,
            "publicKey": public_key,
        }
    }
    params = json.dumps(json_data)
    headers = {
        "Authorization": "GoogleLogin auth=" + self.auth.chromoting_auth_token,
        "Content-Type": "application/json",
    }

    request = urllib2.Request(self.url, params, headers)
    opener = urllib2.OpenerDirector()
    opener.add_handler(urllib2.HTTPDefaultErrorHandler())

    logging.info("Registering host with directory service...")

    res = urllib2.urlopen(request)
    data = res.read()

    logging.info("Done")

  def ask_pin(self):
    while 1:
      pin = getpass.getpass("Host PIN: ")
      if len(pin) < 6:
        print "PIN must be at least 6 characters long."
        continue
      pin2 = getpass.getpass("Confirm host PIN: ")
      if pin2 != pin:
        print "PINs didn't match. Please try again."
        continue
      break
    self.set_pin(pin)

  def set_pin(self, pin):
    if pin == "":
      self.host_secret_hash = "plain:"
    else:
      self.host_secret_hash = "hmac:" + base64.b64encode(
          hmac.new(str(self.host_id), pin, hashlib.sha256).digest())

  def is_pin_set(self):
    return self.host_secret_hash

  def load_config(self, config):
    try:
      self.host_id = config["host_id"]
      self.host_name = config["host_name"]
      self.host_secret_hash = config.get("host_secret_hash")
      self.private_key = config["private_key"]
    except KeyError:
      return False
    return True

  def save_config(self, config):
    config["host_id"] = self.host_id
    config["host_name"] = self.host_name
    config["host_secret_hash"] = self.host_secret_hash
    config["private_key"] = self.private_key

class Desktop:
  """Manage a single virtual desktop"""

  def __init__(self, sizes):
    self.x_proc = None
    self.session_proc = None
    self.host_proc = None
    self.sizes = sizes
    g_desktops.append(self)

  @staticmethod
  def get_unused_display_number():
    """Return a candidate display number for which there is currently no
    X Server lock file"""
    display = FIRST_X_DISPLAY_NUMBER
    while os.path.exists(X_LOCK_FILE_TEMPLATE % display):
      display += 1
    return display

  def launch_x_server(self, extra_x_args):
    devnull = open(os.devnull, "rw")
    display = self.get_unused_display_number()
    ret_code = subprocess.call("xauth add :%d . `mcookie`" % display,
                               shell=True)
    if ret_code != 0:
      raise Exception("xauth failed with code %d" % ret_code)

    max_width = max([width for width, height in self.sizes])
    max_height = max([height for width, height in self.sizes])

    try:
      xvfb = locate_executable("Xvfb-randr")
    except Exception:
      xvfb = "Xvfb"

    logging.info("Starting %s on display :%d" % (xvfb, display));
    screen_option = "%dx%dx24" % (max_width, max_height)
    self.x_proc = subprocess.Popen([xvfb, ":%d" % display,
                                    "-noreset",
                                    "-auth", X_AUTH_FILE,
                                    "-nolisten", "tcp",
                                    "-screen", "0", screen_option
                                    ] + extra_x_args)
    if not self.x_proc.pid:
      raise Exception("Could not start Xvfb.")

    # Create clean environment for new session, so it is cleanly separated from
    # the user's console X session.
    self.child_env = {
        "DISPLAY": ":%d" % display,
        "REMOTING_ME2ME_SESSION": "1" }
    for key in [
        "HOME",
        "LANG",
        "LOGNAME",
        "PATH",
        "SHELL",
        "USER",
        "USERNAME",
        REMOTING_LOG_FILE]:
      if os.environ.has_key(key):
        self.child_env[key] = os.environ[key]

    # Wait for X to be active.
    for test in range(5):
      proc = subprocess.Popen("xdpyinfo", env=self.child_env, stdout=devnull)
      pid, retcode = os.waitpid(proc.pid, 0)
      if retcode == 0:
        break
      time.sleep(0.5)
    if retcode != 0:
      raise Exception("Could not connect to Xvfb.")
    else:
      logging.info("Xvfb is active.")

    # The remoting host expects the server to use "evdev" keycodes, but Xvfb
    # starts configured to use the "base" ruleset, resulting in XKB configuring
    # for "xfree86" keycodes, and screwing up some keys. See crbug.com/119013.
    # Reconfigure the X server to use "evdev" keymap rules.  The X server must
    # be started with -noreset otherwise it'll reset as soon as the command
    # completes, since there are no other X clients running yet.
    proc = subprocess.Popen("setxkbmap -rules evdev", env=self.child_env,
                            shell=True)
    pid, retcode = os.waitpid(proc.pid, 0)
    if retcode != 0:
      logging.error("Failed to set XKB to 'evdev'")

    # Register the screen sizes if the X server's RANDR extension supports it.
    # Errors here are non-fatal; the X server will continue to run with the
    # dimensions from the "-screen" option.
    for width, height in self.sizes:
      label = "%dx%d" % (width, height)
      args = ["xrandr", "--newmode", label, "0", str(width), "0", "0", "0",
              str(height), "0", "0", "0"]
      proc = subprocess.Popen(args, env=self.child_env, stdout=devnull,
                              stderr=devnull)
      proc.wait()
      args = ["xrandr", "--addmode", "screen", label]
      proc = subprocess.Popen(args, env=self.child_env, stdout=devnull,
                              stderr=devnull)
      proc.wait()

    # Set the initial mode to the first size specified, otherwise the X server
    # would default to (max_width, max_height), which might not even be in the
    # list.
    label = "%dx%d" % self.sizes[0]
    args = ["xrandr", "-s", label]
    proc = subprocess.Popen(args, env=self.child_env, stdout=devnull,
                            stderr=devnull)
    proc.wait()

    devnull.close()

  def launch_x_session(self):
    # Start desktop session
    # The /dev/null input redirection is necessary to prevent the X session
    # reading from stdin.  If this code runs as a shell background job in a
    # terminal, any reading from stdin causes the job to be suspended.
    # Daemonization would solve this problem by separating the process from the
    # controlling terminal.
    logging.info("Launching X session: %s" % XSESSION_COMMAND)
    self.session_proc = subprocess.Popen(XSESSION_COMMAND,
                                         stdin=open(os.devnull, "r"),
                                         cwd=HOME_DIR,
                                         env=self.child_env)
    if not self.session_proc.pid:
      raise Exception("Could not start X session")

  def launch_host(self, host_config):
    # Start remoting host
    args = [locate_executable(REMOTING_COMMAND),
            "--host-config=%s" % (host_config.path)]
    self.host_proc = subprocess.Popen(args, env=self.child_env)
    logging.info(args)
    if not self.host_proc.pid:
      raise Exception("Could not start remoting host")


class PidFile:
  """Class to allow creating and deleting a file which holds the PID of the
  running process.  This is used to detect if a process is already running, and
  inform the user of the PID.  On process termination, the PID file is
  deleted.

  Note that PID files are not truly atomic or reliable, see
  http://mywiki.wooledge.org/ProcessManagement for more discussion on this.

  So this class is just to prevent the user from accidentally running two
  instances of this script, and to report which PID may be the other running
  instance.
  """

  def __init__(self, filename):
    """Create an object to manage a PID file.  This does not create the PID
    file itself."""
    self.filename = filename
    self.created = False

  def check(self):
    """Checks current status of the process.

    Returns:
      Tuple (running, pid):
      |running| is True if the daemon is running.
      |pid| holds the process ID of the running instance if |running| is True.
      If the PID file exists but the PID couldn't be read from the file
      (perhaps if the data hasn't been written yet), 0 is returned.

    Raises:
      IOError: Filesystem error occurred.
    """
    if os.path.exists(self.filename):
      pid_file = open(self.filename, 'r')
      file_contents = pid_file.read()
      pid_file.close()

      try:
        pid = int(file_contents)
      except ValueError:
        return True, 0

      # Test to see if there's a process currently running with that PID.
      # If there is no process running, the existing PID file is definitely
      # stale and it is safe to overwrite it.  Otherwise, report the PID as
      # possibly a running instance of this script.
      if os.path.exists("/proc/%d" % pid):
        return True, pid

    return False, 0

  def create(self):
    """Creates an empty PID file."""
    pid_file = open(self.filename, 'w')
    pid_file.close()
    self.created = True

  def write_pid(self):
    """Write the current process's PID to the PID file.

    This is done separately from create() as this needs to be called
    after any daemonization, when the correct PID becomes known.  But
    check() and create() has to happen before daemonization, so that
    if another instance is already running, this fact can be reported
    to the user's terminal session.  This also avoids corrupting the
    log file of the other process, since daemonize() would create a
    new log file.
    """
    pid_file = open(self.filename, 'w')
    pid_file.write('%d\n' % os.getpid())
    pid_file.close()
    self.created = True

  def delete_file(self):
    """Delete the PID file if it was created by this instance.

    This is called on process termination.
    """
    if self.created:
      os.remove(self.filename)


def choose_x_session():
  """Chooses the most appropriate X session command for this system.

  If XSESSION_COMMAND is already set, its value is returned directly.
  Otherwise, a session is chosen for this system.

  Returns:
    A string containing the command to run, or a list of strings containing
    the executable program and its arguments, which is suitable for passing as
    the first parameter of subprocess.Popen().  If a suitable session cannot
    be found, returns None.
  """
  if XSESSION_COMMAND is not None:
    return XSESSION_COMMAND

  # Use a custom startup file if present
  startup_file = os.path.expanduser("~/.chrome-remote-desktop-session")
  if os.path.exists(startup_file):
    # Use the same logic that a Debian system typically uses with ~/.xsession
    # (see /etc/X11/Xsession.d/50x11-common_determine-startup), to determine
    # exactly how to run this file.
    if os.access(startup_file, os.X_OK):
      return startup_file
    else:
      shell = os.environ.get("SHELL", "sh")
      return [shell, startup_file]

  # Unity-2d would normally be the preferred choice on Ubuntu 12.04.  At the
  # time of writing, this session does not work properly (missing launcher and
  # panel), so gnome-session-fallback is used in preference.
  # "unity-2d-panel" was chosen here simply because it appears in the TryExec
  # line of the session's .desktop file; other choices might be just as good.
  for test_file, command in [
    ("/usr/bin/gnome-session-fallback",
      ["/etc/X11/Xsession", "gnome-session-fallback"]),
    ("/etc/gdm/Xsession", "/etc/gdm/Xsession"),
    ("/usr/bin/unity-2d-panel",
      ["/etc/X11/Xsession", "/usr/bin/gnome-session --session=ubuntu-2d"]),
  ]:
    if os.path.exists(test_file):
      return command

  return None


def locate_executable(exe_name):
  for path in EXE_PATHS_TO_TRY:
    exe_path = os.path.join(SCRIPT_PATH, path, exe_name)
    if os.path.exists(exe_path):
      return exe_path

  raise Exception("Could not locate executable '%s'" % exe_name)


def daemonize(log_filename):
  """Background this process and detach from controlling terminal, redirecting
  stdout/stderr to |log_filename|."""

  # TODO(lambroslambrou): Having stdout/stderr redirected to a log file is not
  # ideal - it could create a filesystem DoS if the daemon or a child process
  # were to write excessive amounts to stdout/stderr.  Ideally, stdout/stderr
  # should be redirected to a pipe or socket, and a process at the other end
  # should consume the data and write it to a logging facility which can do
  # data-capping or log-rotation. The 'logger' command-line utility could be
  # used for this, but it might cause too much syslog spam.

  # Create new (temporary) file-descriptors before forking, so any errors get
  # reported to the main process and set the correct exit-code.
  # The mode is provided, since Python otherwise sets a default mode of 0777,
  # which would result in the new file having permissions of 0777 & ~umask,
  # possibly leaving the executable bits set.
  devnull_fd = os.open(os.devnull, os.O_RDONLY)
  log_fd = os.open(log_filename, os.O_WRONLY | os.O_CREAT | os.O_APPEND, 0600)

  pid = os.fork()

  if pid == 0:
    # Child process
    os.setsid()

    # The second fork ensures that the daemon isn't a session leader, so that
    # it doesn't acquire a controlling terminal.
    pid = os.fork()

    if pid == 0:
      # Grandchild process
      pass
    else:
      # Child process
      os._exit(0)
  else:
    # Parent process
    os._exit(0)

  logging.info("Daemon process running, logging to '%s'" % log_filename)

  os.chdir(HOME_DIR)

  # Copy the file-descriptors to create new stdin, stdout and stderr.  Note
  # that dup2(oldfd, newfd) closes newfd first, so this will close the current
  # stdin, stdout and stderr, detaching from the terminal.
  os.dup2(devnull_fd, sys.stdin.fileno())
  os.dup2(log_fd, sys.stdout.fileno())
  os.dup2(log_fd, sys.stderr.fileno())

  # Close the temporary file-descriptors.
  os.close(devnull_fd)
  os.close(log_fd)


def cleanup():
  logging.info("Cleanup.")

  global g_pidfile
  if g_pidfile:
    try:
      g_pidfile.delete_file()
      g_pidfile = None
    except Exception, e:
      logging.error("Unexpected error deleting PID file: " + str(e))

  global g_desktops
  for desktop in g_desktops:
    if desktop.x_proc:
      logging.info("Terminating Xvfb")
      desktop.x_proc.terminate()
  g_desktops = []


def reload_config():
  for desktop in g_desktops:
    if desktop.host_proc:
      desktop.host_proc.send_signal(signal.SIGHUP)


def signal_handler(signum, stackframe):
  if signum == signal.SIGHUP:
    logging.info("SIGHUP caught, reloading configuration.")
    reload_config()
  else:
    # Exit cleanly so the atexit handler, cleanup(), gets called.
    raise SystemExit


def relaunch_self():
  cleanup()
  os.execvp(sys.argv[0], sys.argv)


def main():
  DEFAULT_SIZE = "1280x800"
  parser = optparse.OptionParser(
      "Usage: %prog [options] [ -- [ X server options ] ]")
  parser.add_option("-s", "--size", dest="size", action="append",
                    help="dimensions of virtual desktop (default: %s). "
                    "This can be specified multiple times to make multiple "
                    "screen resolutions available (if the Xvfb server "
                    "supports this)" % DEFAULT_SIZE)
  parser.add_option("-f", "--foreground", dest="foreground", default=False,
                    action="store_true",
                    help="don't run as a background daemon")
  parser.add_option("-k", "--stop", dest="stop", default=False,
                    action="store_true",
                    help="stop the daemon currently running")
  parser.add_option("-p", "--new-pin", dest="new_pin", default=False,
                    action="store_true",
                    help="set new PIN before starting the host")
  parser.add_option("", "--check-running", dest="check_running", default=False,
                    action="store_true",
                    help="return 0 if the daemon is running, or 1 otherwise")
  parser.add_option("", "--silent", dest="silent", default=False,
                    action="store_true",
                    help="Start the host without trying to configure it.")
  parser.add_option("", "--reload", dest="reload", default=False,
                    action="store_true",
                    help="Signal currently running host to reload the config.")
  (options, args) = parser.parse_args()

  host_hash = hashlib.md5(socket.gethostname()).hexdigest()
  pid_filename = os.path.join(CONFIG_DIR, "host#%s.pid" % host_hash)

  if options.check_running:
    running, pid = PidFile(pid_filename).check()
    return 0 if (running and pid != 0) else 1

  if options.stop:
    running, pid = PidFile(pid_filename).check()
    if not running:
      print "The daemon currently is not running"
    else:
      print "Killing process %s" % pid
      os.kill(pid, signal.SIGTERM)
    return 0

  if options.reload:
    running, pid = PidFile(pid_filename).check()
    if not running:
      return 1
    os.kill(pid, signal.SIGHUP)
    return 0

  if not options.size:
    options.size = [DEFAULT_SIZE]

  sizes = []
  for size in options.size:
    size_components = size.split("x")
    if len(size_components) != 2:
      parser.error("Incorrect size format '%s', should be WIDTHxHEIGHT" % size)

    try:
      width = int(size_components[0])
      height = int(size_components[1])

      # Enforce minimum desktop size, as a sanity-check.  The limit of 100 will
      # detect typos of 2 instead of 3 digits.
      if width < 100 or height < 100:
        raise ValueError
    except ValueError:
      parser.error("Width and height should be 100 pixels or greater")

    sizes.append((width, height))

  global XSESSION_COMMAND
  XSESSION_COMMAND = choose_x_session()
  if XSESSION_COMMAND is None:
    print >> sys.stderr, "Unable to choose suitable X session command."
    return 1

  if "--session=ubuntu-2d" in XSESSION_COMMAND:
    print >> sys.stderr, (
      "The Unity 2D desktop session will be used.\n"
      "If you encounter problems with this choice of desktop, please install\n"
      "the gnome-session-fallback package, and restart this script.\n")

  atexit.register(cleanup)

  for s in [signal.SIGHUP, signal.SIGINT, signal.SIGTERM, signal.SIGUSR1]:
    signal.signal(s, signal_handler)

  # Ensure full path to config directory exists.
  if not os.path.exists(CONFIG_DIR):
    os.makedirs(CONFIG_DIR, mode=0700)

  host_config = Config(os.path.join(CONFIG_DIR, "host#%s.json" % host_hash))
  host_config.load()

  auth = Authentication()
  auth_config_loaded = auth.load_config(host_config)
  if not auth_config_loaded and not options.silent:
    # If we failed to load authentication parameters from the host config
    # then try loading them from the legacy auth.json file.
    auth_config = Config(os.path.join(CONFIG_DIR, "auth.json"))
    if auth_config.load():
      auth_config_loaded = auth.load_config(auth_config)
      # If we were able to read auth.json then copy its content to the host
      # config.
      if auth_config_loaded:
        auth.save_config(host_config)
        host_config.save()

  host = Host(auth)
  host_config_loaded = host.load_config(host_config)

  if options.silent:
    # Just validate the config when run with --silent.
    if not host_config_loaded or not auth_config_loaded:
      logging.error("Failed to load host configuration.")
      return 1
    if not auth.has_xmpp_credentials():
      logging.error("Auth tokens are not configured.")
      return 1
  else:
    need_auth_tokens = (
        not auth_config_loaded or not auth.has_chromoting_credentials())
    need_register_host = not host_config_loaded
    # Outside the loop so user doesn't get asked twice.
    if need_register_host:
      host.ask_pin()
    elif options.new_pin or not host.is_pin_set():
      host.ask_pin()
      host.save_config(host_config)
      running, pid = PidFile(pid_filename).check()
      if running and pid != 0:
        os.kill(pid, signal.SIGHUP)
        print "The running instance has been updated with the new PIN."
        return 0

    # The loop is to deal with the case of registering a new Host with
    # previously-saved auth tokens (from a previous run of this script), which
    # may require re-prompting for username & password.
    while True:
      if need_auth_tokens:
        try:
          auth.generate_tokens()
          need_auth_tokens = False
        except Exception:
          logging.error("Authentication failed")
          return 1
        # Save the new auth tokens.
        auth.save_config(host_config)
        if not host_config.save():
          logging.error("Failed to save host configuration.")
          return 1

      if need_register_host:
        try:
          host.register()
          host.save_config(host_config)
        except urllib2.HTTPError, err:
          if err.getcode() == 401:
            # Authentication failed - re-prompt for username & password.
            need_auth_tokens = True
            continue
          else:
            # Not an authentication error.
            logging.error("Directory returned error: " + str(err))
            logging.error(err.read())
            return 1

      # |auth| and |host| are both set up, so break out of the loop.
      break

    if not host_config.save():
      logging.error("Failed to save host configuration.")
      return 1

  global g_pidfile
  g_pidfile = PidFile(pid_filename)
  running, pid = g_pidfile.check()

  if running:
    print "An instance of this script is already running."
    print "Use the -k flag to terminate the running instance."
    print "If this isn't the case, delete '%s' and try again." % pid_filename
    return 1

  g_pidfile.create()

  # daemonize() must only be called after prompting for user/password, as the
  # process will become detached from the controlling terminal.

  if not options.foreground:
    if not os.environ.has_key(REMOTING_LOG_FILE):
      log_file = tempfile.NamedTemporaryFile(prefix="me2me_host_", delete=False)
      os.environ[REMOTING_LOG_FILE] = log_file.name
    daemonize(os.environ[REMOTING_LOG_FILE])

  g_pidfile.write_pid()

  logging.info("Using host_id: " + host.host_id)

  desktop = Desktop(sizes)

  # Remember the time when the last session was launched, in order to enforce
  # a minimum time between launches.  This avoids spinning in case of a
  # misconfigured system, or other error that prevents a session from starting
  # properly.
  last_launch_time = 0

  while True:
    # If the session process or X server stops running (e.g. because the user
    # logged out), kill the other. This will trigger the next conditional block
    # as soon as the os.wait() call (below) returns.
    if desktop.session_proc is None and desktop.x_proc is not None:
      logging.info("Terminating X server")
      desktop.x_proc.terminate()
    elif desktop.x_proc is None and desktop.session_proc is not None:
      logging.info("Terminating X session")
      desktop.session_proc.terminate()
    elif desktop.x_proc is None and desktop.session_proc is None:
      # Neither X server nor X session are running.
      elapsed = time.time() - last_launch_time
      if elapsed < 60:
        logging.error("The session lasted less than 1 minute.  Waiting " +
                      "before starting new session.")
        time.sleep(60 - elapsed)

      if last_launch_time == 0:
        # Neither process has been started yet. Do so now.
        logging.info("Launching X server and X session.")
        last_launch_time = time.time()
        desktop.launch_x_server(args)
        desktop.launch_x_session()
      else:
        # Both processes have terminated. Since the user's desktop is already
        # gone at this point, there's no state to lose and now is a good time
        # to pick up any updates to this script that might have been installed.
        logging.info("Relaunching self")
        relaunch_self()

    if desktop.host_proc is None:
      logging.info("Launching host process")
      desktop.launch_host(host_config)

    try:
      pid, status = os.wait()
    except OSError, e:
      if e.errno == errno.EINTR:
        # Retry on EINTR, which can happen if a signal such as SIGHUP is
        # received.
        continue
      else:
        # Anything else is an unexpected error.
        raise

    logging.info("wait() returned (%s,%s)" % (pid, status))

    # When os.wait() notifies that a process has terminated, any Popen instance
    # for that process is no longer valid.  Reset any affected instance to
    # None.
    if desktop.x_proc is not None and pid == desktop.x_proc.pid:
      logging.info("X server process terminated")
      desktop.x_proc = None

    if desktop.session_proc is not None and pid == desktop.session_proc.pid:
      logging.info("Session process terminated")
      desktop.session_proc = None

    if desktop.host_proc is not None and pid == desktop.host_proc.pid:
      logging.info("Host process terminated")
      desktop.host_proc = None

      # These exit-codes must match the ones used by the host.
      # See remoting/host/constants.h.
      # Delete the host or auth configuration depending on the returned error
      # code, so the next time this script is run, a new configuration
      # will be created and registered.
      if os.WEXITSTATUS(status) == 2:
        logging.info("Host configuration is invalid - exiting.")
        host_config.clear_auth()
        host_config.clear_host_info()
        host_config.save()
        return 0
      elif os.WEXITSTATUS(status) == 3:
        logging.info("Host ID has been deleted - exiting.")
        host_config.clear_host_info()
        host_config.save()
        return 0
      elif os.WEXITSTATUS(status) == 4:
        logging.info("OAuth credentials are invalid - exiting.")
        host_config.clear_auth()
        host_config.save()
        return 0
      elif os.WEXITSTATUS(status) == 5:
        logging.info("Host domain is blocked by policy - exiting.")
        os.remove(host.config_file)
        return 0
      # Nothing to do for Mac-only status 6 (login screen unsupported)

if __name__ == "__main__":
  logging.basicConfig(level=logging.DEBUG)
  sys.exit(main())
