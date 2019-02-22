# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import robot_arm as ra
import third_party.maestro.maestro as maestro
import serial
import threading
import time

SERVO_PORT = 0
SERVO_ACCELERATION = 12
SERVO_SPEED = 128
# Approximately how long it takes for the servo to move from one endpoint to
# the other given the current acceleration and speed values. A more exact
# number could be calculated, but an approximate number works fine without any
# detrimental effect on test results, and is easier.
MOTOPHO_PERIOD = 0.75
MOTOPHO_ENDPOINTS = {
  'start': 4500,
  'end': 7500,
}


class Command(object):
  RESET = 1
  MOTOPHO = 2

class RobotArmMaestro(ra.RobotArm, threading.Thread):
  """Implementation of RobotArm using the Pololu Maestro series controllers.

  The Pololu Maestro is set up to communicate over a serial connection, similar
  to the Arduino version of RobotArm. However, the servo control code is in
  this file instead of being flashed onto the controller like in the case of
  the Arduino.
  """
  def __init__(self, device_name, num_tries=5):
    if not device_name.startswith('/dev/'):
      device_name = '/dev/' + device_name
    self._controller = None
    connected = False
    for _ in xrange(num_tries):
      try:
        self._controller = maestro.Controller(ttyStr=device_name)
      except serial.SerialException as e:
        continue
      connected = True
      break
    if not connected:
      raise serial.SerialException('Failed to connect to the robot arm.')

    threading.Thread.__init__(self)
    self._current_cycle = 0
    self._command = None
    self._thread_can_die = False
    self._command_setup_required = False

    # Lock for waiting on a new command to be issued
    self._new_command_lock = threading.Event()
    # Lock for waiting on synchronous commands to finish
    self._command_finish_lock = threading.Event()
    self._FinishCommand()
    self.start()

  def ResetPosition(self):
    self._IssueCommand(Command.RESET)
    self._WaitForSynchronousCommandFinish()

  def StartMotophoMovement(self):
    self._IssueCommand(Command.MOTOPHO)

  def StopAllMovement(self):
    self._FinishCommand()

  def Terminate(self):
    self._thread_can_die = True
    self._new_command_lock.set()

  def run(self):
    while True:
      self._WaitForNewCommand()
      if self._thread_can_die:
        # Stop sending signals so the servo isn't locked in place
        self._controller.setTarget(SERVO_PORT, 0)
        self._controller.close()
        break
      # Perform any necessary one-time setup for asynchronous commands
      if self._command_setup_required:
        self._command_setup_required = False
        if self._command is Command.MOTOPHO:
          self._SetupMotopho()
      # Synchronous
      if self._command is Command.RESET:
        self._CommandReset()
      # Asynchronous
      elif self._command is Command.MOTOPHO:
        self._CommandMotopho()

  def _SetupMotopho(self):
    self._current_cycle = 0
    # The Maestro controllers support automatic acceleration when starting and
    # stopping, so tune the acceleration and max speed values to get smooth
    # movement instead of trying to write custom control logic to do that
    # ourselves.
    self._controller.setAccel(SERVO_PORT, SERVO_ACCELERATION)
    self._controller.setSpeed(SERVO_PORT, SERVO_SPEED)

  def _CommandReset(self):
    # The Maestro controllers don't seem to have the same issue as Arduinos
    # where stopping close to the desired position can cause the servo to
    # vibrate in place instead of actually moving to the desired position. So,
    # only send one move command instead of moving away and then to the desired
    # position.
    self._controller.setTarget(SERVO_PORT, MOTOPHO_ENDPOINTS['start'])
    time.sleep(MOTOPHO_PERIOD)
    self._FinishCommand()

  def _CommandMotopho(self):
    target_position = (MOTOPHO_ENDPOINTS['start'] if self._current_cycle % 2
                       else MOTOPHO_ENDPOINTS['end'])
    self._controller.setTarget(SERVO_PORT, target_position)
    self._current_cycle += 1
    time.sleep(MOTOPHO_PERIOD)

  def _WaitForNewCommand(self):
    self._new_command_lock.wait()

  def _WaitForSynchronousCommandFinish(self):
    self._command_finish_lock.wait()

  def _FinishCommand(self):
    self._new_command_lock.clear()
    self._command_finish_lock.set()

  def _IssueCommand(self, command):
    self._command = command
    self._command_setup_required = True
    self._command_finish_lock.clear()
    self._new_command_lock.set()
