# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import serial
import time


class RobotArm():
  """Abstract class for controlling the robot arm that moves the test device."""
  def ResetPosition(self):
    raise NotImplementedError(
        'ResetPosition must be implemented in a subclass.')

  def StartMotophoMovement(self):
    raise NotImplementedError(
        'StartMotophoMovement must be implemented in a subclass.')

  def StopAllMovement(self):
    raise NotImplementedError(
        'StopAllMovement must be implemented in a subclass.')
