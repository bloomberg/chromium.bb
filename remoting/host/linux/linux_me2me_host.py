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
import fcntl
import getpass
import grp
import hashlib
import json
import logging
import optparse
import os
import pipes
import platform
import psutil
import platform
import signal
import socket
import subprocess
import sys
import tempfile
import time
import uuid

LOG_FILE_ENV_VAR = "CHROME_REMOTE_DESKTOP_LOG_FILE"

# This script has a sensible default for the initial and maximum desktop size,
# which can be overridden either on the command-line, or via a comma-separated
# list of sizes in this environment variable.
DEFAULT_SIZES_ENV_VAR = "CHROME_REMOTE_DESKTOP_DEFAULT_DESKTOP_SIZES"

# By default, provide a maximum size that is large enough to support clients
# with large or multiple monitors. This is a comma-separated list of
# resolutions that will be made available if the X server supports RANDR. These
# defaults can be overridden in ~/.profile.
DEFAULT_SIZES = "1600x1200,3840x2560"

# If RANDR is not available, use a smaller default size. Only a single
# resolution is supported in this case.
DEFAULT_SIZE_NO_RANDR = "1600x1200"

SCRIPT_PATH = sys.path[0]

IS_INSTALLED = (os.path.basename(sys.argv[0]) != 'linux_me2me_host.py')

if IS_INSTALLED:
  HOST_BINARY_NAME = "chrome-remote-desktop-host"
else:
  HOST_BINARY_NAME = "remoting_me2me_host"

CHROME_REMOTING_GROUP_NAME = "chrome-remote-desktop"

HOME_DIR = os.environ["HOME"]
CONFIG_DIR = os.path.join(HOME_DIR, ".config/chrome-remote-desktop")
SESSION_FILE_PATH = os.path.join(HOME_DIR, ".chrome-remote-desktop-session")
SYSTEM_SESSION_FILE_PATH = "/etc/chrome-remote-desktop-session"

X_LOCK_FILE_TEMPLATE = "/tmp/.X%d-lock"
FIRST_X_DISPLAY_NUMBER = 20

# Amount of time to wait between relaunching processes.
SHORT_BACKOFF_TIME = 5
LONG_BACKOFF_TIME = 60

# How long a process must run in order not to be counted against the restart
# thresholds.
MINIMUM_PROCESS_LIFETIME = 60

# Thresholds for switching from fast- to slow-restart and for giving up
# trying to restart entirely.
SHORT_BACKOFF_THRESHOLD = 5
MAX_LAUNCH_FAILURES = SHORT_BACKOFF_THRESHOLD + 10

# Globals needed by the atexit cleanup() handler.
g_desktops = []
g_host_hash = hashlib.md5(socket.gethostname()).hexdigest()


def is_supported_platform():
  # Always assume that the system is supported if the config directory or
  # session file exist.
  if (os.path.isdir(CONFIG_DIR) or os.path.isfile(SESSION_FILE_PATH) or
      os.path.isfile(SYSTEM_SESSION_FILE_PATH)):
    return True

  # The host has been tested only on Ubuntu.
  distribution = platform.linux_distribution()
  return (distribution[0]).lower() == 'ubuntu'


def get_randr_supporting_x_server():
  """Returns a path to an X server that supports the RANDR extension, if this
  is found on the system. Otherwise returns None."""
  try:
    xvfb = "/usr/bin/Xvfb-randr"
    if not os.path.exists(xvfb):
      xvfb = locate_executable("Xvfb-randr")
    return xvfb
  except Exception:
    return None


class Config:
  def __init__(self, path):
    self.path = path
    self.data = {}
    self.changed = False

  def load(self):
    """Loads the config from file.

    Raises:
      IOError: Error reading data
      ValueError: Error parsing JSON
    """
    settings_file = open(self.path, 'r')
    self.data = json.load(settings_file)
    self.changed = False
    settings_file.close()

  def save(self):
    """Saves the config to file.

    Raises:
      IOError: Error writing data
      TypeError: Error serialising JSON
    """
    if not self.changed:
      return
    old_umask = os.umask(0066)
    try:
      settings_file = open(self.path, 'w')
      settings_file.write(json.dumps(self.data, indent=2))
      settings_file.close()
      self.changed = False
    finally:
      os.umask(old_umask)

  def save_and_log_errors(self):
    """Calls self.save(), trapping and logging any errors."""
    try:
      self.save()
    except (IOError, TypeError) as e:
      logging.error("Failed to save config: " + str(e))

  def get(self, key):
    return self.data.get(key)

  def __getitem__(self, key):
    return self.data[key]

  def __setitem__(self, key, value):
    self.data[key] = value
    self.changed = True

  def clear(self):
    self.data = {}
    self.changed = True


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
    self.host_ready = False
    self.ssh_auth_sockname = None
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

    # Ensure that the software-rendering GL drivers are loaded by the desktop
    # session, instead of any hardware GL drivers installed on the system.
    self.child_env["LD_LIBRARY_PATH"] = (
        "/usr/lib/%(arch)s-linux-gnu/mesa:"
        "/usr/lib/%(arch)s-linux-gnu/dri:"
        "/usr/lib/%(arch)s-linux-gnu/gallium-pipe" %
        { "arch": platform.machine() })

    # Read from /etc/environment if it exists, as it is a standard place to
    # store system-wide environment settings. During a normal login, this would
    # typically be done by the pam_env PAM module, depending on the local PAM
    # configuration.
    env_filename = "/etc/environment"
    try:
      with open(env_filename, "r") as env_file:
        for line in env_file:
          line = line.rstrip("\n")
          # Split at the first "=", leaving any further instances in the value.
          key_value_pair = line.split("=", 1)
          if len(key_value_pair) == 2:
            key, value = tuple(key_value_pair)
            # The file stores key=value assignments, but the value may be
            # quoted, so strip leading & trailing quotes from it.
            value = value.strip("'\"")
            self.child_env[key] = value
    except IOError:
      logging.info("Failed to read %s, skipping." % env_filename)

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
    except IOError, e:
      logging.error("Failed to create pulseaudio pipe: " + str(e))
      return False

    try:
      pulse_config = open(os.path.join(pulse_path, "daemon.conf"), "w")
      pulse_config.write("default-sample-format = s16le\n")
      pulse_config.write("default-sample-rate = 48000\n")
      pulse_config.write("default-sample-channels = 2\n")
      pulse_config.close()

      pulse_script = open(os.path.join(pulse_path, "default.pa"), "w")
      pulse_script.write("load-module module-native-protocol-unix\n")
      pulse_script.write(
          ("load-module module-pipe-sink sink_name=%s file=\"%s\" " +
           "rate=48000 channels=2 format=s16le\n") %
          (sink_name, pipe_name))
      pulse_script.close()
    except IOError, e:
      logging.error("Failed to write pulseaudio config: " + str(e))
      return False

    self.child_env["PULSE_CONFIG_PATH"] = pulse_path
    self.child_env["PULSE_RUNTIME_PATH"] = pulse_path
    self.child_env["PULSE_STATE_PATH"] = pulse_path
    self.child_env["PULSE_SINK"] = sink_name
    self.pulseaudio_pipe = pipe_name

    return True

  def _setup_gnubby(self):
    self.ssh_auth_sockname = ("/tmp/chromoting.%s.ssh_auth_sock" %
                              os.environ["USER"])

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

    xvfb = get_randr_supporting_x_server()
    if xvfb:
      self.server_supports_exact_resize = True
    else:
      xvfb = "Xvfb"
      self.server_supports_exact_resize = False

    # Disable the Composite extension iff the X session is the default
    # Unity-2D, since it uses Metacity which fails to generate DAMAGE
    # notifications correctly. See crbug.com/166468.
    x_session = choose_x_session()
    if (len(x_session) == 2 and
        x_session[1] == "/usr/bin/gnome-session --session=ubuntu-2d"):
      extra_x_args.extend(["-extension", "Composite"])

    logging.info("Starting %s on display :%d" % (xvfb, display))
    screen_option = "%dx%dx24" % (max_width, max_height)
    self.x_proc = subprocess.Popen(
        [xvfb, ":%d" % display,
         "-auth", x_auth_file,
         "-nolisten", "tcp",
         "-noreset",
         "-screen", "0", screen_option
        ] + extra_x_args)
    if not self.x_proc.pid:
      raise Exception("Could not start Xvfb.")

    self.child_env["DISPLAY"] = ":%d" % display
    self.child_env["CHROME_REMOTE_DESKTOP_SESSION"] = "1"

    # Use a separate profile for any instances of Chrome that are started in
    # the virtual session. Chrome doesn't support sharing a profile between
    # multiple DISPLAYs, but Chrome Sync allows for a reasonable compromise.
    chrome_profile = os.path.join(CONFIG_DIR, "chrome-profile")
    self.child_env["CHROME_USER_DATA_DIR"] = chrome_profile

    # Set SSH_AUTH_SOCK to the file name to listen on.
    if self.ssh_auth_sockname:
      self.child_env["SSH_AUTH_SOCK"] = self.ssh_auth_sockname

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
      subprocess.call(args, env=self.child_env, stdout=devnull, stderr=devnull)
      args = ["xrandr", "--addmode", "screen", label]
      subprocess.call(args, env=self.child_env, stdout=devnull, stderr=devnull)

    # Set the initial mode to the first size specified, otherwise the X server
    # would default to (max_width, max_height), which might not even be in the
    # list.
    label = "%dx%d" % self.sizes[0]
    args = ["xrandr", "-s", label]
    subprocess.call(args, env=self.child_env, stdout=devnull, stderr=devnull)

    # Set the physical size of the display so that the initial mode is running
    # at approximately 96 DPI, since some desktops require the DPI to be set to
    # something realistic.
    args = ["xrandr", "--dpi", "96"]
    subprocess.call(args, env=self.child_env, stdout=devnull, stderr=devnull)

    devnull.close()

  def _launch_x_session(self):
    # Start desktop session.
    # The /dev/null input redirection is necessary to prevent the X session
    # reading from stdin.  If this code runs as a shell background job in a
    # terminal, any reading from stdin causes the job to be suspended.
    # Daemonization would solve this problem by separating the process from the
    # controlling terminal.
    xsession_command = choose_x_session()
    if xsession_command is None:
      raise Exception("Unable to choose suitable X session command.")

    logging.info("Launching X session: %s" % xsession_command)
    self.session_proc = subprocess.Popen(xsession_command,
                                         stdin=open(os.devnull, "r"),
                                         cwd=HOME_DIR,
                                         env=self.child_env)
    if not self.session_proc.pid:
      raise Exception("Could not start X session")

  def launch_session(self, x_args):
    self._init_child_env()
    self._setup_pulseaudio()
    self._setup_gnubby()
    self._launch_x_server(x_args)
    self._launch_x_session()

  def launch_host(self, host_config):
    # Start remoting host
    args = [locate_executable(HOST_BINARY_NAME), "--host-config=-"]
    if self.pulseaudio_pipe:
      args.append("--audio-pipe-name=%s" % self.pulseaudio_pipe)
    if self.server_supports_exact_resize:
      args.append("--server-supports-exact-resize")
    if self.ssh_auth_sockname:
      args.append("--ssh-auth-sockname=%s" % self.ssh_auth_sockname)

    # Have the host process use SIGUSR1 to signal a successful start.
    def sigusr1_handler(signum, frame):
      _ = signum, frame
      logging.info("Host ready to receive connections.")
      self.host_ready = True
      if (ParentProcessLogger.instance() and
          False not in [desktop.host_ready for desktop in g_desktops]):
        ParentProcessLogger.instance().release_parent()

    signal.signal(signal.SIGUSR1, sigusr1_handler)
    args.append("--signal-parent")

    self.host_proc = subprocess.Popen(args, env=self.child_env,
                                      stdin=subprocess.PIPE)
    logging.info(args)
    if not self.host_proc.pid:
      raise Exception("Could not start Chrome Remote Desktop host")
    self.host_proc.stdin.write(json.dumps(host_config.data))
    self.host_proc.stdin.close()


def get_daemon_proc():
  """Checks if there is already an instance of this script running, and returns
  a psutil.Process instance for it.

  Returns:
    A Process instance for the existing daemon process, or None if the daemon
    is not running.
  """

  uid = os.getuid()
  this_pid = os.getpid()

  # Support new & old psutil API. This is the right way to check, according to
  # http://grodola.blogspot.com/2014/01/psutil-20-porting.html
  if psutil.version_info >= (2, 0):
    psget = lambda x: x()
  else:
    psget = lambda x: x

  for process in psutil.process_iter():
    # Skip any processes that raise an exception, as processes may terminate
    # during iteration over the list.
    try:
      # Skip other users' processes.
      if psget(process.uids).real != uid:
        continue

      # Skip the process for this instance.
      if process.pid == this_pid:
        continue

      # |cmdline| will be [python-interpreter, script-file, other arguments...]
      cmdline = psget(process.cmdline)
      if len(cmdline) < 2:
        continue
      if cmdline[0] == sys.executable and cmdline[1] == sys.argv[0]:
        return process
    except (psutil.NoSuchProcess, psutil.AccessDenied):
      continue

  return None


def choose_x_session():
  """Chooses the most appropriate X session command for this system.

  Returns:
    A string containing the command to run, or a list of strings containing
    the executable program and its arguments, which is suitable for passing as
    the first parameter of subprocess.Popen().  If a suitable session cannot
    be found, returns None.
  """
  XSESSION_FILES = [
    SESSION_FILE_PATH,
    SYSTEM_SESSION_FILE_PATH ]
  for startup_file in XSESSION_FILES:
    startup_file = os.path.expanduser(startup_file)
    if os.path.exists(startup_file):
      # Use the same logic that a Debian system typically uses with ~/.xsession
      # (see /etc/X11/Xsession.d/50x11-common_determine-startup), to determine
      # exactly how to run this file.
      if os.access(startup_file, os.X_OK):
        # "/bin/sh -c" is smart about how to execute the session script and
        # works in cases where plain exec() fails (for example, if the file is
        # marked executable, but is a plain script with no shebang line).
        return ["/bin/sh", "-c", pipes.quote(startup_file)]
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
      if os.path.exists("/usr/bin/unity-2d-panel"):
        # On Ubuntu 12.04, the default session relies on 3D-accelerated
        # hardware. Trying to run this with a virtual X display produces
        # weird results on some systems (for example, upside-down and
        # corrupt displays).  So if the ubuntu-2d session is available,
        # choose it explicitly.
        return [session_wrapper, "/usr/bin/gnome-session --session=ubuntu-2d"]
      else:
        # Use the session wrapper by itself, and let the system choose a
        # session.
        return [session_wrapper]
  return None


def locate_executable(exe_name):
  if IS_INSTALLED:
    # If the script is running from its installed location, search the host
    # binary only in the same directory.
    paths_to_try = [ SCRIPT_PATH ]
  else:
    paths_to_try = map(lambda p: os.path.join(SCRIPT_PATH, p),
                       [".", "../../../out/Debug", "../../../out/Release" ])
  for path in paths_to_try:
    exe_path = os.path.join(path, exe_name)
    if os.path.exists(exe_path):
      return exe_path

  raise Exception("Could not locate executable '%s'" % exe_name)


class ParentProcessLogger(object):
  """Redirects logs to the parent process, until the host is ready or quits.

  This class creates a pipe to allow logging from the daemon process to be
  copied to the parent process. The daemon process adds a log-handler that
  directs logging output to the pipe. The parent process reads from this pipe
  until and writes the content to stderr.  When the pipe is no longer needed
  (for example, the host signals successful launch or permanent failure), the
  daemon removes the log-handler and closes the pipe, causing the the parent
  process to reach end-of-file while reading the pipe and exit.

  The (singleton) logger should be instantiated before forking. The parent
  process should call wait_for_logs() before exiting. The (grand-)child process
  should call start_logging() when it starts, and then use logging.* to issue
  log statements, as usual. When the child has either succesfully started the
  host or terminated, it must call release_parent() to allow the parent to exit.
  """

  __instance = None

  def __init__(self):
    """Constructor. Must be called before forking."""
    read_pipe, write_pipe = os.pipe()
    # Ensure write_pipe is closed on exec, otherwise it will be kept open by
    # child processes (X, host), preventing the read pipe from EOF'ing.
    old_flags = fcntl.fcntl(write_pipe, fcntl.F_GETFD)
    fcntl.fcntl(write_pipe, fcntl.F_SETFD, old_flags | fcntl.FD_CLOEXEC)
    self._read_file = os.fdopen(read_pipe, 'r')
    self._write_file = os.fdopen(write_pipe, 'a')
    self._logging_handler = None
    ParentProcessLogger.__instance = self

  def start_logging(self):
    """Installs a logging handler that sends log entries to a pipe.

    Must be called by the child process.
    """
    self._read_file.close()
    self._logging_handler = logging.StreamHandler(self._write_file)
    logging.getLogger().addHandler(self._logging_handler)

  def release_parent(self):
    """Uninstalls logging handler and closes the pipe, releasing the parent.

    Must be called by the child process.
    """
    if self._logging_handler:
      logging.getLogger().removeHandler(self._logging_handler)
      self._logging_handler = None
    if not self._write_file.closed:
      self._write_file.close()

  def wait_for_logs(self):
    """Waits and prints log lines from the daemon until the pipe is closed.

    Must be called by the parent process.
    """
    # If Ctrl-C is pressed, inform the user that the daemon is still running.
    # This signal will cause the read loop below to stop with an EINTR IOError.
    def sigint_handler(signum, frame):
      _ = signum, frame
      print >> sys.stderr, ("Interrupted. The daemon is still running in the "
                            "background.")

    signal.signal(signal.SIGINT, sigint_handler)

    # Install a fallback timeout to release the parent process, in case the
    # daemon never responds (e.g. host crash-looping, daemon killed).
    # This signal will cause the read loop below to stop with an EINTR IOError.
    def sigalrm_handler(signum, frame):
      _ = signum, frame
      print >> sys.stderr, ("No response from daemon. It may have crashed, or "
                            "may still be running in the background.")

    signal.signal(signal.SIGALRM, sigalrm_handler)
    signal.alarm(30)

    self._write_file.close()

    # Print lines as they're logged to the pipe until EOF is reached or readline
    # is interrupted by one of the signal handlers above.
    try:
      for line in iter(self._read_file.readline, ''):
        sys.stderr.write(line)
    except IOError as e:
      if e.errno != errno.EINTR:
        raise
    print >> sys.stderr, "Log file: %s" % os.environ[LOG_FILE_ENV_VAR]

  @staticmethod
  def instance():
    """Returns the singleton instance, if it exists."""
    return ParentProcessLogger.__instance


def daemonize():
  """Background this process and detach from controlling terminal, redirecting
  stdout/stderr to a log file."""

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
  if not os.environ.has_key(LOG_FILE_ENV_VAR):
    log_file_prefix = "chrome_remote_desktop_%s_" % time.strftime(
        '%Y%m%d_%H%M%S', time.localtime(time.time()))
    log_file = tempfile.NamedTemporaryFile(prefix=log_file_prefix, delete=False)
    os.environ[LOG_FILE_ENV_VAR] = log_file.name
    log_fd = log_file.file.fileno()
  else:
    log_fd = os.open(os.environ[LOG_FILE_ENV_VAR],
                     os.O_WRONLY | os.O_CREAT | os.O_APPEND, 0600)

  devnull_fd = os.open(os.devnull, os.O_RDONLY)

  parent_logger = ParentProcessLogger()

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
      os._exit(0)  # pylint: disable=W0212
  else:
    # Parent process
    parent_logger.wait_for_logs()
    os._exit(0)  # pylint: disable=W0212

  logging.info("Daemon process started in the background, logging to '%s'" %
               os.environ[LOG_FILE_ENV_VAR])

  os.chdir(HOME_DIR)

  parent_logger.start_logging()

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

  global g_desktops
  for desktop in g_desktops:
    if desktop.x_proc:
      logging.info("Terminating Xvfb")
      desktop.x_proc.terminate()
  g_desktops = []
  if ParentProcessLogger.instance():
    ParentProcessLogger.instance().release_parent()

class SignalHandler:
  """Reload the config file on SIGHUP. Since we pass the configuration to the
  host processes via stdin, they can't reload it, so terminate them. They will
  be relaunched automatically with the new config."""

  def __init__(self, host_config):
    self.host_config = host_config

  def __call__(self, signum, _stackframe):
    if signum == signal.SIGHUP:
      logging.info("SIGHUP caught, restarting host.")
      try:
        self.host_config.load()
      except (IOError, ValueError) as e:
        logging.error("Failed to load config: " + str(e))
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
    self.earliest_successful_termination = 0
    self.failures = 0

  def is_inhibited(self):
    return (not self.running) and (time.time() < self.earliest_relaunch_time)

  def record_started(self, minimum_lifetime, relaunch_delay):
    """Record that the process was launched, and set the inhibit time to
    |timeout| seconds in the future."""
    self.earliest_relaunch_time = time.time() + relaunch_delay
    self.earliest_successful_termination = time.time() + minimum_lifetime
    self.running = True

  def record_stopped(self):
    """Record that the process was stopped, and adjust the failure count
    depending on whether the process ran long enough."""
    self.running = False
    if time.time() < self.earliest_successful_termination:
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
  parser.add_option("", "--get-status", dest="get_status", default=False,
                    action="store_true",
                    help="Prints host status")
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

  # Check for a modal command-line option (start, stop, etc.)

  if options.get_status:
    proc = get_daemon_proc()
    if proc is not None:
      print "STARTED"
    elif is_supported_platform():
      print "STOPPED"
    else:
      print "NOT_IMPLEMENTED"
    return 0

  # TODO(sergeyu): Remove --check-running once NPAPI plugin and NM host are
  # updated to always use get-status flag instead.
  if options.check_running:
    proc = get_daemon_proc()
    return 1 if proc is None else 0

  if options.stop:
    proc = get_daemon_proc()
    if proc is None:
      print "The daemon is not currently running"
    else:
      print "Killing process %s" % proc.pid
      proc.terminate()
      try:
        proc.wait(timeout=30)
      except psutil.TimeoutExpired:
        print "Timed out trying to kill daemon process"
        return 1
    return 0

  if options.reload:
    proc = get_daemon_proc()
    if proc is None:
      return 1
    proc.send_signal(signal.SIGHUP)
    return 0

  if options.add_user:
    user = getpass.getuser()
    try:
      if user in grp.getgrnam(CHROME_REMOTING_GROUP_NAME).gr_mem:
        logging.info("User '%s' is already a member of '%s'." %
                     (user, CHROME_REMOTING_GROUP_NAME))
        return 0
    except KeyError:
      logging.info("Group '%s' not found." % CHROME_REMOTING_GROUP_NAME)

    if os.getenv("DISPLAY"):
      sudo_command = "gksudo --description \"Chrome Remote Desktop\""
    else:
      sudo_command = "sudo"
    command = ("sudo -k && exec %(sudo)s -- sh -c "
               "\"groupadd -f %(group)s && gpasswd --add %(user)s %(group)s\"" %
               { 'group': CHROME_REMOTING_GROUP_NAME,
                 'user': user,
                 'sudo': sudo_command })
    os.execv("/bin/sh", ["/bin/sh", "-c", command])
    return 1

  if options.host_version:
    # TODO(sergeyu): Also check RPM package version once we add RPM package.
    return os.system(locate_executable(HOST_BINARY_NAME) + " --version") >> 8

  if not options.start:
    # If no modal command-line options specified, print an error and exit.
    print >> sys.stderr, EPILOG
    return 1

  # If a RANDR-supporting Xvfb is not available, limit the default size to
  # something more sensible.
  if get_randr_supporting_x_server():
    default_sizes = DEFAULT_SIZES
  else:
    default_sizes = DEFAULT_SIZE_NO_RANDR

  # Collate the list of sizes that XRANDR should support.
  if not options.size:
    if os.environ.has_key(DEFAULT_SIZES_ENV_VAR):
      default_sizes = os.environ[DEFAULT_SIZES_ENV_VAR]
    options.size = default_sizes.split(",")

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

  # Register an exit handler to clean up session process and the PID file.
  atexit.register(cleanup)

  # Load the initial host configuration.
  host_config = Config(options.config)
  try:
    host_config.load()
  except (IOError, ValueError) as e:
    print >> sys.stderr, "Failed to load config: " + str(e)
    return 1

  # Register handler to re-load the configuration in response to signals.
  for s in [signal.SIGHUP, signal.SIGINT, signal.SIGTERM]:
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
  proc = get_daemon_proc()
  if proc is not None:
    # Debian policy requires that services should "start" cleanly and return 0
    # if they are already running.
    print "Service already running."
    return 0

  # Detach a separate "daemon" process to run the session, unless specifically
  # requested to run in the foreground.
  if not options.foreground:
    daemonize()

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
    # Set the backoff interval and exit if a process failed too many times.
    backoff_time = SHORT_BACKOFF_TIME
    for inhibitor in all_inhibitors:
      if inhibitor.failures >= MAX_LAUNCH_FAILURES:
        logging.error("Too many launch failures of '%s', exiting."
                      % inhibitor.label)
        return 1
      elif inhibitor.failures >= SHORT_BACKOFF_THRESHOLD:
        backoff_time = LONG_BACKOFF_TIME

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
          x_server_inhibitor.record_started(MINIMUM_PROCESS_LIFETIME,
                                            backoff_time)
          allow_relaunch_self = True

    if desktop.host_proc is None:
      if host_inhibitor.is_inhibited():
        logging.info("Waiting before launching host process")
        relaunch_times.append(host_inhibitor.earliest_relaunch_time)
      else:
        logging.info("Launching host process")
        desktop.launch_host(host_config)
        host_inhibitor.record_started(MINIMUM_PROCESS_LIFETIME,
                                      backoff_time)

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
      desktop.host_ready = False
      host_inhibitor.record_stopped()

      # These exit-codes must match the ones used by the host.
      # See remoting/host/host_error_codes.h.
      # Delete the host or auth configuration depending on the returned error
      # code, so the next time this script is run, a new configuration
      # will be created and registered.
      if os.WIFEXITED(status):
        if os.WEXITSTATUS(status) == 100:
          logging.info("Host configuration is invalid - exiting.")
          return 0
        elif os.WEXITSTATUS(status) == 101:
          logging.info("Host ID has been deleted - exiting.")
          host_config.clear()
          host_config.save_and_log_errors()
          return 0
        elif os.WEXITSTATUS(status) == 102:
          logging.info("OAuth credentials are invalid - exiting.")
          return 0
        elif os.WEXITSTATUS(status) == 103:
          logging.info("Host domain is blocked by policy - exiting.")
          return 0
        # Nothing to do for Mac-only status 104 (login screen unsupported)
        elif os.WEXITSTATUS(status) == 105:
          logging.info("Username is blocked by policy - exiting.")
          return 0
        else:
          logging.info("Host exited with status %s." % os.WEXITSTATUS(status))
      elif os.WIFSIGNALED(status):
        logging.info("Host terminated by signal %s." % os.WTERMSIG(status))


if __name__ == "__main__":
  logging.basicConfig(level=logging.DEBUG,
                      format="%(asctime)s:%(levelname)s:%(message)s")
  sys.exit(main())
