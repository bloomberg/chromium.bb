#!/usr/bin/python
# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Class capturing a command invocation as data."""


import multiprocessing
import os


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
    values['cores'] = multiprocessing.cpu_count()
    values['package'] = package
    if build_signature is not None:
      values['build_signature'] = build_signature

    if isinstance(self._command, str):
      command = self._command % values
    else:
      command = [arg % values for arg in self._command]
    check_call(command, **kwargs)


def Mkdir(path):
  """Convenience method for generating mkdir commands."""
  return Command('mkdir "%s"' % path, shell=True)


def Copy(src, dst):
  """Convenience method for generating cp commands."""
  return Command('cp "%s" "%s"' % (src, dst), shell=True)
