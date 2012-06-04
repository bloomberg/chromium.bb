# -*- python -*-
# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import os
import subprocess


def LaunchSelLdr(program, name):
  out_dir = os.environ['OUT_DIR']
  stdout = open(os.path.join(out_dir, name + '.pout'), 'w')
  stderr = open(os.path.join(out_dir, name + '.perr'), 'w')
  sel_ldr = os.environ['NACL_SEL_LDR']
  irt = os.environ['NACL_IRT']
  args = [sel_ldr, '-g', '-B', irt]
  if os.environ.has_key('NACL_ASAN'):
    args += ['-Q']
  if os.environ.has_key('NACL_LD_SO'):
    args += ['-a', '--', os.environ['NACL_LD_SO'],
             '--library-path', os.environ['NACL_LIBS']]
  args += [program]
  return subprocess.Popen(args, stdout=stdout, stderr=stderr)


def GenerateManifest(nexe, runnable_ld, name):
  manifest_dir = os.environ['OUT_DIR']
  runnable_ld_url = {'url': os.path.relpath(runnable_ld, manifest_dir)}
  nexe_url = {'url': os.path.relpath(nexe, manifest_dir)}
  manifest = {
      'program': {
          'x86-32': runnable_ld_url,
          'x86-64': runnable_ld_url,
      },
      'files': {
          'main.nexe': {
              'x86-32': nexe_url,
              'x86-64': nexe_url,
          },
      },
  }
  filename = os.path.join(manifest_dir, name + '.nmf')
  with open(filename, 'w') as manifest_file:
    json.dump(manifest, manifest_file)
  return filename


def RemovePrefix(prefix, string):
    assert string.startswith(prefix)
    return string[len(prefix):]


class Gdb(object):

  def __init__(self, name):
    self._name = name
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
    if os.environ.has_key('NACL_LD_SO'):
      result = self.SendRequest('file ' + os.environ['NACL_LD_SO'])
      assert self.GetResultClass(result) == 'done'
      manifest_file = GenerateManifest(program, os.environ['NACL_LD_SO'],
                                       self._name)
      result = self.SendRequest('nacl-manifest ' + manifest_file)
      assert self.GetResultClass(result) == 'done'
      result = self.SendRequest('set breakpoint pending on')
      assert self.GetResultClass(result) == 'done'
    else:
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

