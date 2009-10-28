#!/usr/bin/python2.6.2
# Copyright 2009, Google Inc.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


"""Utility functions for running the lab tests.

"""

import logging
import os
import subprocess
import shutil
import sys

import runner_constants as const
import util

CHANGE_RESOLUTION_PATH = os.path.join(const.TEST_PATH, 'lab', 
                                      'ChangeResolution','Debug',
                                      'changeresolution.exe')

def EnsureWindowsScreenResolution(width, height, bpp):
  """Performs all steps needed to configure system for testing on Windows.

  Args:
    width: new screen resolution width
    height: new screen resolution height
    bpp: new screen resolution bytes per pixel
  Returns:
    True on success.
  """
  if not os.path.exists(CHANGE_RESOLUTION_PATH):
    return False
  command = 'call "%s" %d %d %d' % (CHANGE_RESOLUTION_PATH, width, height, bpp)
  
  
  our_process = subprocess.Popen(command,
                                 shell=True,
                                 stdout=None,
                                 stderr=None,
                                 universal_newlines=True)

  our_process.wait()

  return our_process.returncode == 0


def AddPythonPath(path):
  """Add path to PYTHONPATH in environment."""
  try:
    os.environ['PYTHONPATH'] = path + os.pathsep + os.environ['PYTHONPATH']
  except KeyError:
    os.environ['PYTHONPATH'] = path

  # Need to put at front of sys.path so python will try to import modules from
  # path before locations further down the sys.path.
  sys.path = [path] + sys.path


def InstallO3DPlugin():
  """Installs O3D plugin."""
  
  logging.info('Installing plugin...')
  if util.IsWindows():
    installer_path = os.path.join(const.PRODUCT_DIR_PATH, 'o3d.msi')
  
    if not os.path.exists(installer_path):
      logging.error('Installer path not found, %s' % installer_path)
      return False
      
    install_command = 'msiexec.exe /i "%s"' % installer_path
    if util.RunStr(install_command) != 0:
      return False

  elif util.IsMac():
    dmg_path = os.path.join(const.PRODUCT_DIR_PATH, 'o3d.dmg')
    mnt = util.MountDiskImage(dmg_path)
    if mnt is None:
      return False
    (device, volumes_path) = mnt
    
    installer_path = os.path.join(volumes_path, 'O3D.mpkg')

    if not os.path.exists(installer_path):
      logging.error('Installer path not found, %s' % installer_path)
      util.UnmountDiskImage(device)
      return False
      
    admin_password = 'g00gl3'
    install_command = ('echo %s | sudo -S /usr/sbin/installer -pkg '
                       '"%s" -target /' % (admin_password, installer_path))
                       
    ret_code = util.RunStr(install_command)
    util.UnmountDiskImage(device)
    if ret_code != 0:
      return False
    
  else:
    plugin_path = os.path.join(const.PRODUCT_DIR_PATH, 'libnpo3dautoplugin.so')
    plugin_dst_dir = os.path.expanduser('~/.mozilla/plugins')
    try:
      os.makedirs(plugin_dst_dir)
    except os.error:
      pass

    plugin_dst = os.path.join(plugin_dst_dir, 'libnpo3dautoplugin.so')
    shutil.copyfile(plugin_path, plugin_dst)
    return True
    
  return True

def UninstallO3DPlugin():
  """Uninstalls O3D.
  
  Returns:
    True, if O3D is no longer installed."""
  
  if util.IsWindows():
    installer_path = os.path.join(const.PRODUCT_DIR_PATH, 'o3d.msi')
    os.system('msiexec.exe /x "%s" /q' % installer_path)
  
  # Forcibly remove plugins.
  for path in const.INSTALL_PATHS:
    if os.path.exists(path):
      if util.IsWindows():
        os.remove(path)
      else:
        os.system('echo g00gl3 | sudo -S rm -rf "%s"' % path)
  
  return not DoesAnO3DPluginExist()

def DoesAnO3DPluginExist():
  for path in const.INSTALL_PATHS:
    if os.path.exists(path):
      return True
  return False
    

  