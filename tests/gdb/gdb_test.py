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


class RecordParser(object):

  STATUS_RE = re.compile('[^,]+')
  KEY_RE = re.compile('([^"{\[=]+)=')
  VALUE_PREFIX_RE = re.compile('"|{|\[')
  STRING_VALUE_RE = re.compile('([^"]*)"')

  def __init__(self, line):
    self.line = line
    self.pos = 0

  def Skip(self, c):
    if self.line.startswith(c, self.pos):
      self.pos += len(c)
      return True
    return False

  def Match(self, r):
    match = r.match(self.line, self.pos)
    if match is not None:
      self.pos = match.end()
    return match

  def ParseString(self):
    string_value_match = self.Match(self.STRING_VALUE_RE)
    assert string_value_match is not None
    return string_value_match.group(1)

  def ParseValue(self):
    value_prefix_match = self.Match(self.VALUE_PREFIX_RE)
    assert value_prefix_match is not None
    if value_prefix_match.group(0) == '"':
      return self.ParseString()
    elif value_prefix_match.group(0) == '{':
      return self.ParseDict()
    else:
      return self.ParseList()

  def ParseListMembers(self):
    result = []
    while True:
      # List syntax:
      #   [foo, bar]
      #   [foo=x, bar=y] - we parse this as [{foo=x}, {bar=y}]
      key_match = self.Match(self.KEY_RE)
      value = self.ParseValue()
      if key_match is not None:
        result.append({key_match.group(1): value})
      else:
        result.append(value)
      if not self.Skip(','):
        break
    return result

  def ParseList(self):
    if self.Skip(']'):
      return []
    result = self.ParseListMembers()
    assert self.Skip(']')
    return result

  def ParseDictMembers(self):
    result = {}
    while True:
      key_match = self.Match(self.KEY_RE)
      assert key_match is not None
      result[key_match.group(1)] = self.ParseValue()
      if not self.Skip(','):
        break
    return result

  def ParseDict(self):
    if self.Skip('}'):
      return {}
    result = self.ParseDictMembers()
    assert self.Skip('}')
    return result

  def Parse(self):
    status_match = self.Match(self.STATUS_RE)
    assert status_match is not None
    result = {}
    if self.Skip(','):
      result = self.ParseDictMembers()
    assert self.pos == len(self.line)
    return (status_match.group(0), result)


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
        return RecordParser(line).Parse()

  def _GetLastExecAsyncRecord(self, result):
    for line in reversed(result):
      if line.startswith('*'):
        return RecordParser(line).Parse()

  def Command(self, command):
    status, items = self._GetResultRecord(self._SendRequest(command))
    assert status == '^done', status
    return items

  def ResumeCommand(self, command):
    status, items = self._GetResultRecord(self._SendRequest(command))
    assert status == '^running', status
    status, items = self._GetLastExecAsyncRecord(self._GetResponse())
    assert status == '*stopped', status
    return items

  def Quit(self):
    status, items = self._GetResultRecord(self._SendRequest('-gdb-exit'))
    assert status == '^exit', status

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
