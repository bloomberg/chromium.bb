#!/usr/bin/env python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Virtual Me2Me implementation.  This script runs and manages the processes
# required for a Virtual Me2Me desktop, which are: X server, X desktop
# session, and Host process.
# This script is intended to run continuously as a background daemon
# process, running under an ordinary (non-root) user account.

import atexit
import getpass
import json
import logging
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

SCRIPT_PATH = os.path.dirname(sys.argv[0])
if not SCRIPT_PATH:
  SCRIPT_PATH = os.getcwd()

# These are relative to SCRIPT_PATH.
EXE_PATHS_TO_TRY = [
    ".",
    "../../out/Debug",
    "../../out/Release"
]

CONFIG_DIR = os.path.expanduser("~/.config/chrome-remote-desktop")

X_LOCK_FILE_TEMPLATE = "/tmp/.X%d-lock"
FIRST_X_DISPLAY_NUMBER = 20

X_AUTH_FILE = os.path.expanduser("~/.Xauthority")
os.environ["XAUTHORITY"] = X_AUTH_FILE

def locate_executable(exe_name):
  for path in EXE_PATHS_TO_TRY:
    exe_path = os.path.join(SCRIPT_PATH, path, exe_name)
    if os.path.exists(exe_path):
      return exe_path
  raise Exception("Could not locate executable '%s'" % exe_name)

g_desktops = []

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
    os.umask(0066) # Set permission mask for created file.
    settings_file = open(self.config_file, 'w')
    settings_file.write(json.dumps(data, indent=2))
    settings_file.close()


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


def cleanup():
  logging.info("Cleanup.")

  for desktop in g_desktops:
    if desktop.x_proc:
      logging.info("Terminating Xvfb")
      desktop.x_proc.terminate()

def signal_handler(signum, stackframe):
    # Exit cleanly so the atexit handler, cleanup(), gets called.
    raise SystemExit


class Desktop:
  """Manage a single virtual desktop"""
  def __init__(self):
    self.x_proc = None
    g_desktops.append(self)

  @staticmethod
  def get_unused_display_number():
    """Return a candidate display number for which there is currently no
    X Server lock file"""
    display = FIRST_X_DISPLAY_NUMBER
    while os.path.exists(X_LOCK_FILE_TEMPLATE % display):
      display += 1
    return display

  def launch_x_server(self):
    display = self.get_unused_display_number()
    ret_code = subprocess.call("xauth add :%d . `mcookie`" % display,
                               shell=True)
    if ret_code != 0:
      raise Exception("xauth failed with code %d" % ret_code)

    logging.info("Starting Xvfb on display :%d" % display);
    self.x_proc = subprocess.Popen(["Xvfb", ":%d" % display,
                                    "-auth", X_AUTH_FILE,
                                    "-nolisten", "tcp",
                                    "-screen", "0", "1024x768x24",
                                    ])
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
    session_proc = subprocess.Popen("/etc/X11/Xsession",
                                    cwd=os.environ["HOME"],
                                    env=self.child_env)
    if not session_proc.pid:
      raise Exception("Could not start X session")

  def launch_host(self):
    # Start remoting host
    command = locate_executable(REMOTING_COMMAND)
    self.host_proc = subprocess.Popen(command, env=self.child_env)
    if not self.host_proc.pid:
      raise Exception("Could not start remoting host")


def main():
  atexit.register(cleanup)

  for s in [signal.SIGINT, signal.SIGTERM]:
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

  host = Host(os.path.join(CONFIG_DIR, "host.json"))

  if not host.load_config():
    host.create_config(auth)
    host.save_config()

  logging.info("Using host_id: " + host.host_id)

  desktop = Desktop()
  desktop.launch_x_server()
  desktop.launch_x_session()
  desktop.launch_host()

  while True:
    pid, status = os.wait()
    logging.info("wait() returned (%s,%s)" % (pid, status))

    if pid == desktop.x_proc.pid:
      logging.info("X server process terminated with code %d", status)
      break

    if pid == desktop.host_proc.pid:
      logging.info("Host process terminated, relaunching")
      desktop.launch_host()

if __name__ == "__main__":
  logging.basicConfig(level=logging.DEBUG)
  sys.exit(main())
