#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Enables directory-specific presubmit checks to run at upload and/or commit.
"""

from __future__ import print_function

__version__ = '1.8.0'

# TODO(joi) Add caching where appropriate/needed. The API is designed to allow
# caching (between all different invocations of presubmit scripts for a given
# change). We should add it as our presubmit scripts start feeling slow.

import argparse
import ast  # Exposed through the API.
import contextlib
import cpplint
import fnmatch  # Exposed through the API.
import glob
import inspect
import itertools
import json  # Exposed through the API.
import logging
import multiprocessing
import os  # Somewhat exposed through the API.
import random
import re  # Exposed through the API.
import signal
import sys  # Parts exposed through API.
import tempfile  # Exposed through the API.
import threading
import time
import traceback
import unittest  # Exposed through the API.
from warnings import warn

# Local imports.
import fix_encoding
import gclient_paths  # Exposed through the API
import gclient_utils
import git_footers
import gerrit_util
import owners
import owners_finder
import presubmit_canned_checks
import scm
import subprocess2 as subprocess  # Exposed through the API.

if sys.version_info.major == 2:
  # TODO(1009814): Expose urllib2 only through urllib_request and urllib_error
  import urllib2  # Exposed through the API.
  import urlparse
  import urllib2 as urllib_request
  import urllib2 as urllib_error
else:
  import urllib.parse as urlparse
  import urllib.request as urllib_request
  import urllib.error as urllib_error

# Ask for feedback only once in program lifetime.
_ASKED_FOR_FEEDBACK = False


def time_time():
  # Use this so that it can be mocked in tests without interfering with python
  # system machinery.
  return time.time()


class PresubmitFailure(Exception):
  pass


class CommandData(object):
  def __init__(self, name, cmd, kwargs, message, python3=False):
    self.name = name
    self.cmd = cmd
    self.stdin = kwargs.get('stdin', None)
    self.kwargs = kwargs.copy()
    self.kwargs['stdout'] = subprocess.PIPE
    self.kwargs['stderr'] = subprocess.STDOUT
    self.kwargs['stdin'] = subprocess.PIPE
    self.message = message
    self.info = None
    self.python3 = python3


# Adapted from
# https://github.com/google/gtest-parallel/blob/master/gtest_parallel.py#L37
#
# An object that catches SIGINT sent to the Python process and notices
# if processes passed to wait() die by SIGINT (we need to look for
# both of those cases, because pressing Ctrl+C can result in either
# the main process or one of the subprocesses getting the signal).
#
# Before a SIGINT is seen, wait(p) will simply call p.wait() and
# return the result. Once a SIGINT has been seen (in the main process
# or a subprocess, including the one the current call is waiting for),
# wait(p) will call p.terminate().
class SigintHandler(object):
  sigint_returncodes = {-signal.SIGINT,  # Unix
                        -1073741510,     # Windows
                        }
  def __init__(self):
    self.__lock = threading.Lock()
    self.__processes = set()
    self.__got_sigint = False
    self.__previous_signal = signal.signal(signal.SIGINT, self.interrupt)

  def __on_sigint(self):
    self.__got_sigint = True
    while self.__processes:
      try:
        self.__processes.pop().terminate()
      except OSError:
        pass

  def interrupt(self, signal_num, frame):
    with self.__lock:
      self.__on_sigint()
    self.__previous_signal(signal_num, frame)

  def got_sigint(self):
    with self.__lock:
      return self.__got_sigint

  def wait(self, p, stdin):
    with self.__lock:
      if self.__got_sigint:
        p.terminate()
      self.__processes.add(p)
    stdout, stderr = p.communicate(stdin)
    code = p.returncode
    with self.__lock:
      self.__processes.discard(p)
      if code in self.sigint_returncodes:
        self.__on_sigint()
    return stdout, stderr

sigint_handler = SigintHandler()


class Timer(object):
  def __init__(self, timeout, fn):
    self.completed = False
    self._fn = fn
    self._timer = threading.Timer(timeout, self._onTimer) if timeout else None

  def __enter__(self):
    if self._timer:
      self._timer.start()
    return self

  def __exit__(self, _type, _value, _traceback):
    if self._timer:
      self._timer.cancel()

  def _onTimer(self):
    self._fn()
    self.completed = True


class ThreadPool(object):
  def __init__(self, pool_size=None, timeout=None):
    self.timeout = timeout
    self._pool_size = pool_size or multiprocessing.cpu_count()
    self._messages = []
    self._messages_lock = threading.Lock()
    self._tests = []
    self._tests_lock = threading.Lock()
    self._nonparallel_tests = []

  def _GetCommand(self, test):
    vpython = 'vpython'
    if test.python3:
      vpython += '3'
    if sys.platform == 'win32':
      vpython += '.bat'

    cmd = test.cmd
    if cmd[0] == 'python':
      cmd = list(cmd)
      cmd[0] = vpython
    elif cmd[0].endswith('.py'):
      cmd = [vpython] + cmd

    # On Windows, scripts on the current directory take precedence over PATH, so
    # that when testing depot_tools on Windows, calling `vpython.bat` will
    # execute the copy of vpython of the depot_tools under test instead of the
    # one in the bot.
    # As a workaround, we run the tests from the parent directory instead.
    if (cmd[0] == vpython and
        'cwd' in test.kwargs and
        os.path.basename(test.kwargs['cwd']) == 'depot_tools'):
      test.kwargs['cwd'] = os.path.dirname(test.kwargs['cwd'])
      cmd[1] = os.path.join('depot_tools', cmd[1])

    return cmd

  def _RunWithTimeout(self, cmd, stdin, kwargs):
    p = subprocess.Popen(cmd, **kwargs)
    with Timer(self.timeout, p.terminate) as timer:
      stdout, _ = sigint_handler.wait(p, stdin)
      if timer.completed:
        stdout = 'Process timed out after %ss\n%s' % (self.timeout, stdout)
      return p.returncode, stdout

  def CallCommand(self, test):
    """Runs an external program.

    This function converts invocation of .py files and invocations of 'python'
    to vpython invocations.
    """
    cmd = self._GetCommand(test)
    try:
      start = time_time()
      returncode, stdout = self._RunWithTimeout(cmd, test.stdin, test.kwargs)
      duration = time_time() - start
    except Exception:
      duration = time_time() - start
      return test.message(
          '%s\n%s exec failure (%4.2fs)\n%s' % (
              test.name, ' '.join(cmd), duration, traceback.format_exc()))

    if returncode != 0:
      return test.message(
          '%s\n%s (%4.2fs) failed\n%s' % (
              test.name, ' '.join(cmd), duration, stdout))

    if test.info:
      return test.info('%s\n%s (%4.2fs)' % (test.name, ' '.join(cmd), duration))

  def AddTests(self, tests, parallel=True):
    if parallel:
      self._tests.extend(tests)
    else:
      self._nonparallel_tests.extend(tests)

  def RunAsync(self):
    self._messages = []

    def _WorkerFn():
      while True:
        test = None
        with self._tests_lock:
          if not self._tests:
            break
          test = self._tests.pop()
        result = self.CallCommand(test)
        if result:
          with self._messages_lock:
            self._messages.append(result)

    def _StartDaemon():
      t = threading.Thread(target=_WorkerFn)
      t.daemon = True
      t.start()
      return t

    while self._nonparallel_tests:
      test = self._nonparallel_tests.pop()
      result = self.CallCommand(test)
      if result:
        self._messages.append(result)

    if self._tests:
      threads = [_StartDaemon() for _ in range(self._pool_size)]
      for worker in threads:
        worker.join()

    return self._messages


def normpath(path):
  '''Version of os.path.normpath that also changes backward slashes to
  forward slashes when not running on Windows.
  '''
  # This is safe to always do because the Windows version of os.path.normpath
  # will replace forward slashes with backward slashes.
  path = path.replace(os.sep, '/')
  return os.path.normpath(path)


def _RightHandSideLinesImpl(affected_files):
  """Implements RightHandSideLines for InputApi and GclChange."""
  for af in affected_files:
    lines = af.ChangedContents()
    for line in lines:
      yield (af, line[0], line[1])


def prompt_should_continue(prompt_string):
  sys.stdout.write(prompt_string)
  response = sys.stdin.readline().strip().lower()
  return response in ('y', 'yes')


# Top level object so multiprocessing can pickle
# Public access through OutputApi object.
class _PresubmitResult(object):
  """Base class for result objects."""
  fatal = False
  should_prompt = False

  def __init__(self, message, items=None, long_text=''):
    """
    message: A short one-line message to indicate errors.
    items: A list of short strings to indicate where errors occurred.
    long_text: multi-line text output, e.g. from another tool
    """
    self._message = message
    self._items = items or []
    self._long_text = long_text.rstrip()

  def handle(self):
    sys.stdout.write(self._message)
    sys.stdout.write('\n')
    for index, item in enumerate(self._items):
      sys.stdout.write('  ')
      # Write separately in case it's unicode.
      sys.stdout.write(str(item))
      if index < len(self._items) - 1:
        sys.stdout.write(' \\')
      sys.stdout.write('\n')
    if self._long_text:
      sys.stdout.write('\n***************\n')
      # Write separately in case it's unicode.
      sys.stdout.write(self._long_text)
      sys.stdout.write('\n***************\n')

  def json_format(self):
    return {
        'message': self._message,
        'items': [str(item) for item in self._items],
        'long_text': self._long_text,
        'fatal': self.fatal
    }


# Top level object so multiprocessing can pickle
# Public access through OutputApi object.
class _PresubmitError(_PresubmitResult):
  """A hard presubmit error."""
  fatal = True


# Top level object so multiprocessing can pickle
# Public access through OutputApi object.
class _PresubmitPromptWarning(_PresubmitResult):
  """An warning that prompts the user if they want to continue."""
  should_prompt = True


# Top level object so multiprocessing can pickle
# Public access through OutputApi object.
class _PresubmitNotifyResult(_PresubmitResult):
  """Just print something to the screen -- but it's not even a warning."""
  pass


# Top level object so multiprocessing can pickle
# Public access through OutputApi object.
class _MailTextResult(_PresubmitResult):
  """A warning that should be included in the review request email."""
  def __init__(self, *args, **kwargs):
    super(_MailTextResult, self).__init__()
    raise NotImplementedError()

class GerritAccessor(object):
  """Limited Gerrit functionality for canned presubmit checks to work.

  To avoid excessive Gerrit calls, caches the results.
  """

  def __init__(self, host):
    self.host = host
    self.cache = {}

  def _FetchChangeDetail(self, issue):
    # Separate function to be easily mocked in tests.
    try:
      return gerrit_util.GetChangeDetail(
          self.host, str(issue),
          ['ALL_REVISIONS', 'DETAILED_LABELS', 'ALL_COMMITS'])
    except gerrit_util.GerritError as e:
      if e.http_status == 404:
        raise Exception('Either Gerrit issue %s doesn\'t exist, or '
                        'no credentials to fetch issue details' % issue)
      raise

  def GetChangeInfo(self, issue):
    """Returns labels and all revisions (patchsets) for this issue.

    The result is a dictionary according to Gerrit REST Api.
    https://gerrit-review.googlesource.com/Documentation/rest-api.html

    However, API isn't very clear what's inside, so see tests for example.
    """
    assert issue
    cache_key = int(issue)
    if cache_key not in self.cache:
      self.cache[cache_key] = self._FetchChangeDetail(issue)
    return self.cache[cache_key]

  def GetChangeDescription(self, issue, patchset=None):
    """If patchset is none, fetches current patchset."""
    info = self.GetChangeInfo(issue)
    # info is a reference to cache. We'll modify it here adding description to
    # it to the right patchset, if it is not yet there.

    # Find revision info for the patchset we want.
    if patchset is not None:
      for rev, rev_info in info['revisions'].items():
        if str(rev_info['_number']) == str(patchset):
          break
      else:
        raise Exception('patchset %s doesn\'t exist in issue %s' % (
            patchset, issue))
    else:
      rev = info['current_revision']
      rev_info = info['revisions'][rev]

    return rev_info['commit']['message']

  def GetDestRef(self, issue):
    ref = self.GetChangeInfo(issue)['branch']
    if not ref.startswith('refs/'):
      # NOTE: it is possible to create 'refs/x' branch,
      # aka 'refs/heads/refs/x'. However, this is ill-advised.
      ref = 'refs/heads/%s' % ref
    return ref

  def GetChangeOwner(self, issue):
    return self.GetChangeInfo(issue)['owner']['email']

  def GetChangeReviewers(self, issue, approving_only=True):
    changeinfo = self.GetChangeInfo(issue)
    if approving_only:
      labelinfo = changeinfo.get('labels', {}).get('Code-Review', {})
      values = labelinfo.get('values', {}).keys()
      try:
        max_value = max(int(v) for v in values)
        reviewers = [r for r in labelinfo.get('all', [])
                     if r.get('value', 0) == max_value]
      except ValueError:  # values is the empty list
        reviewers = []
    else:
      reviewers = changeinfo.get('reviewers', {}).get('REVIEWER', [])
    return [r.get('email') for r in reviewers]

  def UpdateDescription(self, description, issue):
    gerrit_util.SetCommitMessage(self.host, issue, description, notify='NONE')


class OutputApi(object):
  """An instance of OutputApi gets passed to presubmit scripts so that they
  can output various types of results.
  """
  PresubmitResult = _PresubmitResult
  PresubmitError = _PresubmitError
  PresubmitPromptWarning = _PresubmitPromptWarning
  PresubmitNotifyResult = _PresubmitNotifyResult
  MailTextResult = _MailTextResult

  def __init__(self, is_committing):
    self.is_committing = is_committing
    self.more_cc = []

  def AppendCC(self, cc):
    """Appends a user to cc for this change."""
    self.more_cc.append(cc)

  def PresubmitPromptOrNotify(self, *args, **kwargs):
    """Warn the user when uploading, but only notify if committing."""
    if self.is_committing:
      return self.PresubmitNotifyResult(*args, **kwargs)
    return self.PresubmitPromptWarning(*args, **kwargs)


class InputApi(object):
  """An instance of this object is passed to presubmit scripts so they can
  know stuff about the change they're looking at.
  """
  # Method could be a function
  # pylint: disable=no-self-use

  # File extensions that are considered source files from a style guide
  # perspective. Don't modify this list from a presubmit script!
  #
  # Files without an extension aren't included in the list. If you want to
  # filter them as source files, add r'(^|.*?[\\\/])[^.]+$' to the white list.
  # Note that ALL CAPS files are black listed in DEFAULT_BLACK_LIST below.
  DEFAULT_WHITE_LIST = (
      # C++ and friends
      r'.+\.c$', r'.+\.cc$', r'.+\.cpp$', r'.+\.h$', r'.+\.m$', r'.+\.mm$',
      r'.+\.inl$', r'.+\.asm$', r'.+\.hxx$', r'.+\.hpp$', r'.+\.s$', r'.+\.S$',
      # Scripts
      r'.+\.js$', r'.+\.py$', r'.+\.sh$', r'.+\.rb$', r'.+\.pl$', r'.+\.pm$',
      # Other
      r'.+\.java$', r'.+\.mk$', r'.+\.am$', r'.+\.css$', r'.+\.mojom$',
      r'.+\.fidl$'
  )

  # Path regexp that should be excluded from being considered containing source
  # files. Don't modify this list from a presubmit script!
  DEFAULT_BLACK_LIST = (
      r'testing_support[\\\/]google_appengine[\\\/].*',
      r'.*\bexperimental[\\\/].*',
      # Exclude third_party/.* but NOT third_party/{WebKit,blink}
      # (crbug.com/539768 and crbug.com/836555).
      r'.*\bthird_party[\\\/](?!(WebKit|blink)[\\\/]).*',
      # Output directories (just in case)
      r'.*\bDebug[\\\/].*',
      r'.*\bRelease[\\\/].*',
      r'.*\bxcodebuild[\\\/].*',
      r'.*\bout[\\\/].*',
      # All caps files like README and LICENCE.
      r'.*\b[A-Z0-9_]{2,}$',
      # SCM (can happen in dual SCM configuration). (Slightly over aggressive)
      r'(|.*[\\\/])\.git[\\\/].*',
      r'(|.*[\\\/])\.svn[\\\/].*',
      # There is no point in processing a patch file.
      r'.+\.diff$',
      r'.+\.patch$',
  )

  def __init__(self, change, presubmit_path, is_committing,
      verbose, gerrit_obj, dry_run=None, thread_pool=None, parallel=False):
    """Builds an InputApi object.

    Args:
      change: A presubmit.Change object.
      presubmit_path: The path to the presubmit script being processed.
      is_committing: True if the change is about to be committed.
      gerrit_obj: provides basic Gerrit codereview functionality.
      dry_run: if true, some Checks will be skipped.
      parallel: if true, all tests reported via input_api.RunTests for all
                PRESUBMIT files will be run in parallel.
    """
    # Version number of the presubmit_support script.
    self.version = [int(x) for x in __version__.split('.')]
    self.change = change
    self.is_committing = is_committing
    self.gerrit = gerrit_obj
    self.dry_run = dry_run

    self.parallel = parallel
    self.thread_pool = thread_pool or ThreadPool()

    # We expose various modules and functions as attributes of the input_api
    # so that presubmit scripts don't have to import them.
    self.ast = ast
    self.basename = os.path.basename
    self.cpplint = cpplint
    self.fnmatch = fnmatch
    self.gclient_paths = gclient_paths
    # TODO(yyanagisawa): stop exposing this when python3 become default.
    # Since python3's tempfile has TemporaryDirectory, we do not need this.
    self.temporary_directory = gclient_utils.temporary_directory
    self.glob = glob.glob
    self.json = json
    self.logging = logging.getLogger('PRESUBMIT')
    self.os_listdir = os.listdir
    self.os_path = os.path
    self.os_stat = os.stat
    self.os_walk = os.walk
    self.re = re
    self.subprocess = subprocess
    self.sys = sys
    self.tempfile = tempfile
    self.time = time
    self.unittest = unittest
    if sys.version_info.major == 2:
      self.urllib2 = urllib2
    self.urllib_request = urllib_request
    self.urllib_error = urllib_error

    self.is_windows = sys.platform == 'win32'

    # Set python_executable to 'vpython' in order to allow scripts in other
    # repos (e.g. src.git) to automatically pick up that repo's .vpython file,
    # instead of inheriting the one in depot_tools.
    self.python_executable = 'vpython'
    self.environ = os.environ

    # InputApi.platform is the platform you're currently running on.
    self.platform = sys.platform

    self.cpu_count = multiprocessing.cpu_count()

    # The local path of the currently-being-processed presubmit script.
    self._current_presubmit_path = os.path.dirname(presubmit_path)

    # We carry the canned checks so presubmit scripts can easily use them.
    self.canned_checks = presubmit_canned_checks

    # Temporary files we must manually remove at the end of a run.
    self._named_temporary_files = []

    # TODO(dpranke): figure out a list of all approved owners for a repo
    # in order to be able to handle wildcard OWNERS files?
    self.owners_db = owners.Database(change.RepositoryRoot(),
                                     fopen=open, os_path=self.os_path)
    self.owners_finder = owners_finder.OwnersFinder
    self.verbose = verbose
    self.Command = CommandData

    # Replace <hash_map> and <hash_set> as headers that need to be included
    # with 'base/containers/hash_tables.h' instead.
    # Access to a protected member _XX of a client class
    # pylint: disable=protected-access
    self.cpplint._re_pattern_templates = [
      (a, b, 'base/containers/hash_tables.h')
        if header in ('<hash_map>', '<hash_set>') else (a, b, header)
      for (a, b, header) in cpplint._re_pattern_templates
    ]

  def SetTimeout(self, timeout):
    self.thread_pool.timeout = timeout

  def PresubmitLocalPath(self):
    """Returns the local path of the presubmit script currently being run.

    This is useful if you don't want to hard-code absolute paths in the
    presubmit script.  For example, It can be used to find another file
    relative to the PRESUBMIT.py script, so the whole tree can be branched and
    the presubmit script still works, without editing its content.
    """
    return self._current_presubmit_path

  def AffectedFiles(self, include_deletes=True, file_filter=None):
    """Same as input_api.change.AffectedFiles() except only lists files
    (and optionally directories) in the same directory as the current presubmit
    script, or subdirectories thereof. Note that files are listed using the OS
    path separator, so backslashes are used as separators on Windows.
    """
    dir_with_slash = normpath('%s/' % self.PresubmitLocalPath())
    if len(dir_with_slash) == 1:
      dir_with_slash = ''

    return list(filter(
        lambda x: normpath(x.AbsoluteLocalPath()).startswith(dir_with_slash),
        self.change.AffectedFiles(include_deletes, file_filter)))

  def LocalPaths(self):
    """Returns local paths of input_api.AffectedFiles()."""
    paths = [af.LocalPath() for af in self.AffectedFiles()]
    logging.debug('LocalPaths: %s', paths)
    return paths

  def AbsoluteLocalPaths(self):
    """Returns absolute local paths of input_api.AffectedFiles()."""
    return [af.AbsoluteLocalPath() for af in self.AffectedFiles()]

  def AffectedTestableFiles(self, include_deletes=None, **kwargs):
    """Same as input_api.change.AffectedTestableFiles() except only lists files
    in the same directory as the current presubmit script, or subdirectories
    thereof.
    """
    if include_deletes is not None:
      warn('AffectedTestableFiles(include_deletes=%s)'
               ' is deprecated and ignored' % str(include_deletes),
           category=DeprecationWarning,
           stacklevel=2)
    return list(filter(
        lambda x: x.IsTestableFile(),
        self.AffectedFiles(include_deletes=False, **kwargs)))

  def AffectedTextFiles(self, include_deletes=None):
    """An alias to AffectedTestableFiles for backwards compatibility."""
    return self.AffectedTestableFiles(include_deletes=include_deletes)

  def FilterSourceFile(self, affected_file, white_list=None, black_list=None):
    """Filters out files that aren't considered 'source file'.

    If white_list or black_list is None, InputApi.DEFAULT_WHITE_LIST
    and InputApi.DEFAULT_BLACK_LIST is used respectively.

    The lists will be compiled as regular expression and
    AffectedFile.LocalPath() needs to pass both list.

    Note: Copy-paste this function to suit your needs or use a lambda function.
    """
    def Find(affected_file, items):
      local_path = affected_file.LocalPath()
      for item in items:
        if self.re.match(item, local_path):
          return True
      return False
    return (Find(affected_file, white_list or self.DEFAULT_WHITE_LIST) and
            not Find(affected_file, black_list or self.DEFAULT_BLACK_LIST))

  def AffectedSourceFiles(self, source_file):
    """Filter the list of AffectedTestableFiles by the function source_file.

    If source_file is None, InputApi.FilterSourceFile() is used.
    """
    if not source_file:
      source_file = self.FilterSourceFile
    return list(filter(source_file, self.AffectedTestableFiles()))

  def RightHandSideLines(self, source_file_filter=None):
    """An iterator over all text lines in 'new' version of changed files.

    Only lists lines from new or modified text files in the change that are
    contained by the directory of the currently executing presubmit script.

    This is useful for doing line-by-line regex checks, like checking for
    trailing whitespace.

    Yields:
      a 3 tuple:
        the AffectedFile instance of the current file;
        integer line number (1-based); and
        the contents of the line as a string.

    Note: The carriage return (LF or CR) is stripped off.
    """
    files = self.AffectedSourceFiles(source_file_filter)
    return _RightHandSideLinesImpl(files)

  def ReadFile(self, file_item, mode='r'):
    """Reads an arbitrary file.

    Deny reading anything outside the repository.
    """
    if isinstance(file_item, AffectedFile):
      file_item = file_item.AbsoluteLocalPath()
    if not file_item.startswith(self.change.RepositoryRoot()):
      raise IOError('Access outside the repository root is denied.')
    return gclient_utils.FileRead(file_item, mode)

  def CreateTemporaryFile(self, **kwargs):
    """Returns a named temporary file that must be removed with a call to
    RemoveTemporaryFiles().

    All keyword arguments are forwarded to tempfile.NamedTemporaryFile(),
    except for |delete|, which is always set to False.

    Presubmit checks that need to create a temporary file and pass it for
    reading should use this function instead of NamedTemporaryFile(), as
    Windows fails to open a file that is already open for writing.

      with input_api.CreateTemporaryFile() as f:
        f.write('xyz')
        f.close()
        input_api.subprocess.check_output(['script-that', '--reads-from',
                                           f.name])


    Note that callers of CreateTemporaryFile() should not worry about removing
    any temporary file; this is done transparently by the presubmit handling
    code.
    """
    if 'delete' in kwargs:
      # Prevent users from passing |delete|; we take care of file deletion
      # ourselves and this prevents unintuitive error messages when we pass
      # delete=False and 'delete' is also in kwargs.
      raise TypeError('CreateTemporaryFile() does not take a "delete" '
                      'argument, file deletion is handled automatically by '
                      'the same presubmit_support code that creates InputApi '
                      'objects.')
    temp_file = self.tempfile.NamedTemporaryFile(delete=False, **kwargs)
    self._named_temporary_files.append(temp_file.name)
    return temp_file

  @property
  def tbr(self):
    """Returns if a change is TBR'ed."""
    return 'TBR' in self.change.tags or self.change.TBRsFromDescription()

  def RunTests(self, tests_mix, parallel=True):
    tests = []
    msgs = []
    for t in tests_mix:
      if isinstance(t, OutputApi.PresubmitResult) and t:
        msgs.append(t)
      else:
        assert issubclass(t.message, _PresubmitResult)
        tests.append(t)
        if self.verbose:
          t.info = _PresubmitNotifyResult
        if not t.kwargs.get('cwd'):
          t.kwargs['cwd'] = self.PresubmitLocalPath()
    self.thread_pool.AddTests(tests, parallel)
    # When self.parallel is True (i.e. --parallel is passed as an option)
    # RunTests doesn't actually run tests. It adds them to a ThreadPool that
    # will run all tests once all PRESUBMIT files are processed.
    # Otherwise, it will run them and return the results.
    if not self.parallel:
      msgs.extend(self.thread_pool.RunAsync())
    return msgs


class _DiffCache(object):
  """Caches diffs retrieved from a particular SCM."""
  def __init__(self, upstream=None):
    """Stores the upstream revision against which all diffs will be computed."""
    self._upstream = upstream

  def GetDiff(self, path, local_root):
    """Get the diff for a particular path."""
    raise NotImplementedError()

  def GetOldContents(self, path, local_root):
    """Get the old version for a particular path."""
    raise NotImplementedError()


class _GitDiffCache(_DiffCache):
  """DiffCache implementation for git; gets all file diffs at once."""
  def __init__(self, upstream):
    super(_GitDiffCache, self).__init__(upstream=upstream)
    self._diffs_by_file = None

  def GetDiff(self, path, local_root):
    if not self._diffs_by_file:
      # Compute a single diff for all files and parse the output; should
      # with git this is much faster than computing one diff for each file.
      diffs = {}

      # Don't specify any filenames below, because there are command line length
      # limits on some platforms and GenerateDiff would fail.
      unified_diff = scm.GIT.GenerateDiff(local_root, files=[], full_move=True,
                                          branch=self._upstream)

      # This regex matches the path twice, separated by a space. Note that
      # filename itself may contain spaces.
      file_marker = re.compile('^diff --git (?P<filename>.*) (?P=filename)$')
      current_diff = []
      keep_line_endings = True
      for x in unified_diff.splitlines(keep_line_endings):
        match = file_marker.match(x)
        if match:
          # Marks the start of a new per-file section.
          diffs[match.group('filename')] = current_diff = [x]
        elif x.startswith('diff --git'):
          raise PresubmitFailure('Unexpected diff line: %s' % x)
        else:
          current_diff.append(x)

      self._diffs_by_file = dict(
        (normpath(path), ''.join(diff)) for path, diff in diffs.items())

    if path not in self._diffs_by_file:
      raise PresubmitFailure(
          'Unified diff did not contain entry for file %s' % path)

    return self._diffs_by_file[path]

  def GetOldContents(self, path, local_root):
    return scm.GIT.GetOldContents(local_root, path, branch=self._upstream)


class AffectedFile(object):
  """Representation of a file in a change."""

  DIFF_CACHE = _DiffCache

  # Method could be a function
  # pylint: disable=no-self-use
  def __init__(self, path, action, repository_root, diff_cache):
    self._path = path
    self._action = action
    self._local_root = repository_root
    self._is_directory = None
    self._cached_changed_contents = None
    self._cached_new_contents = None
    self._diff_cache = diff_cache
    logging.debug('%s(%s)', self.__class__.__name__, self._path)

  def LocalPath(self):
    """Returns the path of this file on the local disk relative to client root.

    This should be used for error messages but not for accessing files,
    because presubmit checks are run with CWD=PresubmitLocalPath() (which is
    often != client root).
    """
    return normpath(self._path)

  def AbsoluteLocalPath(self):
    """Returns the absolute path of this file on the local disk.
    """
    return os.path.abspath(os.path.join(self._local_root, self.LocalPath()))

  def Action(self):
    """Returns the action on this opened file, e.g. A, M, D, etc."""
    return self._action

  def IsTestableFile(self):
    """Returns True if the file is a text file and not a binary file.

    Deleted files are not text file."""
    raise NotImplementedError()  # Implement when needed

  def IsTextFile(self):
    """An alias to IsTestableFile for backwards compatibility."""
    return self.IsTestableFile()

  def OldContents(self):
    """Returns an iterator over the lines in the old version of file.

    The old version is the file before any modifications in the user's
    workspace, i.e. the 'left hand side'.

    Contents will be empty if the file is a directory or does not exist.
    Note: The carriage returns (LF or CR) are stripped off.
    """
    return self._diff_cache.GetOldContents(self.LocalPath(),
                                           self._local_root).splitlines()

  def NewContents(self):
    """Returns an iterator over the lines in the new version of file.

    The new version is the file in the user's workspace, i.e. the 'right hand
    side'.

    Contents will be empty if the file is a directory or does not exist.
    Note: The carriage returns (LF or CR) are stripped off.
    """
    if self._cached_new_contents is None:
      self._cached_new_contents = []
      try:
        self._cached_new_contents = gclient_utils.FileRead(
            self.AbsoluteLocalPath(), 'rU').splitlines()
      except IOError:
        pass  # File not found?  That's fine; maybe it was deleted.
    return self._cached_new_contents[:]

  def ChangedContents(self):
    """Returns a list of tuples (line number, line text) of all new lines.

     This relies on the scm diff output describing each changed code section
     with a line of the form

     ^@@ <old line num>,<old size> <new line num>,<new size> @@$
    """
    if self._cached_changed_contents is not None:
      return self._cached_changed_contents[:]
    self._cached_changed_contents = []
    line_num = 0

    for line in self.GenerateScmDiff().splitlines():
      m = re.match(r'^@@ [0-9\,\+\-]+ \+([0-9]+)\,[0-9]+ @@', line)
      if m:
        line_num = int(m.groups(1)[0])
        continue
      if line.startswith('+') and not line.startswith('++'):
        self._cached_changed_contents.append((line_num, line[1:]))
      if not line.startswith('-'):
        line_num += 1
    return self._cached_changed_contents[:]

  def __str__(self):
    return self.LocalPath()

  def GenerateScmDiff(self):
    return self._diff_cache.GetDiff(self.LocalPath(), self._local_root)


class GitAffectedFile(AffectedFile):
  """Representation of a file in a change out of a git checkout."""
  # Method 'NNN' is abstract in class 'NNN' but is not overridden
  # pylint: disable=abstract-method

  DIFF_CACHE = _GitDiffCache

  def __init__(self, *args, **kwargs):
    AffectedFile.__init__(self, *args, **kwargs)
    self._server_path = None
    self._is_testable_file = None

  def IsTestableFile(self):
    if self._is_testable_file is None:
      if self.Action() == 'D':
        # A deleted file is not testable.
        self._is_testable_file = False
      else:
        self._is_testable_file = os.path.isfile(self.AbsoluteLocalPath())
    return self._is_testable_file


class Change(object):
  """Describe a change.

  Used directly by the presubmit scripts to query the current change being
  tested.

  Instance members:
    tags: Dictionary of KEY=VALUE pairs found in the change description.
    self.KEY: equivalent to tags['KEY']
  """

  _AFFECTED_FILES = AffectedFile

  # Matches key/value (or 'tag') lines in changelist descriptions.
  TAG_LINE_RE = re.compile(
      '^[ \t]*(?P<key>[A-Z][A-Z_0-9]*)[ \t]*=[ \t]*(?P<value>.*?)[ \t]*$')
  scm = ''

  def __init__(
      self, name, description, local_root, files, issue, patchset, author,
      upstream=None):
    if files is None:
      files = []
    self._name = name
    # Convert root into an absolute path.
    self._local_root = os.path.abspath(local_root)
    self._upstream = upstream
    self.issue = issue
    self.patchset = patchset
    self.author_email = author

    self._full_description = ''
    self.tags = {}
    self._description_without_tags = ''
    self.SetDescriptionText(description)

    assert all(
        (isinstance(f, (list, tuple)) and len(f) == 2) for f in files), files

    diff_cache = self._AFFECTED_FILES.DIFF_CACHE(self._upstream)
    self._affected_files = [
        self._AFFECTED_FILES(path, action.strip(), self._local_root, diff_cache)
        for action, path in files
    ]

  def Name(self):
    """Returns the change name."""
    return self._name

  def DescriptionText(self):
    """Returns the user-entered changelist description, minus tags.

    Any line in the user-provided description starting with e.g. 'FOO='
    (whitespace permitted before and around) is considered a tag line.  Such
    lines are stripped out of the description this function returns.
    """
    return self._description_without_tags

  def FullDescriptionText(self):
    """Returns the complete changelist description including tags."""
    return self._full_description

  def SetDescriptionText(self, description):
    """Sets the full description text (including tags) to |description|.

    Also updates the list of tags."""
    self._full_description = description

    # From the description text, build up a dictionary of key/value pairs
    # plus the description minus all key/value or 'tag' lines.
    description_without_tags = []
    self.tags = {}
    for line in self._full_description.splitlines():
      m = self.TAG_LINE_RE.match(line)
      if m:
        self.tags[m.group('key')] = m.group('value')
      else:
        description_without_tags.append(line)

    # Change back to text and remove whitespace at end.
    self._description_without_tags = (
        '\n'.join(description_without_tags).rstrip())

  def AddDescriptionFooter(self, key, value):
    """Adds the given footer to the change description.

    Args:
      key: A string with the key for the git footer. It must conform to
        the git footers format (i.e. 'List-Of-Tokens') and will be case
        normalized so that each token is title-cased.
      value: A string with the value for the git footer.
    """
    description = git_footers.add_footer(
        self.FullDescriptionText(), git_footers.normalize_name(key), value)
    self.SetDescriptionText(description)

  def RepositoryRoot(self):
    """Returns the repository (checkout) root directory for this change,
    as an absolute path.
    """
    return self._local_root

  def __getattr__(self, attr):
    """Return tags directly as attributes on the object."""
    if not re.match(r'^[A-Z_]*$', attr):
      raise AttributeError(self, attr)
    return self.tags.get(attr)

  def GitFootersFromDescription(self):
    """Return the git footers present in the description.

    Returns:
      footers: A dict of {footer: [values]} containing a multimap of the footers
        in the change description.
    """
    return git_footers.parse_footers(self.FullDescriptionText())

  def BugsFromDescription(self):
    """Returns all bugs referenced in the commit description."""
    tags = [b.strip() for b in self.tags.get('BUG', '').split(',') if b.strip()]
    footers = []
    parsed = self.GitFootersFromDescription()
    unsplit_footers = parsed.get('Bug', []) + parsed.get('Fixed', [])
    for unsplit_footer in unsplit_footers:
      footers += [b.strip() for b in unsplit_footer.split(',')]
    return sorted(set(tags + footers))

  def ReviewersFromDescription(self):
    """Returns all reviewers listed in the commit description."""
    # We don't support a 'R:' git-footer for reviewers; that is in metadata.
    tags = [r.strip() for r in self.tags.get('R', '').split(',') if r.strip()]
    return sorted(set(tags))

  def TBRsFromDescription(self):
    """Returns all TBR reviewers listed in the commit description."""
    tags = [r.strip() for r in self.tags.get('TBR', '').split(',') if r.strip()]
    # TODO(agable): Remove support for 'Tbr:' when TBRs are programmatically
    # determined by self-CR+1s.
    footers = self.GitFootersFromDescription().get('Tbr', [])
    return sorted(set(tags + footers))

  # TODO(agable): Delete these once we're sure they're unused.
  @property
  def BUG(self):
    return ','.join(self.BugsFromDescription())
  @property
  def R(self):
    return ','.join(self.ReviewersFromDescription())
  @property
  def TBR(self):
    return ','.join(self.TBRsFromDescription())

  def AllFiles(self, root=None):
    """List all files under source control in the repo."""
    raise NotImplementedError()

  def AffectedFiles(self, include_deletes=True, file_filter=None):
    """Returns a list of AffectedFile instances for all files in the change.

    Args:
      include_deletes: If false, deleted files will be filtered out.
      file_filter: An additional filter to apply.

    Returns:
      [AffectedFile(path, action), AffectedFile(path, action)]
    """
    affected = list(filter(file_filter, self._affected_files))

    if include_deletes:
      return affected
    return list(filter(lambda x: x.Action() != 'D', affected))

  def AffectedTestableFiles(self, include_deletes=None, **kwargs):
    """Return a list of the existing text files in a change."""
    if include_deletes is not None:
      warn('AffectedTeestableFiles(include_deletes=%s)'
               ' is deprecated and ignored' % str(include_deletes),
           category=DeprecationWarning,
           stacklevel=2)
    return list(filter(
        lambda x: x.IsTestableFile(),
        self.AffectedFiles(include_deletes=False, **kwargs)))

  def AffectedTextFiles(self, include_deletes=None):
    """An alias to AffectedTestableFiles for backwards compatibility."""
    return self.AffectedTestableFiles(include_deletes=include_deletes)

  def LocalPaths(self):
    """Convenience function."""
    return [af.LocalPath() for af in self.AffectedFiles()]

  def AbsoluteLocalPaths(self):
    """Convenience function."""
    return [af.AbsoluteLocalPath() for af in self.AffectedFiles()]

  def RightHandSideLines(self):
    """An iterator over all text lines in 'new' version of changed files.

    Lists lines from new or modified text files in the change.

    This is useful for doing line-by-line regex checks, like checking for
    trailing whitespace.

    Yields:
      a 3 tuple:
        the AffectedFile instance of the current file;
        integer line number (1-based); and
        the contents of the line as a string.
    """
    return _RightHandSideLinesImpl(
        x for x in self.AffectedFiles(include_deletes=False)
        if x.IsTestableFile())

  def OriginalOwnersFiles(self):
    """A map from path names of affected OWNERS files to their old content."""
    def owners_file_filter(f):
      return 'OWNERS' in os.path.split(f.LocalPath())[1]
    files = self.AffectedFiles(file_filter=owners_file_filter)
    return dict([(f.LocalPath(), f.OldContents()) for f in files])


class GitChange(Change):
  _AFFECTED_FILES = GitAffectedFile
  scm = 'git'

  def AllFiles(self, root=None):
    """List all files under source control in the repo."""
    root = root or self.RepositoryRoot()
    return subprocess.check_output(
        ['git', '-c', 'core.quotePath=false', 'ls-files', '--', '.'],
        cwd=root).splitlines()


def ListRelevantPresubmitFiles(files, root):
  """Finds all presubmit files that apply to a given set of source files.

  If inherit-review-settings-ok is present right under root, looks for
  PRESUBMIT.py in directories enclosing root.

  Args:
    files: An iterable container containing file paths.
    root: Path where to stop searching.

  Return:
    List of absolute paths of the existing PRESUBMIT.py scripts.
  """
  files = [normpath(os.path.join(root, f)) for f in files]

  # List all the individual directories containing files.
  directories = set([os.path.dirname(f) for f in files])

  # Ignore root if inherit-review-settings-ok is present.
  if os.path.isfile(os.path.join(root, 'inherit-review-settings-ok')):
    root = None

  # Collect all unique directories that may contain PRESUBMIT.py.
  candidates = set()
  for directory in directories:
    while True:
      if directory in candidates:
        break
      candidates.add(directory)
      if directory == root:
        break
      parent_dir = os.path.dirname(directory)
      if parent_dir == directory:
        # We hit the system root directory.
        break
      directory = parent_dir

  # Look for PRESUBMIT.py in all candidate directories.
  results = []
  for directory in sorted(list(candidates)):
    try:
      for f in os.listdir(directory):
        p = os.path.join(directory, f)
        if os.path.isfile(p) and re.match(
            r'PRESUBMIT.*\.py$', f) and not f.startswith('PRESUBMIT_test'):
          results.append(p)
    except OSError:
      pass

  logging.debug('Presubmit files: %s', ','.join(results))
  return results


class GetTryMastersExecuter(object):
  @staticmethod
  def ExecPresubmitScript(script_text, presubmit_path, project, change):
    """Executes GetPreferredTryMasters() from a single presubmit script.

    Args:
      script_text: The text of the presubmit script.
      presubmit_path: Project script to run.
      project: Project name to pass to presubmit script for bot selection.

    Return:
      A map of try masters to map of builders to set of tests.
    """
    context = {}
    try:
      exec(compile(script_text, 'PRESUBMIT.py', 'exec', dont_inherit=True),
           context)
    except Exception as e:
      raise PresubmitFailure('"%s" had an exception.\n%s'
                             % (presubmit_path, e))

    function_name = 'GetPreferredTryMasters'
    if function_name not in context:
      return {}
    get_preferred_try_masters = context[function_name]
    if not len(inspect.getargspec(get_preferred_try_masters)[0]) == 2:
      raise PresubmitFailure(
          'Expected function "GetPreferredTryMasters" to take two arguments.')
    return get_preferred_try_masters(project, change)


class GetPostUploadExecuter(object):
  @staticmethod
  def ExecPresubmitScript(script_text, presubmit_path, gerrit_obj, change):
    """Executes PostUploadHook() from a single presubmit script.

    Args:
      script_text: The text of the presubmit script.
      presubmit_path: Project script to run.
      gerrit_obj: The GerritAccessor object.
      change: The Change object.

    Return:
      A list of results objects.
    """
    context = {}
    try:
      exec(compile(script_text, 'PRESUBMIT.py', 'exec', dont_inherit=True),
           context)
    except Exception as e:
      raise PresubmitFailure('"%s" had an exception.\n%s'
                             % (presubmit_path, e))

    function_name = 'PostUploadHook'
    if function_name not in context:
      return {}
    post_upload_hook = context[function_name]
    if not len(inspect.getargspec(post_upload_hook)[0]) == 3:
      raise PresubmitFailure(
          'Expected function "PostUploadHook" to take three arguments.')
    return post_upload_hook(gerrit_obj, change, OutputApi(False))


def _MergeMasters(masters1, masters2):
  """Merges two master maps. Merges also the tests of each builder."""
  result = {}
  for (master, builders) in itertools.chain(masters1.items(),
                                            masters2.items()):
    new_builders = result.setdefault(master, {})
    for (builder, tests) in builders.items():
      new_builders.setdefault(builder, set([])).update(tests)
  return result


def DoGetTryMasters(change,
                    changed_files,
                    repository_root,
                    default_presubmit,
                    project,
                    verbose,
                    output_stream):
  """Get the list of try masters from the presubmit scripts.

  Args:
    changed_files: List of modified files.
    repository_root: The repository root.
    default_presubmit: A default presubmit script to execute in any case.
    project: Optional name of a project used in selecting trybots.
    verbose: Prints debug info.
    output_stream: A stream to write debug output to.

  Return:
    Map of try masters to map of builders to set of tests.
  """
  presubmit_files = ListRelevantPresubmitFiles(changed_files, repository_root)
  if not presubmit_files and verbose:
    output_stream.write('Warning, no PRESUBMIT.py found.\n')
  results = {}
  executer = GetTryMastersExecuter()

  if default_presubmit:
    if verbose:
      output_stream.write('Running default presubmit script.\n')
    fake_path = os.path.join(repository_root, 'PRESUBMIT.py')
    results = _MergeMasters(results, executer.ExecPresubmitScript(
        default_presubmit, fake_path, project, change))
  for filename in presubmit_files:
    filename = os.path.abspath(filename)
    if verbose:
      output_stream.write('Running %s\n' % filename)
    # Accept CRLF presubmit script.
    presubmit_script = gclient_utils.FileRead(filename, 'rU')
    results = _MergeMasters(results, executer.ExecPresubmitScript(
        presubmit_script, filename, project, change))

  # Make sets to lists again for later JSON serialization.
  for builders in results.values():
    for builder in builders:
      builders[builder] = list(builders[builder])

  if results and verbose:
    output_stream.write('%s\n' % str(results))
  return results


def DoPostUploadExecuter(change,
                         gerrit_obj,
                         verbose):
  """Execute the post upload hook.

  Args:
    change: The Change object.
    gerrit_obj: The GerritAccessor object.
    verbose: Prints debug info.
  """
  presubmit_files = ListRelevantPresubmitFiles(
      change.LocalPaths(), change.RepositoryRoot())
  if not presubmit_files and verbose:
    sys.stdout.write('Warning, no PRESUBMIT.py found.\n')
  results = []
  executer = GetPostUploadExecuter()
  # The root presubmit file should be executed after the ones in subdirectories.
  # i.e. the specific post upload hooks should run before the general ones.
  # Thus, reverse the order provided by ListRelevantPresubmitFiles.
  presubmit_files.reverse()

  for filename in presubmit_files:
    filename = os.path.abspath(filename)
    if verbose:
      sys.stdout.write('Running %s\n' % filename)
    # Accept CRLF presubmit script.
    presubmit_script = gclient_utils.FileRead(filename, 'rU')
    results.extend(executer.ExecPresubmitScript(
        presubmit_script, filename, gerrit_obj, change))

  if not results:
    return 0

  sys.stdout.write('\n')
  sys.stdout.write('** Post Upload Hook Messages **\n')

  exit_code = 0
  for result in results:
    if result.fatal:
      exit_code = 1
    result.handle()
    sys.stdout.write('\n')

  return exit_code


class PresubmitExecuter(object):
  def __init__(self, change, committing, verbose,
               gerrit_obj, dry_run=None, thread_pool=None, parallel=False):
    """
    Args:
      change: The Change object.
      committing: True if 'git cl land' is running, False if 'git cl upload' is.
      gerrit_obj: provides basic Gerrit codereview functionality.
      dry_run: if true, some Checks will be skipped.
      parallel: if true, all tests reported via input_api.RunTests for all
                PRESUBMIT files will be run in parallel.
    """
    self.change = change
    self.committing = committing
    self.gerrit = gerrit_obj
    self.verbose = verbose
    self.dry_run = dry_run
    self.more_cc = []
    self.thread_pool = thread_pool
    self.parallel = parallel

  def ExecPresubmitScript(self, script_text, presubmit_path):
    """Executes a single presubmit script.

    Args:
      script_text: The text of the presubmit script.
      presubmit_path: The path to the presubmit file (this will be reported via
        input_api.PresubmitLocalPath()).

    Return:
      A list of result objects, empty if no problems.
    """

    # Change to the presubmit file's directory to support local imports.
    main_path = os.getcwd()
    os.chdir(os.path.dirname(presubmit_path))

    # Load the presubmit script into context.
    input_api = InputApi(self.change, presubmit_path, self.committing,
                         self.verbose, gerrit_obj=self.gerrit,
                         dry_run=self.dry_run, thread_pool=self.thread_pool,
                         parallel=self.parallel)
    output_api = OutputApi(self.committing)
    context = {}
    try:
      exec(compile(script_text, 'PRESUBMIT.py', 'exec', dont_inherit=True),
           context)
    except Exception as e:
      raise PresubmitFailure('"%s" had an exception.\n%s' % (presubmit_path, e))

    # These function names must change if we make substantial changes to
    # the presubmit API that are not backwards compatible.
    if self.committing:
      function_name = 'CheckChangeOnCommit'
    else:
      function_name = 'CheckChangeOnUpload'
    if function_name in context:
      try:
        context['__args'] = (input_api, output_api)
        logging.debug('Running %s in %s', function_name, presubmit_path)
        result = eval(function_name + '(*__args)', context)
        logging.debug('Running %s done.', function_name)
        self.more_cc.extend(output_api.more_cc)
      finally:
        for f in input_api._named_temporary_files:
          os.remove(f)
      if not isinstance(result, (tuple, list)):
        raise PresubmitFailure(
          'Presubmit functions must return a tuple or list')
      for item in result:
        if not isinstance(item, OutputApi.PresubmitResult):
          raise PresubmitFailure(
            'All presubmit results must be of types derived from '
            'output_api.PresubmitResult')
    else:
      result = ()  # no error since the script doesn't care about current event.

    # Return the process to the original working directory.
    os.chdir(main_path)
    return result

def DoPresubmitChecks(change,
                      committing,
                      verbose,
                      default_presubmit,
                      may_prompt,
                      gerrit_obj,
                      dry_run=None,
                      parallel=False,
                      json_output=None):
  """Runs all presubmit checks that apply to the files in the change.

  This finds all PRESUBMIT.py files in directories enclosing the files in the
  change (up to the repository root) and calls the relevant entrypoint function
  depending on whether the change is being committed or uploaded.

  Prints errors, warnings and notifications.  Prompts the user for warnings
  when needed.

  Args:
    change: The Change object.
    committing: True if 'git cl land' is running, False if 'git cl upload' is.
    verbose: Prints debug info.
    default_presubmit: A default presubmit script to execute in any case.
    may_prompt: Enable (y/n) questions on warning or error. If False,
                any questions are answered with yes by default.
    gerrit_obj: provides basic Gerrit codereview functionality.
    dry_run: if true, some Checks will be skipped.
    parallel: if true, all tests specified by input_api.RunTests in all
              PRESUBMIT files will be run in parallel.

  Return:
    1 if presubmit checks failed or 0 otherwise.
  """
  old_environ = os.environ
  try:
    # Make sure python subprocesses won't generate .pyc files.
    os.environ = os.environ.copy()
    os.environ['PYTHONDONTWRITEBYTECODE'] = '1'

    if committing:
      sys.stdout.write('Running presubmit commit checks ...\n')
    else:
      sys.stdout.write('Running presubmit upload checks ...\n')
    start_time = time_time()
    presubmit_files = ListRelevantPresubmitFiles(
        change.AbsoluteLocalPaths(), change.RepositoryRoot())
    if not presubmit_files and verbose:
      sys.stdout.write('Warning, no PRESUBMIT.py found.\n')
    results = []
    thread_pool = ThreadPool()
    executer = PresubmitExecuter(change, committing, verbose, gerrit_obj,
                                 dry_run, thread_pool, parallel)
    if default_presubmit:
      if verbose:
        sys.stdout.write('Running default presubmit script.\n')
      fake_path = os.path.join(change.RepositoryRoot(), 'PRESUBMIT.py')
      results += executer.ExecPresubmitScript(default_presubmit, fake_path)
    for filename in presubmit_files:
      filename = os.path.abspath(filename)
      if verbose:
        sys.stdout.write('Running %s\n' % filename)
      # Accept CRLF presubmit script.
      presubmit_script = gclient_utils.FileRead(filename, 'rU')
      results += executer.ExecPresubmitScript(presubmit_script, filename)

    results += thread_pool.RunAsync()

    messages = {}
    should_prompt = False
    presubmits_failed = False
    for result in results:
      if result.fatal:
        presubmits_failed = True
        messages.setdefault('ERRORS', []).append(result)
      elif result.should_prompt:
        should_prompt = True
        messages.setdefault('Warnings', []).append(result)
      else:
        messages.setdefault('Messages', []).append(result)

    sys.stdout.write('\n')
    for name, items in messages.items():
      sys.stdout.write('** Presubmit %s **\n' % name)
      for item in items:
        item.handle()
        sys.stdout.write('\n')

    total_time = time_time() - start_time
    if total_time > 1.0:
      sys.stdout.write(
          'Presubmit checks took %.1fs to calculate.\n\n' % total_time)

    if not should_prompt and not presubmits_failed:
      sys.stdout.write('Presubmit checks passed.\n')
    elif should_prompt:
      sys.stdout.write('There were presubmit warnings. ')
      if may_prompt:
        presubmits_failed = not prompt_should_continue(
            'Are you sure you wish to continue? (y/N): ')

    if json_output:
      # Write the presubmit results to json output
      presubmit_results = {
        'errors': [
            error.json_format()
            for error in messages.get('ERRORS', [])
        ],
        'notifications': [
            notification.json_format()
            for notification in messages.get('Messages', [])
        ],
        'warnings': [
            warning.json_format()
            for warning in messages.get('Warnings', [])
        ],
        'more_cc': executer.more_cc,
      }

      gclient_utils.FileWrite(
          json_output, json.dumps(presubmit_results, sort_keys=True))

    global _ASKED_FOR_FEEDBACK
    # Ask for feedback one time out of 5.
    if (len(results) and random.randint(0, 4) == 0 and not _ASKED_FOR_FEEDBACK):
      sys.stdout.write(
          'Was the presubmit check useful? If not, run "git cl presubmit -v"\n'
          'to figure out which PRESUBMIT.py was run, then run git blame\n'
          'on the file to figure out who to ask for help.\n')
      _ASKED_FOR_FEEDBACK = True

    return 1 if presubmits_failed else 0
  finally:
    os.environ = old_environ


def _scan_sub_dirs(mask, recursive):
  if not recursive:
    return [x for x in glob.glob(mask) if x not in ('.svn', '.git')]

  results = []
  for root, dirs, files in os.walk('.'):
    if '.svn' in dirs:
      dirs.remove('.svn')
    if '.git' in dirs:
      dirs.remove('.git')
    for name in files:
      if fnmatch.fnmatch(name, mask):
        results.append(os.path.join(root, name))
  return results


def _parse_files(args, recursive):
  logging.debug('Searching for %s', args)
  files = []
  for arg in args:
    files.extend([('M', f) for f in _scan_sub_dirs(arg, recursive)])
  return files


def _parse_change(parser, options):
  """Process change options.

  Args:
    parser: The parser used to parse the arguments from command line.
    options: The arguments parsed from command line.
  Returns:
    A GitChange if the change root is a git repository, or a Change otherwise.
  """
  if options.files and options.all_files:
    parser.error('<files> cannot be specified when --all-files is set.')

  change_scm = scm.determine_scm(options.root)
  if change_scm != 'git' and not options.files:
    parser.error('<files> is not optional for unversioned directories.')

  if options.files:
    change_files = _parse_files(options.files, options.recursive)
  elif options.all_files:
    change_files = [('M', f) for f in scm.GIT.GetAllFiles(options.root)]
  else:
    change_files = scm.GIT.CaptureStatus(
        options.root, options.upstream or None)

  logging.info('Found %d file(s).', len(change_files))

  change_class = GitChange if change_scm == 'git' else Change
  return change_class(
      options.name,
      options.description,
      options.root,
      change_files,
      options.issue,
      options.patchset,
      options.author,
      upstream=options.upstream)


def _parse_gerrit_options(parser, options):
  """Process gerrit options.

  SIDE EFFECTS: Modifies options.author and options.description from Gerrit if
  options.gerrit_fetch is set.

  Args:
    parser: The parser used to parse the arguments from command line.
    options: The arguments parsed from command line.
  Returns:
    A GerritAccessor object if options.gerrit_url is set, or None otherwise.
  """
  gerrit_obj = None
  if options.gerrit_url:
    gerrit_obj = GerritAccessor(urlparse.urlparse(options.gerrit_url).netloc)

  if not options.gerrit_fetch:
    return gerrit_obj

  if not options.gerrit_url or not options.issue or not options.patchset:
    parser.error(
        '--gerrit_fetch requires --gerrit_url, --issue and --patchset.')

  options.author = gerrit_obj.GetChangeOwner(options.issue)
  options.description = gerrit_obj.GetChangeDescription(
      options.issue, options.patchset)

  logging.info('Got author: "%s"', options.author)
  logging.info('Got description: """\n%s\n"""', options.description)

  return gerrit_obj


@contextlib.contextmanager
def canned_check_filter(method_names):
  filtered = {}
  try:
    for method_name in method_names:
      if not hasattr(presubmit_canned_checks, method_name):
        logging.warn('Skipping unknown "canned" check %s' % method_name)
        continue
      filtered[method_name] = getattr(presubmit_canned_checks, method_name)
      setattr(presubmit_canned_checks, method_name, lambda *_a, **_kw: [])
    yield
  finally:
    for name, method in filtered.items():
      setattr(presubmit_canned_checks, name, method)


def main(argv=None):
  parser = argparse.ArgumentParser(usage='%(prog)s [options] <files...>')
  hooks = parser.add_mutually_exclusive_group()
  hooks.add_argument('-c', '--commit', action='store_true',
                     help='Use commit instead of upload checks.')
  hooks.add_argument('-u', '--upload', action='store_false', dest='commit',
                     help='Use upload instead of commit checks.')
  hooks.add_argument('--post_upload', action='store_true',
                     help='Run post-upload commit hooks.')
  parser.add_argument('-r', '--recursive', action='store_true',
                      help='Act recursively.')
  parser.add_argument('-v', '--verbose', action='count', default=0,
                      help='Use 2 times for more debug info.')
  parser.add_argument('--name', default='no name')
  parser.add_argument('--author')
  desc = parser.add_mutually_exclusive_group()
  desc.add_argument('--description', default='', help='The change description.')
  desc.add_argument('--description_file',
                    help='File to read change description from.')
  parser.add_argument('--issue', type=int, default=0)
  parser.add_argument('--patchset', type=int, default=0)
  parser.add_argument('--root', default=os.getcwd(),
                      help='Search for PRESUBMIT.py up to this directory. '
                      'If inherit-review-settings-ok is present in this '
                      'directory, parent directories up to the root file '
                      'system directories will also be searched.')
  parser.add_argument('--upstream',
                      help='Git only: the base ref or upstream branch against '
                      'which the diff should be computed.')
  parser.add_argument('--default_presubmit')
  parser.add_argument('--may_prompt', action='store_true', default=False)
  parser.add_argument('--skip_canned', action='append', default=[],
                      help='A list of checks to skip which appear in '
                      'presubmit_canned_checks. Can be provided multiple times '
                      'to skip multiple canned checks.')
  parser.add_argument('--dry_run', action='store_true', help=argparse.SUPPRESS)
  parser.add_argument('--gerrit_url', help=argparse.SUPPRESS)
  parser.add_argument('--gerrit_fetch', action='store_true',
                      help=argparse.SUPPRESS)
  parser.add_argument('--parallel', action='store_true',
                      help='Run all tests specified by input_api.RunTests in '
                           'all PRESUBMIT files in parallel.')
  parser.add_argument('--json_output',
                      help='Write presubmit errors to json output.')
  parser.add_argument('--all_files', action='store_true',
                      help='Mark all files under source control as modified.')
  parser.add_argument('files', nargs='*',
                      help='List of files to be marked as modified when '
                      'executing presubmit or post-upload hooks. fnmatch '
                      'wildcards can also be used.')

  options = parser.parse_args(argv)

  log_level = logging.ERROR
  if options.verbose >= 2:
    log_level = logging.DEBUG
  elif options.verbose:
    log_level = logging.INFO
  log_format = ('[%(levelname).1s%(asctime)s %(process)d %(thread)d '
                '%(filename)s] %(message)s')
  logging.basicConfig(format=log_format, level=log_level)

  if options.description_file:
    options.description = gclient_utils.FileRead(options.description_file)
  gerrit_obj = _parse_gerrit_options(parser, options)
  change = _parse_change(parser, options)

  try:
    if options.post_upload:
      return DoPostUploadExecuter(
          change,
          gerrit_obj,
          options.verbose)
    with canned_check_filter(options.skip_canned):
      return DoPresubmitChecks(
          change,
          options.commit,
          options.verbose,
          options.default_presubmit,
          options.may_prompt,
          gerrit_obj,
          options.dry_run,
          options.parallel,
          options.json_output)
  except PresubmitFailure as e:
    print(e, file=sys.stderr)
    print('Maybe your depot_tools is out of date?', file=sys.stderr)
    return 2


if __name__ == '__main__':
  fix_encoding.fix_encoding()
  try:
    sys.exit(main())
  except KeyboardInterrupt:
    sys.stderr.write('interrupted\n')
    sys.exit(2)
