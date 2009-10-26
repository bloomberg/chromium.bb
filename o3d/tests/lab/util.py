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


"""Common utilities for O3D's lab test runner.

"""

import logging
import logging.handlers
import os
import platform
import subprocess
import sys
import socket
import time
import urllib
import urlparse


# Logging utilities.
def ConfigureLogging(logging_file_path=None):
  logging_format = '%(levelname)8s -- %(message)s'
  
  root_logger = logging.getLogger('')
  root_logger.setLevel(logging.NOTSET)
  
  # Log to file.
  if logging_file_path is not None:
    file_handler = logging.handlers.RotatingFileHandler(logging_file_path,
                                                        maxBytes=5000000)
    formatter = logging.Formatter(logging_format)
    file_handler.setFormatter(formatter)
    file_handler.setLevel(logging.NOTSET)
    root_logger.addHandler(file_handler)
    
    if not os.path.exists(logging_file_path):
      print 'Failed to create log.  Can\'t continue:"%s"' % (logging_file_path)
      sys.exit(1)
      
    logging.info('Appending log output to file:"%s"', logging_file_path)
    
  # Log to stdout.
  stdout_handler = logging.StreamHandler(sys.stdout)
  formatter = logging.Formatter(logging_format)
  stdout_handler.setFormatter(formatter)
  stdout_handler.setLevel(logging.NOTSET)
  root_logger.addHandler(stdout_handler)


    
    
# OS Detection utilities.
def IsMac():
  return platform.system() == 'Darwin'

def IsWindows():
  return platform.system() == 'Windows'

def IsLinux():
  return platform.system() == 'Linux'

def IsWin7():
  return IsWindows() and platform.release() == 'post2008Server'

def IsWin7_64():
  return IsWin7() and os.path.exists(r'C:\Program Files (x86)') 

def IsVista():
  return IsWindows() and platform.release() == 'Vista'

def IsVista_64():
  return IsVista() and os.path.exists(r'C:\Program Files (x86)')

def IsXP():
  return (IsWindows() and platform.release() == 'XP') or IsXP_64()

def IsXP_64():
  return (IsWindows() and platform.release() == '2003Server' and 
          os.path.exists(r'C:\Program Files (x86)'))

def GetOSName():
  return platform.platform()

def GetOSPrefix():
  if IsWin7_64():
    prefix = 'win7_64'
  elif IsWin7():
    prefix = 'win7'
  elif IsVista_64():
    prefix = 'vista_64' 
  elif IsVista():
    prefix = 'vista'
  elif IsXP_64():
    prefix = 'xp_64'
  elif IsXP():
    prefix = 'xp'
  elif IsMac():
    prefix = 'mac'
  elif IsLinux():
    prefix = 'linux'
  else:
    prefix = None
  return prefix


NONE = 0
WARN = 1
ERROR = 2
FATAL = 3
def RunCommand(cmd_string, level=ERROR, info=None):
  # Execute command.
  if info == None:
    info = cmd_string
  if level != NONE:
    logging.info(info + '...')
    
    
  our_process = subprocess.Popen(cmd_string,
                                 shell=True,
                                 stdout=subprocess.PIPE,
                                 stderr=subprocess.STDOUT,
                                 universal_newlines=True)

  # Read output so that process isn't blocked with a filled buffer.
  (output,err_output) = our_process.communicate()

  return_code = our_process.returncode

  
  if level != NONE:
    if return_code:
      # Command failed. Take appropriate action.
      if level == WARN:
        logging.warn('Failed: ' + info)
      elif level == ERROR or level == FATAL:
        logging.error('Failed: ' + info)
      
      logging.info('  Details for ' + cmd_string + ':')
      logging.info('  Failed with return code:%d', return_code)
      for line in output.split('\n'):
        logging.info('     ' + line)
        
      if level == FATAL:
        sys.exit(1)
        
    # Command succeeded. Report success.
    else:
      logging.info('Success.')
    
  return (return_code, output)


def RunStr(cmd_string, level=ERROR, info=None):
  return RunCommand(cmd_string, level, info)[0]
    

def RunWithOutput(command, level=ERROR, info=None):
  return RunCommand(' '.join(command), level, info)


def Run(command, level=ERROR, info=None):
  return RunWithOutput(command, level, info)[0]


REG_DWORD = 'REG_DWORD'
REG_SZ = 'REG_SZ'
REG_BINARY = 'REG_BINARY'
def RegAdd(path, key, type, value, level=ERROR, info=None):
  
  cmd = ['REG', 'ADD', '"' + path + '"', '/V', key,
         '/t', type, '/d', '"' + value + '"', '/F']
  
  return Run(cmd, level, info) == 0


def RegDoesExist(path, key, level=NONE):
  cmd = ['REG', 'QUERY', '"' + path + '"', '/V', key]
  
  return Run(cmd, level) == 0


def RegDel(path, key, level=ERROR):
  cmd = ['REG', 'DELETE', '"' + path + '"', '/V', key, '/F']
  
  return Run(cmd, level) == 0


def RegMaybeDel(path, key, level):
  if RegQuery(path, key):
    return RegDel(path, key, level)
    
  return True


def RegQuery(path, key):
  """Read value from registry.

  Args:
    path: full path into registry for parent of value to read.
    key: key under parent to read.

  Returns:
    Value on success and None on failure.
  """
  arguments = ['REG', 'QUERY', path,
               '/V', key]
  reg_process = subprocess.Popen(args=arguments,
                                 stdout=subprocess.PIPE,
                                 stderr=subprocess.STDOUT,
                                 universal_newlines=True)

  value = None

  lines = reg_process.stdout.readlines()
  for line in lines:
    line = line.strip('\n')
    check_key = line.lstrip()
    if check_key.startswith(key):
      type_and_value = check_key[len(key):].lstrip()
      for i in range(len(type_and_value)):
        if type_and_value[i] == '\t' or type_and_value[i] == ' ':
          value = type_and_value[i:].lstrip()
          break
    
  reg_process.wait()
  if reg_process.returncode:
    return None
  return value


# Other utilities. 
def IpAddress():
  """Get IP address of this machine."""
  address_info = socket.gethostbyname_ex(socket.gethostname())
  if len(address_info) == 3:
    ip = address_info[2]
    if ip:
      ip = ip[0]
      return ip

  logging.error('Failed to get local machine IP.')
  return None


def Reboot():
  """Reboots machine."""
  if IsWindows():
    cmd = 'shutdown -r -t 1 -f'
  elif IsMac():
    cmd = 'shutdown -r now'
  
  logging.info('Rebooting machine...')
  os.system(cmd)
  time.sleep(10)
  logging.error('Failed: Could not reboot')

    
def Download(url, prefix_dir):
  """Downloads single file at |url| to |prefix_dir|.
  
  Returns:
    local_path: the path to the downloaded file, or None if download was
      unsuccessful."""
  
  parsed_url = urlparse.urlparse(url)
  path = parsed_url[2]
  name = path.rsplit('/', 1)[1]
  local_path = os.path.join(prefix_dir, name)

  if not os.path.exists(prefix_dir):
    os.mkdir(prefix_dir)
  urllib.urlretrieve(url, local_path)
  
  if not os.path.exists(local_path):
    logging.error('Could not download %s to %s' % (url, local_path))
    return None  

  return local_path


def AddToStartup(name, command):
  if IsWindows():
    path = r'HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\Run'
    RegAdd(path, name, REG_SZ, command)

    
def RemoveFromStartup(name):
  if IsWindows():
    path = r'HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\Run'
    RegMaybeDel(path, name, ERROR)
    

def MountDiskImage(dmg_path):
  """Mounts disk image.

  Args:
    dmg_path: path to image that will be mounted.
    
  Returns:
    Tuple contaiing device path and mounted path on success, 
    or None on failure.
  """
  mount_path = None
  logging.info('Mounting %s...' % dmg_path)

  cmd = ['hdiutil', 'attach', '"' + dmg_path + '"']
  (return_code, output) = RunWithOutput(cmd)

  if return_code == 0 and output:
    # Attempt to grab the mounted path from the command output.
    # This should be safe regardless of actual output.
    new_device = output.strip().split('\n')[-1].split('\t')
    device = new_device[0].strip()
    mount_path = new_device[-1]
    
    logging.info('Device %s mounted at %s' % (device,mount_path))

    # Wait for mounting operation to complete.
    time.sleep(10)
    if os.path.exists(mount_path):
      logging.info('Mount point is accessible.')
    else:
      mount_path = None
          
  if mount_path is None or device is None:
    logging.error('Could not mount properly.')
    return None
    
  return (device, mount_path)


def UnmountDiskImage(device):
  """Unmounts disk image.

  Args:
    device: path to device to be detached

  Returns:
    True on success.
  """
  logging.info('Unmounting device %s...' % device)

  return Run(['hdiutil detach', '"' + device + '"', '-force']) == 0
