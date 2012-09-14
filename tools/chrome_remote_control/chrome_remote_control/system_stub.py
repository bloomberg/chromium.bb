# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import os as real_os
import sys as real_sys
import subprocess as real_subprocess

"""Provides stubs for os, sys and subprocess for testing

This test allows one to test code that itself uses os, sys, and subprocess.
"""
class SysModuleStub(object):
  def __init__(self):
    self.platform = ''

class OSPathModuleStub(object):
  def __init__(self, os_module_stub):
    self._os_module_stub = os_module_stub

  def exists(self, path):
    return path in self._os_module_stub.files

  def join(self, *args):
    if self._os_module_stub.sys.platform.startswith('win'):
      tmp = real_os.path.join(*args)
      return tmp.replace('/', '\\')
    else:
      return real_os.path.join(*args)

  def dirname(self, filename):
    return real_os.path.dirname(filename)

class OSModuleStub(object):
  def __init__(self, sys):
    self.sys = sys
    self.path = OSPathModuleStub(self)
    self.files = []
    self.display = ':0'
    self.local_app_data = None

  def getenv(self, name):
    if name == 'DISPLAY':
      return self.display
    if name == 'LOCALAPPDATA':
      return self.local_app_data
    raise Exception('Unsupported getenv')

class PopenStub(object):
  def __init__(self, communicate_result):
    self.communicate_result = communicate_result

  def communicate(self):
    return self.communicate_result

class SubprocessModuleStub(object):
  def __init__(self):
    self.Popen_hook = None
    self.Popen_result = None
    import subprocess as real_subprocess
    self.PIPE = real_subprocess.PIPE

  def Popen(self, *args, **kwargs):
    assert self.Popen_hook or self.Popen_result
    if self.Popen_hook:
      return self.Popen_hook(*args, **kwargs)
    else:
      return self.Popen_result
