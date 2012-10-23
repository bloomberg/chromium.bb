# -*- python -*-
# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import os
import re
import socket
import subprocess
import sys


def AssertEquals(x, y):
  if x != y:
    raise AssertionError('%r != %r' % (x, y))


def FilenameToUnix(str):
  return str.replace('\\', '/')


def MakeOutFileName(name, ext):
  out_dir = os.environ['OUT_DIR']
  # File name should be consistent with .out file name from nacl.scons
  return os.path.join(out_dir, 'gdb_' + name + ext)


def KillProcess(process):
  try:
    process.kill()
  except OSError:
    # If process is already terminated, kill() throws
    # "WindowsError: [Error 5] Access is denied" on Windows.
    pass
  process.wait()


SEL_LDR_RSP_SOCKET_ADDR = ('localhost', 4014)


def EnsurePortIsAvailable(addr=SEL_LDR_RSP_SOCKET_ADDR):
  # As a sanity check, check that the TCP port is available by binding
  # to it ourselves (and then unbinding).  Otherwise, we could end up
  # talking to an old instance of sel_ldr that is still hanging
  # around, or to some unrelated service that uses the same port
  # number.  Of course, there is still a race condition because an
  # unrelated process could bind the port after we unbind.
  sock = socket.socket()
  sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, True)
  sock.bind(addr)
  sock.close()


def LaunchSelLdr(program, name):
  stdout = open(MakeOutFileName(name, '.pout'), 'w')
  stderr = open(MakeOutFileName(name, '.perr'), 'w')
  sel_ldr = os.environ['NACL_SEL_LDR']
  args = [sel_ldr, '-g']
  if os.environ.has_key('NACL_IRT'):
    args += ['-B', os.environ['NACL_IRT']]
  if os.environ.has_key('NACL_LD_SO'):
    args += ['-a', '--', os.environ['NACL_LD_SO'],
             '--library-path', os.environ['NACL_LIBS']]
  args += [FilenameToUnix(program), name]
  EnsurePortIsAvailable()
  return subprocess.Popen(args, stdout=stdout, stderr=stderr)


def GenerateManifest(nexe, runnable_ld, name):
  manifest_filename = MakeOutFileName(name, '.nmf')
  manifest_dir = os.path.dirname(manifest_filename)
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
  with open(manifest_filename, 'w') as manifest_file:
    json.dump(manifest, manifest_file)
  return manifest_filename


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
    AssertEquals(self.pos, len(self.line))
    return (status_match.group(0), result)


class Gdb(object):

  def __init__(self, name, program):
    self._name = name
    self._program = program
    args = [os.environ['NACL_GDB'], '--interpreter=mi']
    stderr = open(MakeOutFileName(name, '.err'), 'w')
    self._log = open(MakeOutFileName(name, '.log'), 'w')
    self._gdb = subprocess.Popen(args,
                                 stdin=subprocess.PIPE,
                                 stdout=subprocess.PIPE,
                                 stderr=stderr)

  def __enter__(self):
    return self

  def __exit__(self, type, value, traceback):
    KillProcess(self._gdb)
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
    raise AssertionError('No result record found in %r' % result)

  def _GetLastExecAsyncRecord(self, result):
    for line in reversed(result):
      if line.startswith('*'):
        return RecordParser(line).Parse()
    raise AssertionError('No asynchronous execute status record found in %r'
                           % result)

  def Command(self, command):
    status, items = self._GetResultRecord(self._SendRequest(command))
    AssertEquals(status, '^done')
    return items

  def ResumeCommand(self, command):
    status, items = self._GetResultRecord(self._SendRequest(command))
    AssertEquals(status, '^running')
    status, items = self._GetLastExecAsyncRecord(self._GetResponse())
    AssertEquals(status, '*stopped')
    return items

  def Quit(self):
    status, items = self._GetResultRecord(self._SendRequest('-gdb-exit'))
    AssertEquals(status, '^exit')

  def Eval(self, expression):
    return self.Command('-data-evaluate-expression ' + expression)['value']

  def Connect(self):
    self._GetResponse()
    if os.environ.has_key('NACL_IRT'):
      self.Command('nacl-irt ' + FilenameToUnix(os.environ['NACL_IRT']))
    if os.environ.has_key('NACL_LD_SO'):
      # gdb uses bash-like escaping which removes slashes from Windows paths.
      manifest_file = GenerateManifest(self._program,
                                       os.environ['NACL_LD_SO'],
                                       self._name)
      self.Command('nacl-manifest ' + FilenameToUnix(manifest_file))
      self.Command('set breakpoint pending on')
    else:
      self.Command('file ' + FilenameToUnix(self._program))
    self.Command('target remote :4014')


def RunTest(test_func, test_name, program):
  sel_ldr = LaunchSelLdr(program, test_name)
  try:
    with Gdb(test_name, program) as gdb:
      gdb.Connect()
      test_func(gdb)
  finally:
    KillProcess(sel_ldr)
