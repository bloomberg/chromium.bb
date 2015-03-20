# Copyright 2013 The Swarming Authors. All rights reserved.
# Use of this source code is governed under the Apache License, Version 2.0 that
# can be found in the LICENSE file.

"""subprocess42 is the answer to life the universe and everything.

It has the particularity of having a Popen implementation that can yield output
as it is produced while implementing a timeout and not requiring the use of
worker threads.

TODO(maruel): Add VOID and TIMED_OUT support like subprocess2.
"""

import contextlib
import logging
import os
import signal
import time

import subprocess

from subprocess import CalledProcessError, PIPE, STDOUT  # pylint: disable=W0611
from subprocess import call, check_output  # pylint: disable=W0611


# Default maxsize argument.
MAX_SIZE = 16384


if subprocess.mswindows:
  import msvcrt  # pylint: disable=F0401
  from ctypes import wintypes
  from ctypes import windll


  # Which to be received depends on how this process was called and outside the
  # control of this script. See Popen docstring for more details.
  STOP_SIGNALS = (signal.SIGBREAK, signal.SIGTERM)


  def ReadFile(handle, desired_bytes):
    """Calls kernel32.ReadFile()."""
    c_read = wintypes.DWORD()
    buff = wintypes.create_string_buffer(desired_bytes+1)
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
    # TODO(maruel): This is not very ifficient when the caller is doing this in
    # a loop. Add a mechanism to have the caller handle this.
    flags = fcntl.fcntl(conn, fcntl.F_GETFL)
    if not conn.closed:
      # pylint: disable=E1101
      fcntl.fcntl(conn, fcntl.F_SETFL, flags | os.O_NONBLOCK)
    try:
      data = conn.read(maxsize)
      if not data:
        # On posix, this means the channel closed.
        return conns.index(conn), None, True
      return conns.index(conn), data, False
    finally:
      if not conn.closed:
        fcntl.fcntl(conn, fcntl.F_SETFL, flags)


class Popen(subprocess.Popen):
  """Adds timeout support on stdout and stderr.

  Inspired by
  http://code.activestate.com/recipes/440554-module-to-allow-asynchronous-subprocess-use-on-win/

  Arguments:
  - detached: If True, the process is created in a new process group. On
    Windows, use CREATE_NEW_PROCESS_GROUP. On posix, use os.setpgid(0, 0).

  Additional members:
  - start: timestamp when this process started.
  - end: timestamp when this process exited, as seen by this process.
  - detached: If True, the child process was started as a detached process.
  - gid: process group id, if any.
  """
  def __init__(self, args, **kwargs):
    assert 'creationflags' not in kwargs
    assert 'preexec_fn' not in kwargs
    self.start = time.time()
    self.end = None
    self.gid = None
    self.detached = kwargs.pop('detached', False)
    if self.detached:
      if subprocess.mswindows:
        kwargs['creationflags'] = subprocess.CREATE_NEW_PROCESS_GROUP
      else:
        kwargs['preexec_fn'] = lambda: os.setpgid(0, 0)
    super(Popen, self).__init__(args, **kwargs)
    if self.detached and not subprocess.mswindows:
      self.gid = os.getpgid(self.pid)

  def duration(self):
    """Duration of the child process.

    It is greater or equal to the actual time the child process ran. It can be
    significantly higher than the real value if neither .wait() nor .poll() was
    used.
    """
    return (self.end or time.time()) - self.start

  def wait(self):
    ret = super(Popen, self).wait()
    if not self.end:
      # communicate() uses wait() internally.
      self.end = time.time()
    return ret

  def poll(self):
    ret = super(Popen, self).poll()
    if ret is not None and not self.end:
      self.end = time.time()
    return ret

  def yield_any(self, maxsize=None, hard_timeout=None, soft_timeout=None):
    """Yields output until the process terminates or is killed by a timeout.

    Yielded values are in the form (pipename, data).

    Arguments:
    - maxsize: See recv_any(). Can be a callable function.
    - hard_timeout: If None, the process is never killed. If set, the process is
          killed after |hard_timeout| seconds. Can be a callable function.
    - soft_timeout: If None, the call is blocking. If set, yields None, None
          if no data is available within |soft_timeout| seconds. It resets
          itself after each yield. Can be a callable function.
    """
    if hard_timeout is not None:
      # hard_timeout=0 means the process is not even given a little chance to
      # execute and will be immediately killed.
      if isinstance(hard_timeout, (int, float)):
        assert hard_timeout > 0., hard_timeout
        old_hard_timeout = hard_timeout
        hard_timeout = lambda: old_hard_timeout
    if soft_timeout is not None:
      # soft_timeout=0 effectively means that the pipe is continuously polled.
      if isinstance(soft_timeout, (int, float)):
        assert soft_timeout >= 0, soft_timeout
        old_soft_timeout = soft_timeout
        soft_timeout = lambda: old_soft_timeout
      else:
        assert callable(soft_timeout), soft_timeout

    last_yield = time.time()
    while self.poll() is None:
      ms = maxsize
      if callable(maxsize):
        ms = maxsize()
      t, data = self.recv_any(
          maxsize=ms,
          timeout=self._calc_timeout(hard_timeout, soft_timeout, last_yield))
      if data or soft_timeout is not None:
        yield t, data
        last_yield = time.time()

      if hard_timeout and self.duration() >= hard_timeout():
        break

    if self.poll() is None and hard_timeout:
      logging.debug('Kill %s %s', self.duration(), hard_timeout())
      self.kill()
    self.wait()

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

  def _calc_timeout(self, hard_timeout, soft_timeout, last_yield):
    """Returns the timeout to be used on the next recv_any() in yield_any().

    It depends on both timeout. It returns None if no timeout is used. Otherwise
    it returns a value >= 0.001. It's not 0 because it's effectively polling, on
    linux it can peg a single core, so adding 1ms sleep does a tremendous
    difference.
    """
    hard_remaining = (
        None if hard_timeout is None
        else max(hard_timeout() - self.duration(), 0))
    soft_remaining = (
        None if soft_timeout is None
        else max(soft_timeout() - (time.time() - last_yield), 0))
    if hard_remaining is None:
      return soft_remaining
    if soft_remaining is None:
      return hard_remaining
    return min(hard_remaining, soft_remaining)

  def recv_any(self, maxsize=None, timeout=None):
    """Reads from the first pipe available from stdout and stderr.

    Arguments:
    - maxsize: Maximum number of bytes to return. Defaults to MAX_SIZE.
    - timeout: If None, it is blocking. If 0 or above, will return None if no
          data is available within |timeout| seconds.

    Returns:
      tuple(int(index) or None, str(data)).
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

      if self.universal_newlines:
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
    """
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


def call_with_timeout(args, timeout, **kwargs):
  """Runs an executable with an optional timeout.

  timeout 0 or None disables the timeout.
  """
  proc = Popen(
      args,
      stdin=subprocess.PIPE,
      stdout=subprocess.PIPE,
      **kwargs)
  if timeout:
    out = ''
    err = ''
    for t, data in proc.yield_any(hard_timeout=timeout):
      if t == 'stdout':
        out += data
      else:
        err += data
  else:
    # This code path is much faster.
    out, err = proc.communicate()
  return out, err, proc.returncode, proc.duration()


@contextlib.contextmanager
def set_signal_handler(signals, handler):
  """Temporarilly override signals handler."""
  previous = dict((s, signal.signal(s, handler)) for s in signals)
  yield None
  for s in signals:
    signal.signal(s, previous[s])


@contextlib.contextmanager
def Popen_with_handler(args, **kwargs):
  proc = None
  def handler(_signum, _frame):
    if proc:
      proc.terminate()

  with set_signal_handler(STOP_SIGNALS, handler):
    proc = Popen(args, detached=True, **kwargs)
    try:
      yield proc
    finally:
      proc.kill()
