# -*- python -*-
# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import os
import re
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


# Match FOO=BAR, where BAR is one of:
#   "BAZ" (c-string), where BAZ does not contain "
#   {BAZ} (tuple), where BAZ does not contain { and }
#   [BAZ] (list), where BAZ does not contain [ and ]
# WARNING! returns invalid matches when c-strings contain \"{}[]!
_RESULT_RE = re.compile('\,([^=]+)=("([^"]*)"|{([^{}]*)}|\[([^[\]]*)\])')

def ParseRecord(s, prefix):
  res = {}
  assert s.startswith(prefix)
  try:
    # Oversimplification: match some results, break nesting.
    for m in _RESULT_RE.finditer(s[len(prefix):]):
      # Oversimplification: ignore tuples and lists.
      if m.group(3):
        res[m.group(1)] = m.group(3)
  except:
    # Oversimplification: ignore parsing failures.
    pass
  return res


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

  def _SendRequest(self, request):
    self._log.write('(gdb) ')
    self._log.write(request)
    self._log.write('\n')
    self._gdb.stdin.write(request)
    self._gdb.stdin.write('\n')
    return self._GetResponse()

  def _GetResponse(self):
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

  def _GetResultRecord(self, result):
    for line in result:
      if line.startswith('^'):
        return line

  def _GetLastExecAsyncRecord(self, result):
    for line in reversed(result):
      if line.startswith('*'):
        return line

  def Command(self, command):
    res = self._GetResultRecord(self._SendRequest(command))
    return ParseRecord(res, '^done')

  def ResumeCommand(self, command):
    res = self._GetResultRecord(self._SendRequest(command))
    assert res.startswith('^running')
    res = self._GetLastExecAsyncRecord(self._GetResponse())
    return ParseRecord(res, '*stopped')

  def Quit(self):
    assert self._GetResultRecord(self._SendRequest('-gdb-exit')) == '^exit'

  def Eval(self, expression):
    return self.Command('-data-evaluate-expression ' + expression)['value']

  def Connect(self, program):
    self._GetResponse()
    if os.environ.has_key('NACL_LD_SO'):
      self.Command('file ' + os.environ['NACL_LD_SO'])
      manifest_file = GenerateManifest(program, os.environ['NACL_LD_SO'],
                                       self._name)
      self.Command('nacl-manifest ' + manifest_file)
      self.Command('set breakpoint pending on')
    else:
      self.Command('file ' + program)
    self.Command('target remote :4014')


def RunTest(test_func, test_name, program):
  sel_ldr = LaunchSelLdr(program, test_name)
  try:
    with Gdb(test_name) as gdb:
      gdb.Connect(program)
      test_func(gdb)
  finally:
    sel_ldr.kill()
    sel_ldr.wait()
