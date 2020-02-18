# Copyright 2013 The LUCI Authors. All rights reserved.
# Use of this source code is governed under the Apache License, Version 2.0
# that can be found in the LICENSE file.

"""subprocess42 is the answer to life the universe and everything.

It has the particularity of having a Popen implementation that can yield output
as it is produced while implementing a timeout and NOT requiring the use of
worker threads.

Example:
  Wait for a child process with a timeout, send SIGTERM, wait a grace period
  then send SIGKILL:

    def wait_terminate_then_kill(proc, timeout, grace):
      try:
        return proc.wait(timeout)
      except subprocess42.TimeoutExpired:
        proc.terminate()
        try:
          return proc.wait(grace)
        except subprocess42.TimeoutExpired:
          proc.kill()
        return proc.wait()


TODO(maruel): Add VOID support like subprocess2.
"""

import collections
import contextlib
import errno
import os
import signal
import sys
import threading
import time

import subprocess

from subprocess import CalledProcessError, PIPE, STDOUT  # pylint: disable=W0611
from subprocess import list2cmdline


# Default maxsize argument.
MAX_SIZE = 16384


# Set to True when inhibit_crash_dump() has been called.
_OS_ERROR_REPORTING_INHIBITED = False


if subprocess.mswindows:
  import ctypes
  import msvcrt  # pylint: disable=F0401
  from ctypes import wintypes
  from ctypes import windll


  # Which to be received depends on how this process was called and outside the
  # control of this script. See Popen docstring for more details.
  STOP_SIGNALS = (signal.SIGBREAK, signal.SIGTERM)

  # Windows processes constants.

  # Subset of process priority classes.
  # https://docs.microsoft.com/windows/desktop/api/processthreadsapi/nf-processthreadsapi-getpriorityclass
  BELOW_NORMAL_PRIORITY_CLASS = 0x4000
  IDLE_PRIORITY_CLASS = 0x40

  # Constants passed to CreateProcess creationflags argument.
  # https://docs.microsoft.com/windows/desktop/api/processthreadsapi/nf-processthreadsapi-createprocessw
  CREATE_SUSPENDED = 0x4
  CREATE_NEW_CONSOLE = subprocess.CREATE_NEW_CONSOLE
  CREATE_NEW_PROCESS_GROUP = subprocess.CREATE_NEW_PROCESS_GROUP

  # Job Objects constants and structs.
  JobObjectBasicLimitInformation = 2
  JobObjectBasicUIRestrictions = 4
  JobObjectExtendedLimitInformation = 9

  # https://docs.microsoft.com/windows/desktop/api/winnt/ns-winnt-_jobobject_basic_limit_information
  JOB_OBJECT_LIMIT_ACTIVE_PROCESS = 0x8
  JOB_OBJECT_LIMIT_AFFINITY = 0x10
  JOB_OBJECT_LIMIT_BREAKAWAY_OK = 0x800
  JOB_OBJECT_LIMIT_DIE_ON_UNHANDLED_EXCEPTION = 0x400
  JOB_OBJECT_LIMIT_JOB_MEMORY = 0x200
  JOB_OBJECT_LIMIT_JOB_TIME = 0x4
  JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE = 0x2000
  JOB_OBJECT_LIMIT_PRESERVE_JOB_TIME = 0x40
  JOB_OBJECT_LIMIT_PRIORITY_CLASS = 0x20
  JOB_OBJECT_LIMIT_PROCESS_MEMORY = 0x100
  JOB_OBJECT_LIMIT_PROCESS_TIME = 0x2
  JOB_OBJECT_LIMIT_SCHEDULING_CLASS = 0x80
  JOB_OBJECT_LIMIT_SILENT_BREAKAWAY_OK = 0x1000
  JOB_OBJECT_LIMIT_SUBSET_AFFINITY = 0x4000
  JOB_OBJECT_LIMIT_WORKINGSET = 0x1
  class JOBOBJECT_BASIC_LIMIT_INFORMATION(ctypes.Structure):
    _fields_ = [
      ('PerProcessUserTimeLimit', ctypes.wintypes.LARGE_INTEGER),
      ('PerJobUserTimeLimit', ctypes.wintypes.LARGE_INTEGER),
      ('LimitFlags', ctypes.wintypes.DWORD),
      ('MinimumWorkingSetSize', ctypes.c_size_t),
      ('MaximumWorkingSetSize', ctypes.c_size_t),
      ('ActiveProcessLimit', ctypes.wintypes.DWORD),
      ('Affinity', ctypes.POINTER(ctypes.wintypes.ULONG)),
      ('PriorityClass', ctypes.wintypes.DWORD),
      ('SchedulingClass', ctypes.wintypes.DWORD),
    ]

    @property
    def info_type(self):
      return JobObjectBasicLimitInformation


  # https://docs.microsoft.com/windows/desktop/api/winnt/ns-winnt-io_counters
  class IO_COUNTERS(ctypes.Structure):
    _fields_ = [
      ('ReadOperationCount', ctypes.c_ulonglong),
      ('WriteOperationCount', ctypes.c_ulonglong),
      ('OtherOperationCount', ctypes.c_ulonglong),
      ('ReadTransferCount', ctypes.c_ulonglong),
      ('WriteTransferCount', ctypes.c_ulonglong),
      ('OtherTransferCount', ctypes.c_ulonglong),
    ]


  # https://docs.microsoft.com/windows/desktop/api/winnt/ns-winnt-_jobobject_extended_limit_information
  class JOBOBJECT_EXTENDED_LIMIT_INFORMATION(ctypes.Structure):
    _fields_ = [
      ('BasicLimitInformation', JOBOBJECT_BASIC_LIMIT_INFORMATION),
      ('IoInfo', IO_COUNTERS),
      ('ProcessMemoryLimit', ctypes.c_size_t),
      ('JobMemoryLimit', ctypes.c_size_t),
      ('PeakProcessMemoryUsed', ctypes.c_size_t),
      ('PeakJobMemoryUsed', ctypes.c_size_t),
    ]

    @property
    def info_type(self):
      return JobObjectExtendedLimitInformation


  # https://docs.microsoft.com/en-us/windows/desktop/api/winnt/ns-winnt-jobobject_basic_ui_restrictions
  JOB_OBJECT_UILIMIT_DESKTOP = 0x40
  JOB_OBJECT_UILIMIT_DISPLAYSETTINGS = 0x10
  JOB_OBJECT_UILIMIT_EXITWINDOWS = 0x80
  JOB_OBJECT_UILIMIT_GLOBALATOMS = 0x20
  JOB_OBJECT_UILIMIT_HANDLES = 0x1
  class JOBOBJECT_BASIC_UI_RESTRICTIONS(ctypes.Structure):
    _fields_ = [('UIRestrictionsClass', ctypes.wintypes.DWORD)]

    @property
    def info_type(self):
      return JobObjectBasicUIRestrictions


  def ReadFile(handle, desired_bytes):
    """Calls kernel32.ReadFile()."""
    c_read = wintypes.DWORD()
    buff = wintypes.create_string_buffer(desired_bytes+1)
    # If it fails, the buffer will probably(?) not be affected.
    windll.kernel32.ReadFile(
        handle, buff, desired_bytes, wintypes.byref(c_read), None)
    # NULL terminate it.
    buff[c_read.value] = '\x00'
    return wintypes.GetLastError(), buff.value

  def PeekNamedPipe(handle):
    """Calls kernel32.PeekNamedPipe(). Simplified version."""
    c_avail = wintypes.DWORD()
    c_message = wintypes.DWORD()
    success = windll.kernel32.PeekNamedPipe(
        handle, None, 0, None, wintypes.byref(c_avail),
        wintypes.byref(c_message))
    if not success:
      raise OSError(wintypes.GetLastError())
    return c_avail.value

  def recv_multi_impl(conns, maxsize, timeout):
    """Reads from the first available pipe.

    It will immediately return on a closed connection, independent of timeout.

    Arguments:
    - maxsize: Maximum number of bytes to return. Defaults to MAX_SIZE.
    - timeout: If None, it is blocking. If 0 or above, will return None if no
          data is available within |timeout| seconds.

    Returns:
      tuple(int(index), str(data), bool(closed)).
    """
    assert conns
    assert timeout is None or isinstance(timeout, (int, float)), timeout
    maxsize = max(maxsize or MAX_SIZE, 1)

    # TODO(maruel): Use WaitForMultipleObjects(). Python creates anonymous pipes
    # for proc.stdout and proc.stderr but they are implemented as named pipes on
    # Windows. Since named pipes are not waitable object, they can't be passed
    # as-is to WFMO(). So this means N times CreateEvent(), N times ReadFile()
    # and finally WFMO(). This requires caching the events handles in the Popen
    # object and remembering the pending ReadFile() calls. This will require
    # some re-architecture to store the relevant event handle and OVERLAPPEDIO
    # object in Popen or the file object.
    start = time.time()
    handles = [
      (i, msvcrt.get_osfhandle(c.fileno())) for i, c in enumerate(conns)
    ]
    while True:
      for index, handle in handles:
        try:
          avail = min(PeekNamedPipe(handle), maxsize)
          if avail:
            return index, ReadFile(handle, avail)[1], False
        except OSError:
          # The pipe closed.
          return index, None, True

      if timeout is not None and (time.time() - start) >= timeout:
        return None, None, False
      # Polling rocks.
      time.sleep(0.001)


  class _JobObject(object):
    """Manages a job object."""
    def __init__(self, containment):
      # The first process to be added to the job object.
      self._proc = None
      # https://docs.microsoft.com/windows/desktop/api/jobapi2/nf-jobapi2-createjobobjectw
      self._hjob = ctypes.windll.kernel32.CreateJobObjectW(None, None)
      if not self._hjob:
        # pylint: disable=undefined-variable
        raise WindowsError(
            'Failed to create job object: %s' % ctypes.GetLastError())

      # TODO(maruel): Use a completion port to listen to messages as described
      # at
      # https://docs.microsoft.com/windows/desktop/api/winnt/ns-winnt-_jobobject_associate_completion_port

      # TODO(maruel): Enable configuring the limit, like maximum number of
      # processes, working set size.
      obj = JOBOBJECT_EXTENDED_LIMIT_INFORMATION()
      obj.BasicLimitInformation.LimitFlags = (
          JOB_OBJECT_LIMIT_DIE_ON_UNHANDLED_EXCEPTION|
          JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE)
      if containment.limit_processes:
        obj.BasicLimitInformation.ActiveProcessLimit = (
            containment.limit_processes)
        obj.BasicLimitInformation.LimitFlags |= JOB_OBJECT_LIMIT_ACTIVE_PROCESS
      if containment.limit_total_committed_memory:
        obj.BasicLimitInformation.LimitFlags |= JOB_OBJECT_LIMIT_JOB_MEMORY
        obj.JobMemoryLimit = containment.limit_total_committed_memory
      self._set_information(obj)

      # Add UI limitations.
      # TODO(maruel): The limitations currently used are based on Chromium's
      # testing needs. For example many unit tests use the clipboard, or change
      # the display settings (!)
      obj = JOBOBJECT_BASIC_UI_RESTRICTIONS(
          UIRestrictionsClass=
              JOB_OBJECT_UILIMIT_DESKTOP|
              JOB_OBJECT_UILIMIT_EXITWINDOWS|
              JOB_OBJECT_UILIMIT_GLOBALATOMS|
              JOB_OBJECT_UILIMIT_HANDLES)
      self._set_information(obj)

    def close(self):
      if self._hjob:
        ctypes.windll.kernel32.CloseHandle(self._hjob)
        self._hjob = None

    def kill(self, exit_code):
      """Return True if the TerminateJobObject call succeeded, or not operation
      was done.
      """
      if not self._hjob:
        return True
      # "Kill" the job object instead of the process.
      # https://docs.microsoft.com/windows/desktop/api/jobapi2/nf-jobapi2-terminatejobobject
      return bool(
          ctypes.windll.kernel32.TerminateJobObject(self._hjob, exit_code))

    def assign_proc(self, proc):
      """Assigns the process handle to the job object."""
      if not ctypes.windll.kernel32.AssignProcessToJobObject(
          self._hjob, int(proc._handle)):
        # pylint: disable=undefined-variable
        raise WindowsError(
            'Failed to assign job object: %s' % ctypes.GetLastError())
      if not ctypes.windll.kernel32.ResumeThread(int(proc._handle_thread)):
        # pylint: disable=undefined-variable
        raise WindowsError(
            'Failed to resume child process thread: %s' %
            ctypes.GetLastError())

    def _set_information(self, obj):
      # https://docs.microsoft.com/windows/desktop/api/jobapi2/nf-jobapi2-setinformationjobobject
      if not ctypes.windll.kernel32.SetInformationJobObject(
          self._hjob, obj.info_type, ctypes.byref(obj), ctypes.sizeof(obj)):
        # pylint: disable=undefined-variable
        raise WindowsError(
            'Failed to adjust job object with type %s: %s' %
            (obj.info_type, ctypes.GetLastError()))


else:
  import fcntl  # pylint: disable=F0401
  import select


  # Signals that mean this process should exit quickly.
  STOP_SIGNALS = (signal.SIGINT, signal.SIGTERM)


  def recv_multi_impl(conns, maxsize, timeout):
    """Reads from the first available pipe.

    It will immediately return on a closed connection, independent of timeout.

    Arguments:
    - maxsize: Maximum number of bytes to return. Defaults to MAX_SIZE.
    - timeout: If None, it is blocking. If 0 or above, will return None if no
          data is available within |timeout| seconds.

    Returns:
      tuple(int(index), str(data), bool(closed)).
    """
    assert conns
    assert timeout is None or isinstance(timeout, (int, float)), timeout
    maxsize = max(maxsize or MAX_SIZE, 1)

    # select(timeout=0) will block, it has to be a value > 0.
    if timeout == 0:
      timeout = 0.001
    try:
      r, _, _ = select.select(conns, [], [], timeout)
    except select.error:
      r = None
    if not r:
      return None, None, False

    conn = r[0]
    # Temporarily make it non-blocking.
    # TODO(maruel): This is not very efficient when the caller is doing this in
    # a loop. Add a mechanism to have the caller handle this.
    flags = fcntl.fcntl(conn, fcntl.F_GETFL)
    if not conn.closed:
      # pylint: disable=E1101
      fcntl.fcntl(conn, fcntl.F_SETFL, flags | os.O_NONBLOCK)
    try:
      try:
        data = conn.read(maxsize)
      except IOError as e:
        # On posix, this means the read would block.
        if e.errno == errno.EAGAIN:
          return conns.index(conn), None, False
        raise e

      if not data:
        # On posix, this means the channel closed.
        return conns.index(conn), None, True

      return conns.index(conn), data, False
    finally:
      if not conn.closed:
        fcntl.fcntl(conn, fcntl.F_SETFL, flags)


class TimeoutExpired(Exception):
  """Compatible with python3 subprocess."""
  def __init__(self, cmd, timeout, output=None, stderr=None):
    self.cmd = cmd
    self.timeout = timeout
    self.output = output
    # Non-standard:
    self.stderr = stderr
    super(TimeoutExpired, self).__init__(str(self))

  def __str__(self):
    return "Command '%s' timed out after %s seconds" % (self.cmd, self.timeout)


class Containment(object):
  """Defines the containment used to run the process.

  On Windows, this is done via a Job Object.
  https://docs.microsoft.com/en-us/windows/desktop/procthread/job-objects
  """
  # AUTO will use containment if possible, but will not fail if not adequate on
  # this operating system.
  #
  # For example, job objects cannot be nested on Windows 7 / Windows Server 2008
  # and earlier, thus AUTO means NONE on these platforms. Windows 8 and Window
  # Server 2012 and later support nest job objects, thus AUTO means ENABLED on
  # these platforms.
  # See https://docs.microsoft.com/en-us/windows/desktop/procthread/job-objects
  # cgroups will be added.
  NONE, AUTO, JOB_OBJECT = range(3)

  NAMES = {
    NONE: 'NONE',
    AUTO: 'AUTO',
    JOB_OBJECT: 'JOB_OBJECT',
  }

  def __init__(
      self,
      containment_type=NONE,
      limit_processes=0,
      limit_total_committed_memory=0):
    self.containment_type = containment_type
    # Limit on the number of active processes.
    self.limit_processes = limit_processes
    self.limit_total_committed_memory = limit_total_committed_memory

  def __eq__(self, rhs):
    if not rhs:
      return False
    return (
        self.containment_type == rhs.containment_type and
        self.limit_processes == rhs.limit_processes and
        self.limit_total_committed_memory == rhs.limit_total_committed_memory)

  def __str__(self):
    return 'Containment<%s, %s, %s>' % (
        self.NAMES[self.containment_type],
        self.limit_processes,
        self.limit_total_committed_memory)

  def __repr__(self):
    return self.__str__()


class Popen(subprocess.Popen):
  """Adds timeout support on stdout and stderr.

  Inspired by
  http://code.activestate.com/recipes/440554-module-to-allow-asynchronous-subprocess-use-on-win/

  Unlike subprocess, yield_any(), recv_*(), communicate() will close stdout and
  stderr once the child process closes them, after all the data is read.

  Mutated behavior:
  - args: transparently encode('utf-8') any unicode items.
  - cwd: transparently encode('utf-8') if unicode.
  - env: transparently encode('utf-8') any unicode keys or values.

  Additional arguments:
  - detached: If True, the process is created in a new process group. On
    Windows, use CREATE_NEW_PROCESS_GROUP. On posix, use os.setpgid(0, 0).
  - lower_priority: reduce the process priority a bit.
  - containment: Containment instance or None. When using containment, one of
        communicate(), poll(), wait(), yield_any(), yield_any_line() must be
        used otherwise a kernel handle may leak.

  Additional members:
  - start: timestamp when this process started.
  - end: timestamp when this process exited, as seen by this process.
  - detached: If True, the child process was started as a detached process.
  - gid: process group id, if any.
  - duration: time in seconds the process lasted.

  Additional methods:
  - yield_any(): yields output until the process terminates.
  - recv_any(): reads from stdout and/or stderr with optional timeout.
  - recv_out() & recv_err(): specialized version of recv_any().
  """
  # subprocess.Popen.__init__() is not threadsafe; there is a race between
  # creating the exec-error pipe for the child and setting it to CLOEXEC during
  # which another thread can fork and cause the pipe to be inherited by its
  # descendents, which will cause the current Popen to hang until all those
  # descendents exit. Protect this with a lock so that only one fork/exec can
  # happen at a time.
  popen_lock = threading.Lock()

  def __init__(self, args, **kwargs):
    # Windows version of subprocess.Popen() really doens't like unicode. In
    # practice we should use the current ANSI code page, but settle for utf-8
    # across all OSes for consistency.
    to_str = lambda i: i if isinstance(i, str) else i.encode('utf-8')
    args = [to_str(i) for i in args]
    if kwargs.get('cwd') is not None:
      kwargs['cwd'] = to_str(kwargs['cwd'])
    if kwargs.get('env'):
      kwargs['env'] = {
        to_str(k): to_str(v) for k, v in kwargs['env'].iteritems()
      }

    # Set via contrived monkey patching below, because stdlib doesn't expose
    # thread handle. Only set on Windows.
    self._handle_thread = None
    # Will be set by super constructor but may be accessed in failure modes by
    # _cleanup().
    self._handle = None
    self._job = None

    self.detached = kwargs.pop('detached', False)
    if self.detached:
      if subprocess.mswindows:
        prev = kwargs.get('creationflags', 0)
        kwargs['creationflags'] = prev | CREATE_NEW_PROCESS_GROUP
      else:
        old_preexec_fn_1 = kwargs.get('preexec_fn')
        def new_preexec_fn_1():
          if old_preexec_fn_1:
            old_preexec_fn_1()
          os.setpgid(0, 0)
        kwargs['preexec_fn'] = new_preexec_fn_1

    if kwargs.pop('lower_priority', False):
      if subprocess.mswindows:
        # TODO(maruel): If already in this class, it should use
        # IDLE_PRIORITY_CLASS.
        prev = kwargs.get('creationflags', 0)
        kwargs['creationflags'] = prev | BELOW_NORMAL_PRIORITY_CLASS
      else:
        old_preexec_fn_2 = kwargs.get('preexec_fn')
        def new_preexec_fn_2():
          if old_preexec_fn_2:
            old_preexec_fn_2()
          os.nice(1)
        kwargs['preexec_fn'] = new_preexec_fn_2

    self.containment = kwargs.pop('containment', None) or Containment()
    if self.containment.containment_type != Containment.NONE:
      if self.containment.containment_type == Containment.JOB_OBJECT:
        if not subprocess.mswindows:
          raise NotImplementedError(
              'containment is not implemented on this platform')
      if subprocess.mswindows:
        # May throw an WindowsError.
        # pylint: disable=undefined-variable
        self._job = _JobObject(self.containment)
        # In this case, start the process suspended, so we can assign the job
        # object, then resume it.
        prev = kwargs.get('creationflags', 0)
        kwargs['creationflags'] = prev | CREATE_SUSPENDED

    self.end = None
    self.gid = None
    self.start = time.time()
    try:
      with self.popen_lock:
        if subprocess.mswindows:
          # We need the thread handle, save it.
          old = subprocess._subprocess.CreateProcess
          class FakeHandle(object):
            def Close(self):
              pass
          def patch_CreateProcess(*args, **kwargs):
             hp, ht, pid, tid = old(*args, **kwargs)
             # Save the thread handle, and return a fake one that
             # _execute_child() will close indiscriminally.
             self._handle_thread = ht
             return hp, FakeHandle(), pid, tid
          subprocess._subprocess.CreateProcess = patch_CreateProcess
        try:
          super(Popen, self).__init__(args, **kwargs)
        finally:
          if subprocess.mswindows:
            subprocess._subprocess.CreateProcess = old
    except:
      self._cleanup()
      raise

    self.args = args
    if self.detached and not subprocess.mswindows:
      try:
        self.gid = os.getpgid(self.pid)
      except OSError:
        # sometimes the process can run+finish before we collect its pgid. fun.
        pass

    if self._job:
      try:
        self._job.assign_proc(self)
      except OSError:
        self.kill()
        self.wait()

  def duration(self):
    """Duration of the child process.

    It is greater or equal to the actual time the child process ran. It can be
    significantly higher than the real value if neither .wait() nor .poll() was
    used.
    """
    return (self.end or time.time()) - self.start

  # pylint: disable=arguments-differ,redefined-builtin
  def communicate(self, input=None, timeout=None):
    """Implements python3's timeout support.

    Unlike wait(), timeout=0 is considered the same as None.

    Returns:
      tuple of (stdout, stderr).

    Raises:
    - TimeoutExpired when more than timeout seconds were spent waiting for the
      process.
    """
    if not timeout:
      return super(Popen, self).communicate(input=input)

    assert isinstance(timeout, (int, float)), timeout

    if self.stdin or self.stdout or self.stderr:
      stdout = '' if self.stdout else None
      stderr = '' if self.stderr else None
      t = None
      if input is not None:
        assert self.stdin, (
            'Can\'t use communicate(input) if not using '
            'Popen(stdin=subprocess42.PIPE')
        # TODO(maruel): Switch back to non-threading.
        def write():
          try:
            self.stdin.write(input)
          except IOError:
            pass
        t = threading.Thread(name='Popen.communicate', target=write)
        t.daemon = True
        t.start()

      try:
        if self.stdout or self.stderr:
          start = time.time()
          end = start + timeout
          def remaining():
            return max(end - time.time(), 0)
          for pipe, data in self.yield_any(timeout=remaining):
            if pipe is None:
              raise TimeoutExpired(self.args, timeout, stdout, stderr)
            assert pipe in ('stdout', 'stderr'), pipe
            if pipe == 'stdout':
              stdout += data
            else:
              stderr += data
        else:
          # Only stdin is piped.
          self.wait(timeout=timeout)
      finally:
        if t:
          try:
            self.stdin.close()
          except IOError:
            pass
          t.join()
    else:
      # No pipe. The user wanted to use wait().
      self.wait(timeout=timeout)
      return None, None

    # Indirectly initialize self.end.
    self.wait()
    return stdout, stderr

  def wait(self, timeout=None,
           poll_initial_interval=0.001,
           poll_max_interval=0.05):  # pylint: disable=arguments-differ
    """Implements python3's timeout support.

    Raises:
    - TimeoutExpired when more than timeout seconds were spent waiting for the
      process.
    """
    assert timeout is None or isinstance(timeout, (int, float)), timeout
    if timeout is None:
      super(Popen, self).wait()
    elif self.returncode is None:
      if subprocess.mswindows:
        WAIT_TIMEOUT = 258
        result = subprocess._subprocess.WaitForSingleObject(
            self._handle, int(timeout * 1000))
        if result == WAIT_TIMEOUT:
          raise TimeoutExpired(self.args, timeout)
        self.returncode = subprocess._subprocess.GetExitCodeProcess(
            self._handle)
      else:
        # If you think the following code is horrible, it's because it is
        # inspired by python3's stdlib.
        end = time.time() + timeout
        delay = poll_initial_interval
        while True:
          try:
            pid, sts = subprocess._eintr_retry_call(
                os.waitpid, self.pid, os.WNOHANG)
          except OSError as e:
            if e.errno != errno.ECHILD:
              raise
            pid = self.pid
            sts = 0
          if pid == self.pid:
            # This sets self.returncode.
            self._handle_exitstatus(sts)
            break
          remaining = end - time.time()
          if remaining <= 0:
            raise TimeoutExpired(self.args, timeout)
          delay = min(delay * 2, remaining, poll_max_interval)
          time.sleep(delay)

    if not self.end:
      # communicate() uses wait() internally.
      self.end = time.time()
    self._cleanup()
    return self.returncode

  def poll(self):
    ret = super(Popen, self).poll()
    if ret is not None and not self.end:
      self.end = time.time()
      # This may kill all children processes.
      self._cleanup()
    return ret

  def yield_any_line(self, **kwargs):
    """Yields lines until the process terminates.

    Like yield_any, but yields lines.
    """
    return split(self.yield_any(**kwargs))

  def yield_any(self, maxsize=None, timeout=None):
    """Yields output until the process terminates.

    Unlike wait(), does not raise TimeoutExpired.

    Yields:
      (pipename, data) where pipename is either 'stdout', 'stderr' or None in
      case of timeout or when the child process closed one of the pipe(s) and
      all pending data on the pipe was read.

    Arguments:
    - maxsize: See recv_any(). Can be a callable function.
    - timeout: If None, the call is blocking. If set, yields None, None if no
          data is available within |timeout| seconds. It resets itself after
          each yield. Can be a callable function.
    """
    assert self.stdout or self.stderr
    if timeout is not None:
      # timeout=0 effectively means that the pipe is continuously polled.
      if isinstance(timeout, (int, float)):
        assert timeout >= 0, timeout
        old_timeout = timeout
        timeout = lambda: old_timeout
      else:
        assert callable(timeout), timeout

    if maxsize is not None and not callable(maxsize):
      assert isinstance(maxsize, (int, float)), maxsize

    last_yield = time.time()
    while self.poll() is None:
      to = timeout() if timeout else None
      if to is not None:
        to = max(to - (time.time() - last_yield), 0)
      t, data = self.recv_any(
          maxsize=maxsize() if callable(maxsize) else maxsize, timeout=to)
      if data or to is 0:
        yield t, data
        last_yield = time.time()

    # Read all remaining output in the pipes.
    # There is 3 cases:
    # - pipes get closed automatically by the calling process before it exits
    # - pipes are closed automated by the OS
    # - pipes are kept open due to grand-children processes outliving the
    #   children process.
    while True:
      ms = maxsize
      if callable(maxsize):
        ms = maxsize()
      # timeout=0 is mainly to handle the case where a grand-children process
      # outlives the process started.
      t, data = self.recv_any(maxsize=ms, timeout=0)
      if not data:
        break
      yield t, data

  def recv_any(self, maxsize=None, timeout=None):
    """Reads from the first pipe available from stdout and stderr.

    Unlike wait(), does not throw TimeoutExpired.

    Arguments:
    - maxsize: Maximum number of bytes to return. Defaults to MAX_SIZE.
    - timeout: If None, it is blocking. If 0 or above, will return None if no
          data is available within |timeout| seconds.

    Returns:
      tuple(pipename or None, str(data)). pipename is one of 'stdout' or
      'stderr'.
    """
    # recv_multi_impl will early exit on a closed connection. Loop accordingly
    # to simplify call sites.
    while True:
      pipes = [
        x for x in ((self.stderr, 'stderr'), (self.stdout, 'stdout')) if x[0]
      ]
      # If both stdout and stderr have the exact file handle, they are
      # effectively the same pipe. Deduplicate it since otherwise it confuses
      # recv_multi_impl().
      if len(pipes) == 2 and self.stderr.fileno() == self.stdout.fileno():
        pipes.pop(0)

      if not pipes:
        return None, None
      start = time.time()
      conns, names = zip(*pipes)
      index, data, closed = recv_multi_impl(conns, maxsize, timeout)
      if index is None:
        return index, data
      if closed:
        self._close(names[index])
        if not data:
          # Loop again. The other pipe may still be open.
          if timeout:
            timeout -= (time.time() - start)
          continue

      if self.universal_newlines and data:
        data = self._translate_newlines(data)
      return names[index], data

  def recv_out(self, maxsize=None, timeout=None):
    """Reads from stdout synchronously with timeout."""
    return self._recv('stdout', maxsize, timeout)

  def recv_err(self, maxsize=None, timeout=None):
    """Reads from stderr synchronously with timeout."""
    return self._recv('stderr', maxsize, timeout)

  def terminate(self):
    """Tries to do something saner on Windows that the stdlib.

    Windows:
      self.detached/CREATE_NEW_PROCESS_GROUP determines what can be used:
      - If set, only SIGBREAK can be sent and it is sent to a single process.
      - If not set, in theory only SIGINT can be used and *all processes* in
         the processgroup receive it. In practice, we just kill the process.
      See http://msdn.microsoft.com/library/windows/desktop/ms683155.aspx
      The default on Windows is to call TerminateProcess() always, which is not
      useful.

    On Posix, always send SIGTERM.
    """
    try:
      if subprocess.mswindows and self.detached:
        return self.send_signal(signal.CTRL_BREAK_EVENT)
      super(Popen, self).terminate()
    except OSError:
      # The function will throw if the process terminated in-between. Swallow
      # this.
      pass

  def kill(self):
    """Kills the process and its children if possible.

    Swallows exceptions and return True on success.

    This process may be asynchronous. The user should still call wait() to
    ensure the process is indeed terminated.
    """
    if self._job:
      # Use the equivalent of SIGKILL on linux. signal.SIGKILL is not available
      # on Windows.
      return self._job.kill(-9)

    if self.returncode is not None:
      # If a return code was recorded, it means there's nothing to kill as there
      # was no containment.
      return True

    if self.gid:
      try:
        os.killpg(self.gid, signal.SIGKILL)
      except OSError:
        return False
    else:
      try:
        super(Popen, self).kill()
      except OSError:
        return False
    return True

  def _close(self, which):
    """Closes either stdout or stderr."""
    getattr(self, which).close()
    setattr(self, which, None)

  def _cleanup(self):
    """Makes sure resources are not leaked."""
    if self._job:
      # This may kill all children processes.
      self._job.close()
      self._job = None
    if self._handle_thread:
      self._handle_thread.Close()
      self._handle_thread = None
    if self._handle:
      # self._handle is deleted via __del__ but when it happens is
      # non-deterministic, so do it earlier.
      self._handle.Close()
      self._handle = None

  def _recv(self, which, maxsize, timeout):
    """Reads from one of stdout or stderr synchronously with timeout."""
    conn = getattr(self, which)
    if conn is None:
      return None
    _, data, closed = recv_multi_impl([conn], maxsize, timeout)
    if closed:
      self._close(which)
    if self.universal_newlines and data:
      data = self._translate_newlines(data)
    return data


@contextlib.contextmanager
def set_signal_handler(signals, handler):
  """Temporarilly override signals handler.

  Useful when waiting for a child process to handle signals like SIGTERM, so the
  signal can be propagated to the child process.
  """
  previous = {s: signal.signal(s, handler) for s in signals}
  try:
    yield
  finally:
    for sig, h in previous.iteritems():
      signal.signal(sig, h)


def call(*args, **kwargs):
  """Adds support for timeout."""
  timeout = kwargs.pop('timeout', None)
  return Popen(*args, **kwargs).wait(timeout)


def check_call(*args, **kwargs):
  """Adds support for timeout."""
  retcode = call(*args, **kwargs)
  if retcode:
    raise CalledProcessError(retcode, kwargs.get('args') or args[0])
  return 0


def check_output(*args, **kwargs):
  """Adds support for timeout."""
  timeout = kwargs.pop('timeout', None)
  if 'stdout' in kwargs:
    raise ValueError('stdout argument not allowed, it will be overridden.')
  process = Popen(stdout=PIPE, *args, **kwargs)
  output, _ = process.communicate(timeout=timeout)
  retcode = process.poll()
  if retcode:
    raise CalledProcessError(retcode, kwargs.get('args') or args[0], output)
  return output


def call_with_timeout(args, timeout, **kwargs):
  """Runs an executable; kill it in case of timeout."""
  proc = Popen(args, stdin=subprocess.PIPE, stdout=subprocess.PIPE, **kwargs)
  try:
    out, err = proc.communicate(timeout=timeout)
  except TimeoutExpired as e:
    out = e.output
    err = e.stderr
    proc.kill()
    proc.wait()
  return out, err, proc.returncode, proc.duration()


def inhibit_os_error_reporting():
  """Inhibits error reporting UI and core files.

  This function should be called as early as possible in the process lifetime.
  """
  global _OS_ERROR_REPORTING_INHIBITED
  if not _OS_ERROR_REPORTING_INHIBITED:
    _OS_ERROR_REPORTING_INHIBITED = True
    if sys.platform == 'win32':
      # Windows has a bad habit of opening a dialog when a console program
      # crashes, rather than just letting it crash. Therefore, when a program
      # crashes on Windows, we don't find out until the build step times out.
      # This code prevents the dialog from appearing, so that we find out
      # immediately and don't waste time waiting for a user to close the dialog.
      # https://msdn.microsoft.com/en-us/library/windows/desktop/ms680621.aspx
      SEM_FAILCRITICALERRORS = 1
      SEM_NOGPFAULTERRORBOX = 2
      SEM_NOALIGNMENTFAULTEXCEPT = 0x8000
      ctypes.windll.kernel32.SetErrorMode(
          SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX|
            SEM_NOALIGNMENTFAULTEXCEPT)
  # TODO(maruel): Other OSes.
  # - OSX, need to figure out a way to make the following process tree local:
  #     defaults write com.apple.CrashReporter UseUNC 1
  #     defaults write com.apple.CrashReporter DialogType none
  # - Ubuntu, disable apport if needed.


def split(data, sep='\n'):
  """Splits pipe data by |sep|. Does some buffering.

  For example, [('stdout', 'a\nb'), ('stdout', '\n'), ('stderr', 'c\n')] ->
  [('stdout', 'a'), ('stdout', 'b'), ('stderr', 'c')].

  Args:
    data: iterable of tuples (pipe_name, bytes).

  Returns:
    An iterator of tuples (pipe_name, bytes) where bytes is the input data
    but split by sep into separate tuples.
  """
  # A dict {pipe_name -> list of pending chunks without separators}
  pending_chunks = collections.defaultdict(list)
  for pipe_name, chunk in data:
    if chunk is None:
      # Happens if a pipe is closed.
      continue

    pending = pending_chunks[pipe_name]
    start = 0  # offset in chunk to start |sep| search from
    while start < len(chunk):
      j = chunk.find(sep, start)
      if j == -1:
        pending_chunks[pipe_name].append(chunk[start:])
        break

      to_emit = chunk[start:j]
      start = j + 1
      if pending:
        # prepend and forget
        to_emit = ''.join(pending) + to_emit
        pending = []
        pending_chunks[pipe_name] = pending
      yield pipe_name, to_emit

  # Emit remaining chunks that don't end with separators as is.
  for pipe_name, chunks in sorted(pending_chunks.iteritems()):
    if chunks:
      yield pipe_name, ''.join(chunks)
