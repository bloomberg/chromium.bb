#!/usr/bin/python
# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Class capturing a command invocation as data."""


import multiprocessing
import os
import sys


def PrepareCommandValues(cwd, inputs, outputs):
  values = {}
  values['cwd'] = os.path.abspath(cwd)
  for i in range(len(inputs)):
    values['input%d' % i] = os.path.abspath(inputs[i])
  for i in range(len(outputs)):
    values['output%d' % i] = os.path.abspath(outputs[i])
  return values


class Command(object):
  """An object representing a single command."""

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

  def Invoke(self, check_call, package, inputs, outputs, cwd,
             build_signature=None):
    # TODO(bradnelson): Instead of allowing full subprocess functionality,
    #     move execution here and use polymorphism to implement things like
    #     mkdir, copy directly in python.
    values = PrepareCommandValues(cwd, inputs, outputs)
    kwargs = self._kwargs.copy()
    if 'cwd' in kwargs:
      kwargs['cwd'] = os.path.join(os.path.abspath(values['cwd']),
                                   kwargs['cwd'] % values)
    else:
      kwargs['cwd'] = values['cwd']
    values['cwd'] = kwargs['cwd']
    try:
      values['cores'] = multiprocessing.cpu_count()
    except NotImplementedError:
      values['cores'] = 4  # Assume 4 if we can't measure.
    values['package'] = package
    if build_signature is not None:
      values['build_signature'] = build_signature

    if 'path_dirs' in kwargs:
      path_dirs = [dirname % values for dirname in kwargs['path_dirs']]
      del kwargs['path_dirs']
      env = os.environ.copy()
      env['PATH'] = os.pathsep.join(path_dirs + env['PATH'].split(os.pathsep))
      kwargs['env'] = env

    if isinstance(self._command, str):
      command = self._command % values
    else:
      command = [arg % values for arg in self._command]
    check_call(command, **kwargs)


def Mkdir(path):
  """Convenience method for generating mkdir commands."""
  # TODO(bradnelson): Replace with something less hacky.
  return Command([
      sys.executable, '-c',
      'import sys,os; os.mkdir(sys.argv[1])', path])


def Copy(src, dst):
  """Convenience method for generating cp commands."""
  # TODO(bradnelson): Replace with something less hacky.
  return Command([
      sys.executable, '-c',
      'import sys,shutil; shutil.copyfile(sys.argv[1], sys.argv[2])', src, dst])


def RemoveDirectory(path):
  """Convenience method for generating a command to remove a directory tree."""
  # TODO(mcgrathr): Windows
  return Command(['rm', '-rf', path])
