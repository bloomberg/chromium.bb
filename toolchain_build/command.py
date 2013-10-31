#!/usr/bin/python
# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Class capturing a command invocation as data."""


# Done first to setup python module path.
import toolchain_env

import multiprocessing
import os
import sys

import file_tools


# MSYS tools do not always work with combinations of Windows and MSYS
# path conventions, e.g. '../foo\\bar' doesn't find '../foo/bar'.
# Since we convert all the directory names to MSYS conventions, we
# should not be using Windows separators with those directory names.
# As this is really an implementation detail of this module, we export
# 'command.path' to use in place of 'os.path', rather than having
# users of the module know which flavor to use.
import posixpath
path = posixpath


SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
NACL_DIR = os.path.dirname(SCRIPT_DIR)


def FixPath(path):
  """Convert to msys paths on windows."""
  if sys.platform != 'win32':
    return path
  drive, path = os.path.splitdrive(path)
  # Replace X:\... with /x/....
  # Msys does not like x:\ style paths (especially with mixed slashes).
  if drive:
    drive = '/' + drive.lower()[0]
  path = drive + path
  path = path.replace('\\', '/')
  return path


def PrepareCommandValues(cwd, inputs, output):
  values = {}
  values['cwd'] = FixPath(os.path.abspath(cwd))
  for key, value in inputs.iteritems():
    if key.startswith('abs_'):
      raise Exception('Invalid key starts with "abs_": %s' % key)
    values['abs_' + key] = FixPath(os.path.abspath(value))
    values[key] = FixPath(os.path.relpath(value, cwd))
  values['abs_output'] = FixPath(os.path.abspath(output))
  values['output'] = FixPath(os.path.relpath(output, cwd))
  return values


def PlatformEnvironment(extra_paths):
  """Select the environment variables to run commands with.

  Args:
    extra_paths: Extra paths to add to the PATH variable.
  Returns:
    A dict to be passed as env to subprocess.
  """
  env = os.environ.copy()
  paths = []
  if sys.platform == 'win32':
    if Command.use_cygwin:
      # Use the hermetic cygwin.
      paths = [os.path.join(NACL_DIR, 'cygwin', 'bin')]
    else:
      # TODO(bradnelson): switch to something hermetic.
      mingw = os.environ.get('MINGW', r'c:\mingw')
      msys = os.path.join(mingw, 'msys', '1.0')
      if not os.path.exists(msys):
        msys = os.path.join(mingw, 'msys')
      # We need both msys (posix like build environment) and MinGW (windows
      # build of tools like gcc). We add <MINGW>/msys/[1.0/]bin to the path to
      # get sh.exe. We add <MINGW>/bin to allow direct invocation on MinGW
      # tools. We also add an msys style path (/mingw/bin) to get things like
      # gcc from inside msys.
      paths = [
          '/mingw/bin',
          os.path.join(mingw, 'bin'),
          os.path.join(msys, 'bin'),
      ]
  env['PATH'] = os.pathsep.join(
      paths + extra_paths + env.get('PATH', '').split(os.pathsep))
  return env


class Command(object):
  """An object representing a single command."""
  use_cygwin = False

  def __init__(self, command, **kwargs):
    self._command = command
    self._kwargs = kwargs

  def __str__(self):
    values = []
    # TODO(bradnelson): Do something more reasoned here.
    values += [repr(self._command)]
    for k, v in self._kwargs.iteritems():
      values += [repr(k), repr(v)]
    return '\n'.join(values)

  def Invoke(self, check_call, package, inputs, output, cwd,
             build_signature=None):
    # TODO(bradnelson): Instead of allowing full subprocess functionality,
    #     move execution here and use polymorphism to implement things like
    #     mkdir, copy directly in python.
    kwargs = self._kwargs.copy()
    kwargs['cwd'] = os.path.join(os.path.abspath(cwd), kwargs.get('cwd', '.'))
    values = PrepareCommandValues(kwargs['cwd'], inputs, output)
    try:
      values['cores'] = multiprocessing.cpu_count()
    except NotImplementedError:
      values['cores'] = 4  # Assume 4 if we can't measure.
    values['package'] = package
    if build_signature is not None:
      values['build_signature'] = build_signature
    values['top_srcdir'] = FixPath(os.path.relpath(NACL_DIR, kwargs['cwd']))
    values['abs_top_srcdir'] = FixPath(os.path.abspath(NACL_DIR))

    # Extract paths from kwargs and add to the command environment.
    path_dirs = []
    if 'path_dirs' in kwargs:
      path_dirs = [dirname % values for dirname in kwargs['path_dirs']]
      del kwargs['path_dirs']
    kwargs['env'] = PlatformEnvironment(path_dirs)

    if isinstance(self._command, str):
      command = self._command % values
    else:
      command = [arg % values for arg in self._command]
      paths = kwargs['env']['PATH'].split(os.pathsep)
      command[0] = file_tools.Which(command[0], paths=paths)
    check_call(command, **kwargs)


def Mkdir(path, parents=False, **kwargs):
  """Convenience method for generating mkdir commands."""
  # TODO(bradnelson): Replace with something less hacky.
  func = 'os.mkdir'
  if parents:
    func = 'os.makedirs'
  return Command([
      sys.executable, '-c',
      'import sys,os; ' + func + '(sys.argv[1])', path],
      **kwargs)


def Copy(src, dst, **kwargs):
  """Convenience method for generating cp commands."""
  # TODO(bradnelson): Replace with something less hacky.
  return Command([
      sys.executable, '-c',
      'import sys,shutil; shutil.copyfile(sys.argv[1], sys.argv[2])', src, dst],
      **kwargs)


def RemoveDirectory(path):
  """Convenience method for generating a command to remove a directory tree."""
  # TODO(mcgrathr): Windows
  return Command(['rm', '-rf', path])


def Remove(path):
  """Convenience method for generating a command to remove a file."""
  # TODO(mcgrathr): Replace with something less hacky.
  return Command([
      sys.executable, '-c',
      'import sys, os\n'
      'if os.path.exists(sys.argv[1]): os.remove(sys.argv[1])', path
      ])


def Rename(src, dst):
  """Convenience method for generating a command to rename a file."""
  # TODO(mcgrathr): Replace with something less hacky.
  return Command([
      sys.executable, '-c',
      'import sys, os; os.rename(sys.argv[1], sys.argv[2])', src, dst
      ])


def WriteData(data, dst):
  """Convenience method to write a file with fixed contents."""
  # TODO(mcgrathr): Replace with something less hacky.
  return Command([
      sys.executable, '-c',
      'import sys; open(sys.argv[1], "wb").write(%r)' % data, dst
      ])
