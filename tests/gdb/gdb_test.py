# -*- python -*-
# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import subprocess


def LaunchSelLdr(program, name):
  out_dir = os.environ['OUT_DIR']
  stdout = open(os.path.join(out_dir, name + '.pout'), 'w')
  stderr = open(os.path.join(out_dir, name + '.perr'), 'w')
  sel_ldr = os.environ['NACL_SEL_LDR']
  irt = os.environ['NACL_IRT']
  args = [sel_ldr, '-g']
  if os.environ.has_key('NACL_ASAN'):
    args += ['-Q']
  args += ['-B', irt, program]
  return subprocess.Popen(args, stdout=stdout, stderr=stderr)


def RemovePrefix(prefix, string):
    assert string.startswith(prefix)
    return string[len(prefix):]


class Gdb(object):

  def __init__(self, name):
    args = [os.environ['NACL_GDB'], '--interpreter=mi']
    out_dir = os.environ['OUT_DIR']
    stderr = open(os.path.join(out_dir, name + '.err'), 'w')
    self._log = open(os.path.join(out_dir, name + '.log'), 'w')
    self._gdb = subprocess.Popen(args,
                                 stdin=subprocess.PIPE,
                                 stdout=subprocess.PIPE,
                                 stderr=stderr)

  def __enter__(self):
    return self

  def __exit__(self, type, value, traceback):
    self._gdb.kill()
    self._gdb.wait()
    self._log.close()

  def SendRequest(self, request):
    self._log.write(request)
    self._log.write('\n')
    self._gdb.stdin.write(request)
    self._gdb.stdin.write('\n')
    return self.GetResponse()

  def GetResponse(self):
    results = []
    while True:
      line = self._gdb.stdout.readline().rstrip()
      if line == '':
        return results
      self._log.write(line)
      self._log.write('\n')
      if line == '(gdb)':
        return results
      results.append(line)

  def GetResultClass(self, result):
    for line in result:
      if line.startswith('^'):
        return line[1:].split(',', 1)[0]

  def GetAsyncStatus(self, result):
    for line in result:
      if line.startswith('*'):
        return line[1:].split(',', 1)[0]

  def GetExpressionResult(self, expression):
    result = self.SendRequest('-data-evaluate-expression ' + expression)
    assert len(result) == 1, result
    value = RemovePrefix('^done,value="', result[0])
    assert value.endswith('"')
    return value[:-1]

  def Connect(self, program):
    self.GetResponse()
    file_result = self.SendRequest('file ' + program)
    assert self.GetResultClass(file_result) == 'done'
    target_result = self.SendRequest('target remote :4014')
    assert self.GetResultClass(target_result) == 'done'


def RunTest(test_func, test_name, program):
  sel_ldr = LaunchSelLdr(program, test_name)
  try:
    with Gdb(test_name) as gdb:
      gdb.Connect(program)
      test_func(gdb)
  finally:
    sel_ldr.kill()
    sel_ldr.wait()

