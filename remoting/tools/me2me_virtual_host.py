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
import getpass
import hashlib
import json
import logging
import optparse
import os
import random
import signal
import socket
import subprocess
import sys
import time
import urllib2
import uuid

# Local modules
import gaia_auth
import keygen

REMOTING_COMMAND = "remoting_me2me_host"

# Command-line switch for passing the config path to remoting_me2me_host.
HOST_CONFIG_SWITCH_NAME = "host-config"

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
    "../../out/Release"
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


class Authentication:
  """Manage authentication tokens for Chromoting/xmpp"""

  def __init__(self, config_file):
    self.config_file = config_file

  def refresh_tokens(self):
    print "Email:",
    self.login = raw_input()
    password = getpass.getpass("Password: ")

    chromoting_auth = gaia_auth.GaiaAuthenticator('chromoting')
    self.chromoting_auth_token = chromoting_auth.authenticate(self.login,
                                                              password)

    xmpp_authenticator = gaia_auth.GaiaAuthenticator('chromiumsync')
    self.xmpp_auth_token = xmpp_authenticator.authenticate(self.login,
                                                           password)

  def load_config(self):
    try:
      settings_file = open(self.config_file, 'r')
      data = json.load(settings_file)
      settings_file.close()
      self.login = data["xmpp_login"]
      self.chromoting_auth_token = data["chromoting_auth_token"]
      self.xmpp_auth_token = data["xmpp_auth_token"]
    except:
      return False
    return True

  def save_config(self):
    data = {
        "xmpp_login": self.login,
        "chromoting_auth_token": self.chromoting_auth_token,
        "xmpp_auth_token": self.xmpp_auth_token,
    }
    # File will contain private keys, so deny read/write access to others.
    old_umask = os.umask(0066)
    settings_file = open(self.config_file, 'w')
    settings_file.write(json.dumps(data, indent=2))
    settings_file.close()
    os.umask(old_umask)


class Host:
  """This manages the configuration for a host.

  Callers should instantiate a Host object (passing in a filename where the
  config will be kept), then should call either of the methods:

  * create_config(auth): Create a new Host configuration and register it with
  the Directory Service (the "auth" parameter is used to authenticate with the
  Service).
  * load_config(): Load a config from disk, with details of an existing Host
  registration.

  After calling create_config() (or making any config changes) the method
  save_config() should be called to save the details to disk.
  """

  server = 'www.googleapis.com'
  url = 'https://' + server + '/chromoting/v1/@me/hosts'

  def __init__(self, config_file):
    self.config_file = config_file

  def create_config(self, auth):
    self.host_id = str(uuid.uuid1())
    logging.info("HostId: " + self.host_id)
    self.host_name = socket.gethostname()
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
        "Authorization": "GoogleLogin auth=" + auth.chromoting_auth_token,
        "Content-Type": "application/json",
    }

    request = urllib2.Request(self.url, params, headers)
    opener = urllib2.OpenerDirector()
    opener.add_handler(urllib2.HTTPDefaultErrorHandler())

    logging.info("Registering host with directory service...")
    try:
      res = urllib2.urlopen(request)
      data = res.read()
    except urllib2.HTTPError, err:
      logging.error("Directory returned error: " + str(err))
      logging.error(err.read())
      sys.exit(1)
    logging.info("Done")

  def load_config(self):
    try:
      settings_file = open(self.config_file, 'r')
      data = json.load(settings_file)
      settings_file.close()
      self.host_id = data["host_id"]
      self.host_name = data["host_name"]
      self.private_key = data["private_key"]
    except:
      return False
    return True

  def save_config(self):
    data = {
        "host_id": self.host_id,
        "host_name": self.host_name,
        "private_key": self.private_key,
    }
    os.umask(0066) # Set permission mask for created file.
    settings_file = open(self.config_file, 'w')
    settings_file.write(json.dumps(data, indent=2))
    settings_file.close()


class Desktop:
  """Manage a single virtual desktop"""

  def __init__(self, width, height):
    self.x_proc = None
    self.session_proc = None
    self.host_proc = None
    self.width = width
    self.height = height
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
    display = self.get_unused_display_number()
    ret_code = subprocess.call("xauth add :%d . `mcookie`" % display,
                               shell=True)
    if ret_code != 0:
      raise Exception("xauth failed with code %d" % ret_code)

    logging.info("Starting Xvfb on display :%d" % display);
    screen_option = "%dx%dx24" % (self.width, self.height)
    self.x_proc = subprocess.Popen(["Xvfb", ":%d" % display,
                                    "-auth", X_AUTH_FILE,
                                    "-nolisten", "tcp",
                                    "-screen", "0", screen_option
                                    ] + extra_x_args)
    if not self.x_proc.pid:
      raise Exception("Could not start Xvfb.")

    # Create clean environment for new session, so it is cleanly separated from
    # the user's console X session.
    self.child_env = {"DISPLAY": ":%d" % display}
    for key in [
        "HOME",
        "LOGNAME",
        "PATH",
        "SHELL",
        "USER",
        "USERNAME"]:
      if os.environ.has_key(key):
        self.child_env[key] = os.environ[key]

    # Wait for X to be active.
    for test in range(5):
      proc = subprocess.Popen("xdpyinfo > /dev/null", env=self.child_env,
                              shell=True)
      pid, retcode = os.waitpid(proc.pid, 0)
      if retcode == 0:
        break
      time.sleep(0.5)
    if retcode != 0:
      raise Exception("Could not connect to Xvfb.")
    else:
      logging.info("Xvfb is active.")

  def launch_x_session(self):
    # Start desktop session
    # The /dev/null input redirection is necessary to prevent Xsession from
    # reading from stdin.  If this code runs as a shell background job in a
    # terminal, any reading from stdin causes the job to be suspended.
    # Daemonization would solve this problem by separating the process from the
    # controlling terminal.
    self.session_proc = subprocess.Popen("/etc/X11/Xsession",
                                         stdin=open(os.devnull, "r"),
                                         cwd=HOME_DIR,
                                         env=self.child_env)
    if not self.session_proc.pid:
      raise Exception("Could not start X session")

  def launch_host(self, host):
    # Start remoting host
    args = [locate_executable(REMOTING_COMMAND),
            "--%s=%s" % (HOST_CONFIG_SWITCH_NAME, host.config_file)]
    self.host_proc = subprocess.Popen(args, env=self.child_env)
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

  def check_and_create_file(self):
    """Attempt to create the PID file, checking first for any currently-running
    process.

    Returns:
      Tuple (created, pid):
      |created| is True if the new file was created, False if there was an
      existing process running.
      |pid| holds the process ID of the running instance if |created| is False.
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
        return False, 0

      # Test to see if there's a process currently running with that PID.
      # If there is no process running, the existing PID file is definitely
      # stale and it is safe to overwrite it.  Otherwise, report the PID as
      # possibly a running instance of this script.
      if os.path.exists("/proc/%d" % pid):
        return False, pid

    # Create new (or overwrite existing) PID file.
    pid_file = open(self.filename, 'w')
    pid_file.close()
    self.created = True
    return True, 0

  def write_pid(self):
    """Write the current process's PID to the PID file.

    This is done separately from check_and_create_file() as this needs to be
    called after any daemonization, when the correct PID becomes known.  But
    check_and_create_file() has to happen before daemonization, so that if
    another instance is already running, this fact can be reported to the
    user's terminal session.  This also avoids corrupting the log file of the
    other process, since daemonize() would create a new log file.
    """
    pid_file = open(self.filename, 'w')
    pid_file.write('%d\n' % os.getpid())
    pid_file.close()

  def delete_file(self):
    """Delete the PID file if it was created by this instance.

    This is called on process termination.
    """
    if self.created:
      os.remove(self.filename)


def locate_executable(exe_name):
  for path in EXE_PATHS_TO_TRY:
    exe_path = os.path.join(SCRIPT_PATH, path, exe_name)
    if os.path.exists(exe_path):
      return exe_path

  raise Exception("Could not locate executable '%s'" % exe_name)


def daemonize(log_filename):
  """Background this process and detach from controlling terminal, redirecting
  stdout/stderr to |log_filename|."""

  # Create new (temporary) file-descriptors before forking, so any errors get
  # reported to the main process and set the correct exit-code.
  devnull_fd = os.open(os.devnull, os.O_RDONLY)
  log_fd = os.open(log_filename, os.O_WRONLY | os.O_CREAT | os.O_TRUNC)

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

  logging.info("Daemon process running, PID = %d, logging to '%s'" %
               (os.getpid(), log_filename))

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

  if g_pidfile:
    try:
      g_pidfile.delete_file()
    except Exception, e:
      logging.error("Unexpected error deleting PID file: " + str(e))

  for desktop in g_desktops:
    if desktop.x_proc:
      logging.info("Terminating Xvfb")
      desktop.x_proc.terminate()


def signal_handler(signum, stackframe):
    # Exit cleanly so the atexit handler, cleanup(), gets called.
    raise SystemExit


def main():
  parser = optparse.OptionParser(
      "Usage: %prog [options] [ -- [ X server options ] ]")
  parser.add_option("-s", "--size", dest="size", default="1280x1024",
                    help="dimensions of virtual desktop (default: %default)")
  parser.add_option("-f", "--foreground", dest="foreground", default=False,
                    action="store_true",
                    help="don't run as a background daemon")
  (options, args) = parser.parse_args()

  size_components = options.size.split("x")
  if len(size_components) != 2:
    parser.error("Incorrect size format, should be WIDTHxHEIGHT");

  try:
    width = int(size_components[0])
    height = int(size_components[1])

    # Enforce minimum desktop size, as a sanity-check.  The limit of 100 will
    # detect typos of 2 instead of 3 digits.
    if width < 100 or height < 100:
      raise ValueError
  except ValueError:
    parser.error("Width and height should be 100 pixels or greater")

  atexit.register(cleanup)

  for s in [signal.SIGHUP, signal.SIGINT, signal.SIGTERM]:
    signal.signal(s, signal_handler)

  # Ensure full path to config directory exists.
  if not os.path.exists(CONFIG_DIR):
    os.makedirs(CONFIG_DIR, mode=0700)

  auth = Authentication(os.path.join(CONFIG_DIR, "auth.json"))
  if not auth.load_config():
    try:
      auth.refresh_tokens()
    except:
      logging.error("Authentication failed.")
      return 1
    auth.save_config()

  host_hash = hashlib.md5(socket.gethostname()).hexdigest()
  host = Host(os.path.join(CONFIG_DIR, "host#%s.json" % host_hash))

  if not host.load_config():
    host.create_config(auth)
    host.save_config()

  pid_filename = os.path.join(CONFIG_DIR, "host#%s.pid" % host_hash)
  global g_pidfile
  g_pidfile = PidFile(pid_filename)
  created, pid = g_pidfile.check_and_create_file()

  if not created:
    if pid == 0:
      pid = 'unknown'

    logging.error("An instance of this script is already running, PID is %s." %
                  pid)
    logging.error("If this isn't the case, delete '%s' and try again." %
                  pid_filename)
    return 1

  # daemonize() must only be called after prompting for user/password, as the
  # process will become detached from the controlling terminal.
  log_filename = os.path.join(CONFIG_DIR, "host#%s.log" % host_hash)

  if not options.foreground:
    daemonize(log_filename)

  g_pidfile.write_pid()

  logging.info("Using host_id: " + host.host_id)

  desktop = Desktop(width, height)

  while True:
    # If the session process stops running (e.g. because the user logged out),
    # the X server should be reset and the session restarted, to provide a
    # completely clean new session.
    if desktop.session_proc is None and desktop.x_proc is not None:
      logging.info("Terminating X server")
      desktop.x_proc.terminate()

    if desktop.x_proc is None:
      if desktop.session_proc is not None:
        # The X session would probably die soon if the X server is not
        # running (because of the loss of the X connection).  Terminate it
        # anyway, to be sure.
        logging.info("Terminating X session")
        desktop.session_proc.terminate()
      else:
        # Neither X server nor X session are running, so launch them both.
        logging.info("Launching X server and X session")
        desktop.launch_x_server(args)
        desktop.launch_x_session()

    if desktop.host_proc is None:
      logging.info("Launching host process")
      desktop.launch_host(host)

    pid, status = os.wait()
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


if __name__ == "__main__":
  logging.basicConfig(level=logging.DEBUG)
  sys.exit(main())
