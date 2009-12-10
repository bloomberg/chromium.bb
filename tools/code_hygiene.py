#!/usr/bin/python
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


"""Simple Code Hygiene tool meant to be run as a presubmit test or standalone

Usage:
./code_hygiene.py <file1> <file2> ...

For the styleguides see:
http://code.google.com/p/soc/wiki/PythonStyleGuide
http://google-styleguide.googlecode.com/svn/trunk/cppguide.xml
"""

import os
import re
import signal
import subprocess
import sys
import time

# ======================================================================
VERBOSE = 0
LIMIT = 5
TIMEOUT = 10

# These program are likely not available/working on windows

# http://tidy.sourceforge.net/
HTML_CHECKER = ['tidy', '-errors']

# From depot_tools
# to see a list of all filters run: 'depot_tools/cpplint.py --filter='
CPP_CHECKER = ['cpplint.py', '--filter=-build/header_guard']

# From depot_tools (currently not uses -- too many false positives
# to see a list of all filters run: 'depot_tools/cpplint.py --filter='
C_CHECKER = ['cpplint.py', '--filter=-build/header_guard']

# http://pychecker.sourceforge.net/
PYTHON_CHECKER = ['pychecker']

RE_IGNORE = re.compile(r'@IGNORE_LINES_FOR_CODE_HYGIENE\[([0-9]+)\]')

# ======================================================================
def Debug(s):
  if VERBOSE:
    print s


def RunCommand(cmd):
  """Run a shell command given as an argv style vector."""
  Debug(str(cmd))

  start = time.time()
  p = subprocess.Popen(cmd,
                       bufsize=1000*1000,
                       stderr=subprocess.PIPE,
                       stdout=subprocess.PIPE)
  while p.poll() is None:
    time.sleep(1)
    now = time.time()
    if now - start > TIMEOUT:
      Debug('Error: timeout')
      os.kill(p.pid, signal.SIGKILL)
      os.waitpid(-1, os.WNOHANG)
      return -666, "", ""
  stdout = p.stdout.read()
  stderr = p.stderr.read()
  retcode = p.wait()

  return retcode, stdout, stderr


# ======================================================================
class ExternalChecker(object):
  """Base Class"""
  def __init__(self, commandline, use_stderr=False):
    self._commandline = commandline
    self._use_stderr = use_stderr

  def FindProblems(self, filename, unused_data):
    try:
      retcode, stdout, stderr = RunCommand(self._commandline + [filename])
    except Exception, err:
      print 'cannot excuted of command %s failed reason: %s ' % (
          str(self._commandline), str(err))
      return []
    if retcode == 0:
      return []
    if retcode == 666:
      print 'command %s timed out' % str(self._commandline)
      return []
    if self._use_stderr:
      return [stderr]
    else:
      return [stdout]

  def FileFilter(self, unused_filename):
    raise NotImplementedError()


class TidyChecker(ExternalChecker):
  """Invoke tidy tool on html files."""
  def __init__(self):
    ExternalChecker.__init__(self, HTML_CHECKER, use_stderr=True)
    return

  def FileFilter(self, filename):
    return filename.endswith('.html')


class PyChecker(ExternalChecker):
  """Invoke pychecker tool on python files."""
  def __init__(self):
    ExternalChecker.__init__(self, PYTHON_CHECKER)
    return

  def FileFilter(self, filename):
    return filename.endswith('.py')


class CppLintChecker(ExternalChecker):
  """Invoke Google's cpplint tool on c++ files."""
  def __init__(self):
    ExternalChecker.__init__(self, CPP_CHECKER, use_stderr=True)
    return

  def FileFilter(self, filename):
    return filename.endswith('.cc') or filename.endswith('.h')

# ======================================================================
class GenericRegexChecker(object):
  """Base Class"""
  def __init__(self, content_regex, line_regex=None, analyze_match=False):
    self._content_regex = re.compile(content_regex, re.MULTILINE)
    if line_regex:
      self._line_regex = re.compile(line_regex)
    else:
      self._line_regex = re.compile(content_regex)
    self._analyze_match = analyze_match
    return

  def FindProblems(self, unused_filename, data):
    # speedy early out
    if not self._content_regex.search(data):
      return []

    lines = data.split('\n')
    problem = []
    ignore_line_count = 0
    for no, line in enumerate(lines):
      if ignore_line_count > 0:
        ignore_line_count -= 1
        continue
      match = RE_IGNORE.search(line)
      if match:
        ignore_line_count = int(match.group(1))
        continue
      match = self._line_regex.search(line)
      if not match:
        continue
      if self._analyze_match:
        if not self.IsProblemMatch(match):
          continue
      problem.append(self._RenderItem(no, line))
    return problem

  def _RenderItem(self, no, line):
      return 'line %d: [%s]' % (no, repr(line))

  def FileFilter(self, unused_filename):
    raise NotImplementedError()


class OpenCurlyChecker(GenericRegexChecker):
  """Checks for orphan opening curly braces"""

  def __init__(self):
    GenericRegexChecker.__init__(self, r'^ *{ *$')
    return

  def FileFilter(self, filename):
    return (filename.endswith('.h') or
            filename.endswith('.cc') or
            filename.endswith('.c'))


class TrailingWhiteSpaceChecker(GenericRegexChecker):

  def __init__(self):
    GenericRegexChecker.__init__(self, r' $')
    return

  def FileFilter(self, filename):
    return (not filename.endswith('.patch'))


class CarriageReturnChecker(GenericRegexChecker):

  def __init__(self):
    GenericRegexChecker.__init__(self, r'\r')
    return

  def FileFilter(self, filename):
    return (filename.endswith('.py') or
            filename.endswith('.h') or
            filename.endswith('.cc') or
            filename.endswith('.c'))


class CppCommentChecker(GenericRegexChecker):

  def __init__(self):
    # we tolerate http:// etc in comments
    GenericRegexChecker.__init__(self, r'(^|[^:])//')
    return

  def FileFilter(self, filename):
    return filename.endswith('.c')


class TabsChecker(GenericRegexChecker):

  def __init__(self):
    GenericRegexChecker.__init__(self, r'\t')
    return

  def FileFilter(self, filename):
    return ("akefile" not in filename
        and not filename.endswith('.patch')
        and not filename.endswith('.valerr')
        and not filename.endswith('.dis'))


class FixmeChecker(GenericRegexChecker):

  def __init__(self):
    GenericRegexChecker.__init__(self, r'FIXME[:]')
    return

  def FileFilter(self, filename):
    return 1


class ExternChecker(GenericRegexChecker):

  def __init__(self):
    GenericRegexChecker.__init__(self, r'^ *extern\s+(.*)$', analyze_match=True)
    return

  def IsProblemMatch(self, match):
    extra = match.group(1).strip()
    # allow 'extern "C" {'
    # but do not allow plain 'extern "C"'
    if extra == '"C" {':
      return False
    return True

  def FileFilter(self, filename):
    return filename.endswith('.c') or filename.endswith('.cc')


class RewriteChecker(GenericRegexChecker):

  def __init__(self):
    GenericRegexChecker.__init__(self, r'[@][@]REWRITE')
    return

  def FileFilter(self, unused_filename):
    return 1


VALID_INCLUDE_PREFIX = [
    'native_client/',
    'third_party/',
    'gtest/',
    'gen/native_client/',
    ]


class IncludeChecker(object):
  """Enforce some base 'include' rules. Cpplint does more of these."""
  def __init__(self):
    pass

  def FindProblems(self, unused_filename, data):
    lines = data.split('\n')
    problem = []
    seen_code = False
    for no, line in enumerate(lines):
      if line.startswith('}'):
        # this test is our hacked indicator signaling that no includes
        # directives should follow after we have seen actual code
        seen_code = True
        continue

      if not line.startswith('#include'):
        continue
      if seen_code:
        problem.append('line %d: [%s]' % (no, repr(line)))
      token = line.split()
      path = token[1][1:]
      if '..' in path:
        problem.append('line %d: [%s]' % (no, repr(line)))
      if not token[1].startswith('<'):
        for prefix in VALID_INCLUDE_PREFIX:
          if path.startswith(prefix):
            break
        else:
          # no prefix matched
          problem.append('line %d: [%s]' % (no, repr(line)))

    return problem

  def FileFilter(self, filename):
    return filename.endswith('.c') or filename.endswith('.cc')


class LineLengthChecker(object):
  def __init__(self):
    pass

  def FindProblems(self, filename, data):
    # Exempt golden files used in tests from line-length limits.
    if filename.endswith('.val') or filename.endswith('.valerr'):
      return

    lines = data.split('\n')
    problem = []
    for no, line in enumerate(lines):
      if len(line) <= 80:
        continue
      line = line.strip()
      # we allow comments and as it so happens pre processor directives
      # to be longer
      if line.startswith('//') or line.startswith('/*') or line.startswith('#'):
        continue

      problem.append('line %d: [%s]' % (no + 1, repr(line)))
    return problem

  def FileFilter(self, filename):
    return 1


# ======================================================================
def IsBinary(data):
  return '\x00' in data

def FindExemptions(data):
  match = re.search(r'@EXEMPTION\[([^\]]*)\]', data)
  if match:
    return match.group(1).split(',')
  return []


# ======================================================================
CHECKS = [# fatal checks
          (1, 'tabs', TabsChecker()),
          (1, 'trailing_space', TrailingWhiteSpaceChecker()),
          (1, 'cpp_comment', CppCommentChecker()),
          (1, 'fixme', FixmeChecker()),
          (1, 'include', IncludeChecker()),
          (1, 'extern', ExternChecker()),
          # Non fatal checks
          (0, 'open_curly', OpenCurlyChecker()),
          (0, 'line_length', LineLengthChecker()),
          (0, 'carriage_return', CarriageReturnChecker()),
          (0, 'rewrite', RewriteChecker()),
          (0, 'tidy', TidyChecker()),
          (0, 'pychecker', PyChecker()),
          (0, 'cpplint', CppLintChecker()),
          ]


def EmitStatus(filename, status, details=[]):
  print '%s: %s' % (filename, status)
  for no, line in details[:LIMIT]:
    print '  line %d: [%s]' % (no, repr(line))
  return


def CheckFile(filename, report):
    errors = {}
    warnings = {}

    data = open(filename).read()

    if IsBinary(data):
      EmitStatus(filename, 'binary')
      return errors, warnings

    exemptions = FindExemptions(data)

    for fatal, name, check in CHECKS:
      Debug('trying %s' % name)

      if not check.FileFilter(filename):
        Debug('skipping')
        continue

      if name in exemptions:
        Debug('exempt')
        continue
      problems = check.FindProblems(filename, data)
      if problems:
        info =  '%s: %s' % (filename, name)
        items = problems[:LIMIT]
        if report:
          print info
          for i in items:
            print i
        if fatal:
          errors[info] = items
          break
        else:
          warnings[info] = items
    return errors, warnings


def HasGypFileCorrespondingToSconsFile(sconsfile, list):
  scons_root = os.path.dirname(sconsfile)
  for name in list:
    if name.startswith(scons_root) and name.endswith('.gyp'):
      return True
  return False


def main(argv):
  num_error = 0
  for filename in argv:
    if os.path.isdir(filename):
      EmitStatus(filename, 'directory')
      continue

    if '-' in os.path.basename(filename):
      print 'hyphen in:', filename
      num_error += 1
      continue

    # Verify that gyp files are updated together with scons files.
    # We probably won't have any gyp files for the untrusted code.
    if filename.endswith('.scons') and -1 == filename.find('untrusted'):
      if not HasGypFileCorrespondingToSconsFile(filename, argv):
        print 'please update the gyp file relevant for ', filename
        num_error += 1

    errors, warnings = CheckFile(filename, True)
    if errors:
      num_error += 1
  return num_error

if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
