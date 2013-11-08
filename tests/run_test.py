#!/usr/bin/python
# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Runs a single test case.

On success returns 0. On failure returns a negative value. See
websocket_handler.py for possible return values.

To use, run ./run_test.py test_case_name timeout_in_secs. Note, that all of the
Chrome windows will be killed.

The test runner runs a testing package of the Camera app within a new instance
of chrome, and starts a websocket server. The Camera app tries to connect to it
as soon as possible, and waits for the test case name to be run. After that,
messages are sent to the test runner. Possible messages are: SUCCESS, FAILURE,
INFO and COMMAND. The FAILURE one will cause tests fail. At least one SUCCESS
is required to pass the test. COMMAND messages are used to control the hardware,
eg. detach and reattach the USB camera device.

Note, that the Websocket implementation is trivial, and just a subset of the
protocol. However, it is enough for the purpose of this test runner.
"""


import argparse
import glob
import os
import signal
import subprocess
import sys
import websocket_handler

# Location of the Camera app.
SELF_PATH = os.path.dirname(os.path.abspath(__file__))
CAMERA_PATH = os.path.join(SELF_PATH, '../build/tests')

# App ID of the testing version of the Camera app.
# Keep in sync with manifest_tests.json.
CAMERA_APP_ID = 'mbflcebpggnecokmikipoihdbecnjfoj'

# Port to be used to communicate with the Camera app. Keep in sync with
# src/js/test.js.
WEBSOCKET_PORT = 47552


def RunCommand(command):
  """Runs a command, waits and returns the error code."""
  process = subprocess.Popen(command, shell=False)
  process.wait()
  return process.returncode


class TestCaseRunner(object):
  """Test case runner."""

  def __init__(self, test_case, test_timeout, chrome_binary, chrome_args):
    self.__server = None
    self.__test_case = test_case
    self.__test_timeout = test_timeout
    self.__chrome_binary = chrome_binary
    self.__chrome_args = chrome_args
    self.__chrome_process = None

  def Run(self):
    """Runs the test case with the specified timeout passed as arguments.

    Returns:
      The error code.

    Raises:
      Exception if called more than once per instance.
    """

    if self.__server:
      raise Exception('Unable to call Run() more than once.')

    print 'Step 1. Set the timeout.'
    def Timeout(signum, frame):
      print 'Timeout.'
      if self.__server:
        self.__server.Terminate()
      if self.__chrome_process:
        self.__chrome_process.kill()

      # Terminate forcibly.
      os._exit(websocket_handler.STATUS_INTERNAL_ERROR)

    signal.signal(signal.SIGALRM, Timeout)
    signal.alarm(self.__test_timeout)

    print 'Step 2. Uninstall the testing version of Camera app.'
    run_command = [
        self.__chrome_binary, '--verbose', '--enable-logging',
        '--uninstall-extension=%s' % CAMERA_APP_ID] + self.__chrome_args
    subprocess.Popen(run_command, shell=False).wait()

    print 'Step 3. Restart the camera module.'
    if RunCommand(['sudo', os.path.join(SELF_PATH, 'camera_reset.py')]) != 0:
      print 'Failed to reload the camera kernel module.'
      return websocket_handler.STATUS_INTERNAL_ERROR

    print 'Step 4. Fetch camera devices.'
    camera_devices = [os.path.basename(usb_id)
                      for usb_id in
                      glob.glob('/sys/bus/usb/drivers/uvcvideo/?-*')]
    if not camera_devices:
      print 'No cameras detected.'
      return websocket_handler.STATUS_INTERNAL_ERROR

    print 'Step 5. Check if there is a camera attached.'
    if not glob.glob('/dev/video*'):
      print 'Camera device is missing.'
      return websocket_handler.STATUS_INTERNAL_ERROR

    print('Step 6. Start the websocket server for communication with the'
          'Camera app.')
    def HandleWebsocketCommand(name):
      if name == 'detach':
        for camera_device in camera_devices:
          if not os.path.exists('/sys/bus/usb/drivers/uvcvideo/%s' %
                                camera_device):
            continue
          if RunCommand(['sudo', os.path.join(SELF_PATH, 'camera_ctl.py'),
                         camera_device, 'unbind']) != 0:
            print 'Failed to detach a camera.'
            return websocket_handler.STATUS_INTERNAL_ERROR
      if name == 'attach':
        for camera_device in camera_devices:
          if os.path.exists('/sys/bus/usb/drivers/uvcvideo/%s' % camera_device):
            continue
          if RunCommand(['sudo', os.path.join(SELF_PATH, 'camera_ctl.py'),
                         camera_device, 'bind']) != 0:
            print 'Failed to attach the camera.'
            return websocket_handler.STATUS_INTERNAL_ERROR

    self.__server = websocket_handler.Server(
        ('localhost', WEBSOCKET_PORT), websocket_handler.Handler,
        HandleWebsocketCommand, self.__test_case)

    print 'Step 7. Install the Camera app.'
    # If already installed, then it will be reinstalled automatically. Do not
    # wait, since the process may be detached in case of reusing an existing
    # process.
    run_command = [
        self.__chrome_binary, '--verbose', '--enable-logging',
        '--load-and-launch-app=%s' % CAMERA_PATH] + self.__chrome_args
    self.__chrome_process = subprocess.Popen(run_command, shell=False)

    print 'Step 8. Wait for the incoming connection.'
    self.__server.handle_request()

    print 'Step 9. Terminate the process (if not detached earlier).'
    self.__chrome_process.kill()

    print 'Finished gracefully.'
    return self.__server.status


def main():
  """Starts the application."""
  parser = argparse.ArgumentParser(description='Runs a single test.')
  parser.add_argument('--chrome',
                      help='Path for the Chrome binary.',
                      default='google-chrome')
  parser.add_argument('--timeout',
                      help='Timeout in seconds.',
                      type=int,
                      default=30)
  parser.add_argument('test_case',
                      help='Test case name to be run.', nargs=1)
  args, chrome_args = parser.parse_known_args()

  test_runner = TestCaseRunner(args.test_case[0],
                               args.timeout,
                               args.chrome,
                               chrome_args or [])
  sys.exit(test_runner.Run())

if __name__ == '__main__':
  main()

