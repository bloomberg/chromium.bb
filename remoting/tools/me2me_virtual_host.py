#!/usr/bin/python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Virtual Me2Me implementation.  This script runs and manages the processes
# required for a Virtual Me2Me desktop, which are: X server, X desktop
# session, and Host process.
# This script is intended to run continuously as a background daemon
# process, running under an ordinary (non-root) user account.

import atexit
import errno
import getpass
import hashlib
import json
import logging
import optparse
import os
import signal
import socket
import subprocess
import sys
import tempfile
import time
import uuid

# By default this script will try to determine the most appropriate X session
# command for the system.  To use a specific session instead, set this variable
# to the executable filename, or a list containing the executable and any
# arguments, for example:
# XSESSION_COMMAND = "/usr/bin/gnome-session-fallback"
# XSESSION_COMMAND = ["/usr/bin/gnome-session", "--session=ubuntu-2d"]
XSESSION_COMMAND = None

LOG_FILE_ENV_VAR = "CHROME_REMOTE_DESKTOP_LOG_FILE"

# This script has a sensible default for the initial and maximum desktop size,
# which can be overridden either on the command-line, or via a comma-separated
# list of sizes in this environment variable.
DEFAULT_SIZES_ENV_VAR = "CHROME_REMOTE_DESKTOP_DEFAULT_DESKTOP_SIZES"

# By default, provide a relatively small size to handle the case where resize-
# to-client is disabled, and a much larger size to support clients with large
# or mulitple monitors. These defaults can be overridden in ~/.profile.
DEFAULT_SIZES = "1600x1200,3840x1600"

SCRIPT_PATH = sys.path[0]

DEFAULT_INSTALL_PATH = "/opt/google/chrome-remote-desktop"
if SCRIPT_PATH == DEFAULT_INSTALL_PATH:
  HOST_BINARY_NAME = "chrome-remote-desktop-host"
else:
  HOST_BINARY_NAME = "remoting_me2me_host"

CHROME_REMOTING_GROUP_NAME = "chrome-remote-desktop"

CONFIG_DIR = os.path.expanduser("~/.config/chrome-remote-desktop")
HOME_DIR = os.environ["HOME"]

X_LOCK_FILE_TEMPLATE = "/tmp/.X%d-lock"
FIRST_X_DISPLAY_NUMBER = 20

# Minimum amount of time to wait between relaunching processes.
BACKOFF_TIME = 60

# Maximum allowed consecutive times that a child process runs for less than
# BACKOFF_TIME. This script exits if this limit is exceeded.
MAX_LAUNCH_FAILURES = 10

# Globals needed by the atexit cleanup() handler.
g_desktops = []
g_pidfile = None
g_host_hash = hashlib.md5(socket.gethostname()).hexdigest()

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
    except Exception:
      return False
    return True

  def save(self):
    if not self.changed:
      return True
    old_umask = os.umask(0066)
    try:
      settings_file = open(self.path, 'w')
      settings_file.write(json.dumps(self.data, indent=2))
      settings_file.close()
    except Exception:
      return False
    finally:
      os.umask(old_umask)
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
    self.oauth_refresh_token = None

  def copy_from(self, config):
    """Loads the config and returns false if the config is invalid."""
    try:
      self.login = config["xmpp_login"]
      self.oauth_refresh_token = config["oauth_refresh_token"]
    except KeyError:
      return False
    return True

  def copy_to(self, config):
    config["xmpp_login"] = self.login
    config["oauth_refresh_token"] = self.oauth_refresh_token


class Host:
  """This manages the configuration for a host."""

  def __init__(self):
    self.host_id = str(uuid.uuid1())
    self.host_name = socket.gethostname()
    self.host_secret_hash = None
    self.private_key = None

  def copy_from(self, config):
    try:
      self.host_id = config["host_id"]
      self.host_name = config["host_name"]
      self.host_secret_hash = config.get("host_secret_hash")
      self.private_key = config["private_key"]
    except KeyError:
      return False
    return True

  def copy_to(self, config):
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
    self.child_env = None
    self.sizes = sizes
    self.pulseaudio_pipe = None
    self.server_supports_exact_resize = False
    g_desktops.append(self)

  @staticmethod
  def get_unused_display_number():
    """Return a candidate display number for which there is currently no
    X Server lock file"""
    display = FIRST_X_DISPLAY_NUMBER
    while os.path.exists(X_LOCK_FILE_TEMPLATE % display):
      display += 1
    return display

  def _init_child_env(self):
    # Create clean environment for new session, so it is cleanly separated from
    # the user's console X session.
    self.child_env = {}
    for key in [
        "HOME",
        "LANG",
        "LOGNAME",
        "PATH",
        "SHELL",
        "USER",
        "USERNAME",
        LOG_FILE_ENV_VAR]:
      if os.environ.has_key(key):
        self.child_env[key] = os.environ[key]

  def _setup_pulseaudio(self):
    self.pulseaudio_pipe = None

    # pulseaudio uses UNIX sockets for communication. Length of UNIX socket
    # name is limited to 108 characters, so audio will not work properly if
    # the path is too long. To workaround this problem we use only first 10
    # symbols of the host hash.
    pulse_path = os.path.join(CONFIG_DIR,
                              "pulseaudio#%s" % g_host_hash[0:10])
    if len(pulse_path) + len("/native") >= 108:
      logging.error("Audio will not be enabled because pulseaudio UNIX " +
                    "socket path is too long.")
      return False

    sink_name = "chrome_remote_desktop_session"
    pipe_name = os.path.join(pulse_path, "fifo_output")

    try:
      if not os.path.exists(pulse_path):
        os.mkdir(pulse_path)
      if not os.path.exists(pipe_name):
        os.mkfifo(pipe_name)
    except IOError, e:
      logging.error("Failed to create pulseaudio pipe: " + str(e))
      return False

    try:
      pulse_config = open(os.path.join(pulse_path, "default.pa"), "w")
      pulse_config.write("load-module module-native-protocol-unix\n")
      pulse_config.write(
          ("load-module module-pipe-sink sink_name=%s file=\"%s\" " +
           "rate=48000 channels=2 format=s16le\n") %
          (sink_name, pipe_name))
      pulse_config.close()
    except IOError, e:
      logging.error("Failed to write pulseaudio config: " + str(e))
      return False

    self.child_env["PULSE_CONFIG_PATH"] = pulse_path
    self.child_env["PULSE_RUNTIME_PATH"] = pulse_path
    self.child_env["PULSE_STATE_PATH"] = pulse_path
    self.child_env["PULSE_SINK"] = sink_name
    self.pulseaudio_pipe = pipe_name

    return True

  def _launch_x_server(self, extra_x_args):
    x_auth_file = os.path.expanduser("~/.Xauthority")
    self.child_env["XAUTHORITY"] = x_auth_file
    devnull = open(os.devnull, "rw")
    display = self.get_unused_display_number()

    # Run "xauth add" with |child_env| so that it modifies the same XAUTHORITY
    # file which will be used for the X session.
    ret_code = subprocess.call("xauth add :%d . `mcookie`" % display,
                               env=self.child_env, shell=True)
    if ret_code != 0:
      raise Exception("xauth failed with code %d" % ret_code)

    max_width = max([width for width, height in self.sizes])
    max_height = max([height for width, height in self.sizes])

    try:
      # TODO(jamiewalch): This script expects to be installed alongside
      # Xvfb-randr, but that's no longer the case. Fix this once we have
      # a Xvfb-randr package that installs somewhere sensible.
      xvfb = "/usr/bin/Xvfb-randr"
      if not os.path.exists(xvfb):
        xvfb = locate_executable("Xvfb-randr")
      self.server_supports_exact_resize = True
    except Exception:
      xvfb = "Xvfb"
      self.server_supports_exact_resize = False

    logging.info("Starting %s on display :%d" % (xvfb, display))
    screen_option = "%dx%dx24" % (max_width, max_height)
    self.x_proc = subprocess.Popen([xvfb, ":%d" % display,
                                    "-noreset",
                                    "-auth", x_auth_file,
                                    "-nolisten", "tcp",
                                    "-screen", "0", screen_option
                                    ] + extra_x_args)
    if not self.x_proc.pid:
      raise Exception("Could not start Xvfb.")

    self.child_env["DISPLAY"] = ":%d" % display
    self.child_env["CHROME_REMOTE_DESKTOP_SESSION"] = "1"

    # Wait for X to be active.
    for _test in range(5):
      proc = subprocess.Popen("xdpyinfo", env=self.child_env, stdout=devnull)
      _pid, retcode = os.waitpid(proc.pid, 0)
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
    _pid, retcode = os.waitpid(proc.pid, 0)
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

  def _launch_x_session(self):
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

  def launch_session(self, x_args):
    self._init_child_env()
    self._setup_pulseaudio()
    self._launch_x_server(x_args)
    self._launch_x_session()

  def launch_host(self, host_config):
    # Start remoting host
    args = [locate_executable(HOST_BINARY_NAME), "--host-config=/dev/stdin"]
    if self.pulseaudio_pipe:
      args.append("--audio-pipe-name=%s" % self.pulseaudio_pipe)
    if self.server_supports_exact_resize:
      args.append("--server-supports-exact-resize")
    self.host_proc = subprocess.Popen(args, env=self.child_env,
                                      stdin=subprocess.PIPE)
    logging.info(args)
    if not self.host_proc.pid:
      raise Exception("Could not start Chrome Remote Desktop host")
    self.host_proc.stdin.write(json.dumps(host_config.data))
    self.host_proc.stdin.close()


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

  # If the session wrapper script (see below) is given a specific session as an
  # argument (such as ubuntu-2d on Ubuntu 12.04), the wrapper will run that
  # session instead of looking for custom .xsession files in the home directory.
  # So it's necessary to test for these files here.
  XSESSION_FILES = [
    "~/.chrome-remote-desktop-session",
    "~/.xsession",
    "~/.Xsession" ]
  for startup_file in XSESSION_FILES:
    startup_file = os.path.expanduser(startup_file)
    if os.path.exists(startup_file):
      # Use the same logic that a Debian system typically uses with ~/.xsession
      # (see /etc/X11/Xsession.d/50x11-common_determine-startup), to determine
      # exactly how to run this file.
      if os.access(startup_file, os.X_OK):
        return startup_file
      else:
        shell = os.environ.get("SHELL", "sh")
        return [shell, startup_file]

  # Choose a session wrapper script to run the session. On some systems,
  # /etc/X11/Xsession fails to load the user's .profile, so look for an
  # alternative wrapper that is more likely to match the script that the
  # system actually uses for console desktop sessions.
  SESSION_WRAPPERS = [
    "/usr/sbin/lightdm-session",
    "/etc/gdm/Xsession",
    "/etc/X11/Xsession" ]
  for session_wrapper in SESSION_WRAPPERS:
    if os.path.exists(session_wrapper):
      break
  else:
    # No session wrapper found.
    return None

  # On Ubuntu 12.04, the default session relies on 3D-accelerated hardware.
  # Trying to run this with a virtual X display produces weird results on some
  # systems (for example, upside-down and corrupt displays).  So if the
  # ubuntu-2d session is available, choose it explicitly.
  if os.path.exists("/usr/bin/unity-2d-panel"):
    return [session_wrapper, "/usr/bin/gnome-session --session=ubuntu-2d"]

  # Use the session wrapper by itself, and let the system choose a session.
  return session_wrapper


def locate_executable(exe_name):
  if SCRIPT_PATH == DEFAULT_INSTALL_PATH:
    # If we are installed in the default path, then search the host binary
    # only in the same directory.
    paths_to_try = [ DEFAULT_INSTALL_PATH ]
  else:
    paths_to_try = map(lambda p: os.path.join(SCRIPT_PATH, p),
                       [".", "../../out/Debug", "../../out/Release" ])
  for path in paths_to_try:
    exe_path = os.path.join(path, exe_name)
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


class SignalHandler:
  """Reload the config file on SIGHUP. Since we pass the configuration to the
  host processes via stdin, they can't reload it, so terminate them. They will
  be relaunched automatically with the new config."""

  def __init__(self, host_config):
    self.host_config = host_config

  def __call__(self, signum, _stackframe):
    if signum == signal.SIGHUP:
      logging.info("SIGHUP caught, restarting host.")
      self.host_config.load()
      for desktop in g_desktops:
        if desktop.host_proc:
          desktop.host_proc.send_signal(signal.SIGTERM)
    else:
      # Exit cleanly so the atexit handler, cleanup(), gets called.
      raise SystemExit


class RelaunchInhibitor:
  """Helper class for inhibiting launch of a child process before a timeout has
  elapsed.

  A managed process can be in one of these states:
    running, not inhibited (running == True)
    stopped and inhibited (running == False and is_inhibited() == True)
    stopped but not inhibited (running == False and is_inhibited() == False)

  Attributes:
    label: Name of the tracked process. Only used for logging.
    running: Whether the process is currently running.
    earliest_relaunch_time: Time before which the process should not be
      relaunched, or 0 if there is no limit.
    failures: The number of times that the process ran for less than a
      specified timeout, and had to be inhibited.  This count is reset to 0
      whenever the process has run for longer than the timeout.
  """

  def __init__(self, label):
    self.label = label
    self.running = False
    self.earliest_relaunch_time = 0
    self.failures = 0

  def is_inhibited(self):
    return (not self.running) and (time.time() < self.earliest_relaunch_time)

  def record_started(self, timeout):
    """Record that the process was launched, and set the inhibit time to
    |timeout| seconds in the future."""
    self.earliest_relaunch_time = time.time() + timeout
    self.running = True

  def record_stopped(self):
    """Record that the process was stopped, and adjust the failure count
    depending on whether the process ran long enough."""
    self.running = False
    if time.time() < self.earliest_relaunch_time:
      self.failures += 1
    else:
      self.failures = 0
    logging.info("Failure count for '%s' is now %d", self.label, self.failures)


def relaunch_self():
  cleanup()
  os.execvp(sys.argv[0], sys.argv)


def waitpid_with_timeout(pid, deadline):
  """Wrapper around os.waitpid() which waits until either a child process dies
  or the deadline elapses.

  Args:
    pid: Process ID to wait for, or -1 to wait for any child process.
    deadline: Waiting stops when time.time() exceeds this value.

  Returns:
    (pid, status): Same as for os.waitpid(), except that |pid| is 0 if no child
    changed state within the timeout.

  Raises:
    Same as for os.waitpid().
  """
  while time.time() < deadline:
    pid, status = os.waitpid(pid, os.WNOHANG)
    if pid != 0:
      return (pid, status)
    time.sleep(1)
  return (0, 0)


def waitpid_handle_exceptions(pid, deadline):
  """Wrapper around os.waitpid()/waitpid_with_timeout(), which waits until
  either a child process exits or the deadline elapses, and retries if certain
  exceptions occur.

  Args:
    pid: Process ID to wait for, or -1 to wait for any child process.
    deadline: If non-zero, waiting stops when time.time() exceeds this value.
      If zero, waiting stops when a child process exits.

  Returns:
    (pid, status): Same as for waitpid_with_timeout(). |pid| is non-zero if and
    only if a child exited during the wait.

  Raises:
    Same as for os.waitpid(), except:
      OSError with errno==EINTR causes the wait to be retried (this can happen,
      for example, if this parent process receives SIGHUP).
      OSError with errno==ECHILD means there are no child processes, and so
      this function sleeps until |deadline|. If |deadline| is zero, this is an
      error and the OSError exception is raised in this case.
  """
  while True:
    try:
      if deadline == 0:
        pid_result, status = os.waitpid(pid, 0)
      else:
        pid_result, status = waitpid_with_timeout(pid, deadline)
      return (pid_result, status)
    except OSError, e:
      if e.errno == errno.EINTR:
        continue
      elif e.errno == errno.ECHILD:
        now = time.time()
        if deadline == 0:
          # No time-limit and no child processes. This is treated as an error
          # (see docstring).
          raise
        elif deadline > now:
          time.sleep(deadline - now)
        return (0, 0)
      else:
        # Anything else is an unexpected error.
        raise


def main():
  EPILOG = """This script is not intended for use by end-users.  To configure
Chrome Remote Desktop, please install the app from the Chrome
Web Store: https://chrome.google.com/remotedesktop"""
  parser = optparse.OptionParser(
      usage="Usage: %prog [options] [ -- [ X server options ] ]",
      epilog=EPILOG)
  parser.add_option("-s", "--size", dest="size", action="append",
                    help="Dimensions of virtual desktop. This can be specified "
                    "multiple times to make multiple screen resolutions "
                    "available (if the Xvfb server supports this).")
  parser.add_option("-f", "--foreground", dest="foreground", default=False,
                    action="store_true",
                    help="Don't run as a background daemon.")
  parser.add_option("", "--start", dest="start", default=False,
                    action="store_true",
                    help="Start the host.")
  parser.add_option("-k", "--stop", dest="stop", default=False,
                    action="store_true",
                    help="Stop the daemon currently running.")
  parser.add_option("", "--check-running", dest="check_running", default=False,
                    action="store_true",
                    help="Return 0 if the daemon is running, or 1 otherwise.")
  parser.add_option("", "--config", dest="config", action="store",
                    help="Use the specified configuration file.")
  parser.add_option("", "--reload", dest="reload", default=False,
                    action="store_true",
                    help="Signal currently running host to reload the config.")
  parser.add_option("", "--add-user", dest="add_user", default=False,
                    action="store_true",
                    help="Add current user to the chrome-remote-desktop group.")
  parser.add_option("", "--host-version", dest="host_version", default=False,
                    action="store_true",
                    help="Prints version of the host.")
  (options, args) = parser.parse_args()

  # Determine the filename of the host configuration and PID files.
  if not options.config:
    options.config = os.path.join(CONFIG_DIR, "host#%s.json" % g_host_hash)
  pid_filename = os.path.splitext(options.config)[0] + ".pid"

  # Check for a modal command-line option (start, stop, etc.)
  if options.check_running:
    running, pid = PidFile(pid_filename).check()
    return 0 if (running and pid != 0) else 1

  if options.stop:
    running, pid = PidFile(pid_filename).check()
    if not running:
      print "The daemon is not currently running"
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

  if options.add_user:
    sudo_command = "gksudo --message" if os.getenv("DISPLAY") else "sudo -p"
    command = ("sudo -k && %(sudo)s "
               "\"Please enter your password to enable "
               "Chrome Remote Desktop: \" "
               "-- sh -c "
               "\"groupadd -f %(group)s && gpasswd --add %(user)s %(group)s\"" %
               { 'group': CHROME_REMOTING_GROUP_NAME,
                 'user': getpass.getuser(),
                 'sudo': sudo_command })
    return os.system(command) >> 8

  if options.host_version:
    # TODO(sergeyu): Also check RPM package version once we add RPM package.
    return os.system(locate_executable(HOST_BINARY_NAME) + " --version") >> 8

  if not options.start:
    # If no modal command-line options specified, print an error and exit.
    print >> sys.stderr, EPILOG
    return 1

  # Collate the list of sizes that XRANDR should support.
  if not options.size:
    default_sizes = DEFAULT_SIZES
    if os.environ.has_key(DEFAULT_SIZES_ENV_VAR):
      default_sizes = os.environ[DEFAULT_SIZES_ENV_VAR]
    options.size = default_sizes.split(",");

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

  # Determine the command-line to run the user's preferred X environment.
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

  # Register an exit handler to clean up session process and the PID file.
  atexit.register(cleanup)

  # Load the initial host configuration.
  host_config = Config(options.config)
  if (not host_config.load()):
    print >> sys.stderr, "Failed to load " + config_filename
    return 1

  # Register handler to re-load the configuration in response to signals.
  for s in [signal.SIGHUP, signal.SIGINT, signal.SIGTERM, signal.SIGUSR1]:
    signal.signal(s, SignalHandler(host_config))

  # Verify that the initial host configuration has the necessary fields.
  auth = Authentication()
  auth_config_valid = auth.copy_from(host_config)
  host = Host()
  host_config_valid = host.copy_from(host_config)
  if not host_config_valid or not auth_config_valid:
    logging.error("Failed to load host configuration.")
    return 1

  # Determine whether a desktop is already active for the specified host
  # host configuration.
  global g_pidfile
  g_pidfile = PidFile(pid_filename)
  running, pid = g_pidfile.check()
  if running:
    # Debian policy requires that services should "start" cleanly and return 0
    # if they are already running.
    print "Service already running."
    return 0

  # Record that we are running a desktop against for this configuration.
  g_pidfile.create()

  # Detach a separate "daemon" process to run the session, unless specifically
  # requested to run in the foreground.
  if not options.foreground:
    if not os.environ.has_key(LOG_FILE_ENV_VAR):
      log_file = tempfile.NamedTemporaryFile(
          prefix="chrome_remote_desktop_", delete=False)
      os.environ[LOG_FILE_ENV_VAR] = log_file.name
    daemonize(os.environ[LOG_FILE_ENV_VAR])

  g_pidfile.write_pid()

  logging.info("Using host_id: " + host.host_id)

  desktop = Desktop(sizes)

  # Keep track of the number of consecutive failures of any child process to
  # run for longer than a set period of time. The script will exit after a
  # threshold is exceeded.
  # There is no point in tracking the X session process separately, since it is
  # launched at (roughly) the same time as the X server, and the termination of
  # one of these triggers the termination of the other.
  x_server_inhibitor = RelaunchInhibitor("X server")
  host_inhibitor = RelaunchInhibitor("host")
  all_inhibitors = [x_server_inhibitor, host_inhibitor]

  # Don't allow relaunching the script on the first loop iteration.
  allow_relaunch_self = False

  while True:
    # Exit if a process failed too many times.
    for inhibitor in all_inhibitors:
      if inhibitor.failures >= MAX_LAUNCH_FAILURES:
        logging.error("Too many launch failures of '%s', exiting."
                      % inhibitor.label)
        return 1

    relaunch_times = []

    # If the session process or X server stops running (e.g. because the user
    # logged out), kill the other. This will trigger the next conditional block
    # as soon as os.waitpid() reaps its exit-code.
    if desktop.session_proc is None and desktop.x_proc is not None:
      logging.info("Terminating X server")
      desktop.x_proc.terminate()
    elif desktop.x_proc is None and desktop.session_proc is not None:
      logging.info("Terminating X session")
      desktop.session_proc.terminate()
    elif desktop.x_proc is None and desktop.session_proc is None:
      # Both processes have terminated.
      if (allow_relaunch_self and x_server_inhibitor.failures == 0 and
          host_inhibitor.failures == 0):
        # Since the user's desktop is already gone at this point, there's no
        # state to lose and now is a good time to pick up any updates to this
        # script that might have been installed.
        logging.info("Relaunching self")
        relaunch_self()
      else:
        # If there is a non-zero |failures| count, restarting the whole script
        # would lose this information, so just launch the session as normal.
        if x_server_inhibitor.is_inhibited():
          logging.info("Waiting before launching X server")
          relaunch_times.append(x_server_inhibitor.earliest_relaunch_time)
        else:
          logging.info("Launching X server and X session.")
          desktop.launch_session(args)
          x_server_inhibitor.record_started(BACKOFF_TIME)
          allow_relaunch_self = True

    if desktop.host_proc is None:
      if host_inhibitor.is_inhibited():
        logging.info("Waiting before launching host process")
        relaunch_times.append(host_inhibitor.earliest_relaunch_time)
      else:
        logging.info("Launching host process")
        desktop.launch_host(host_config)
        host_inhibitor.record_started(BACKOFF_TIME)

    deadline = min(relaunch_times) if relaunch_times else 0
    pid, status = waitpid_handle_exceptions(-1, deadline)
    if pid == 0:
      continue

    logging.info("wait() returned (%s,%s)" % (pid, status))

    # When a process has terminated, and we've reaped its exit-code, any Popen
    # instance for that process is no longer valid. Reset any affected instance
    # to None.
    if desktop.x_proc is not None and pid == desktop.x_proc.pid:
      logging.info("X server process terminated")
      desktop.x_proc = None
      x_server_inhibitor.record_stopped()

    if desktop.session_proc is not None and pid == desktop.session_proc.pid:
      logging.info("Session process terminated")
      desktop.session_proc = None

    if desktop.host_proc is not None and pid == desktop.host_proc.pid:
      logging.info("Host process terminated")
      desktop.host_proc = None
      host_inhibitor.record_stopped()

      # These exit-codes must match the ones used by the host.
      # See remoting/host/host_error_codes.h.
      # Delete the host or auth configuration depending on the returned error
      # code, so the next time this script is run, a new configuration
      # will be created and registered.
      if os.WEXITSTATUS(status) == 100:
        logging.info("Host configuration is invalid - exiting.")
        host_config.clear_auth()
        host_config.clear_host_info()
        host_config.save()
        return 0
      elif os.WEXITSTATUS(status) == 101:
        logging.info("Host ID has been deleted - exiting.")
        host_config.clear_host_info()
        host_config.save()
        return 0
      elif os.WEXITSTATUS(status) == 102:
        logging.info("OAuth credentials are invalid - exiting.")
        host_config.clear_auth()
        host_config.save()
        return 0
      elif os.WEXITSTATUS(status) == 103:
        logging.info("Host domain is blocked by policy - exiting.")
        host_config.clear_auth()
        host_config.clear_host_info()
        host_config.save()
        return 0
      # Nothing to do for Mac-only status 104 (login screen unsupported)
      elif os.WEXITSTATUS(status) == 105:
        logging.info("Username is blocked by policy - exiting.")
        host_config.clear_auth()
        host_config.clear_host_info()
        host_config.save()
        return 0


if __name__ == "__main__":
  logging.basicConfig(level=logging.DEBUG,
                      format="%(asctime)s:%(levelname)s:%(message)s")
  sys.exit(main())
