# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from common import ParseFlags


# Platform-specific decorators.
# These decorators can be used to only run a test function for certain platforms
# by annotating the function with them.

def AndroidOnly(func):
  def wrapper(*args, **kwargs):
    if ParseFlags().android:
      func(*args, **kwargs)
    else:
      args[0].skipTest('This test runs on Android only.')
  return wrapper

def NotAndroid(func):
  def wrapper(*args, **kwargs):
    if not ParseFlags().android:
      func(*args, **kwargs)
    else:
      args[0].skipTest('This test does not run on Android.')
  return wrapper

def WindowsOnly(func):
  def wrapper(*args, **kwargs):
    if sys.platform == 'win32':
      func(*args, **kwargs)
    else:
      args[0].skipTest('This test runs on Windows only.')
  return wrapper

def NotWindows(func):
  def wrapper(*args, **kwargs):
    if sys.platform != 'win32':
      func(*args, **kwargs)
    else:
      args[0].skipTest('This test does not run on Windows.')
  return wrapper

def LinuxOnly(func):
  def wrapper(*args, **kwargs):
    if sys.platform.startswith('linux'):
      func(*args, **kwargs)
    else:
      args[0].skipTest('This test runs on Linux only.')
  return wrapper

def NotLinux(func):
  def wrapper(*args, **kwargs):
    if sys.platform.startswith('linux'):
      func(*args, **kwargs)
    else:
      args[0].skipTest('This test does not run on Linux.')
  return wrapper

def MacOnly(func):
  def wrapper(*args, **kwargs):
    if sys.platform == 'darwin':
      func(*args, **kwargs)
    else:
      args[0].skipTest('This test runs on Mac OS only.')
  return wrapper

def NotMac(func):
  def wrapper(*args, **kwargs):
    if sys.platform == 'darwin':
      func(*args, **kwargs)
    else:
      args[0].skipTest('This test does not run on Mac OS.')
  return wrapper
