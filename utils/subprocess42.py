# Copyright 2013 The Swarming Authors. All rights reserved.
# Use of this source code is governed under the Apache License, Version 2.0 that
# can be found in the LICENSE file.

"""subprocess42 is the answer to life the universe and everything.

It has the particularity of having a Popen implementation that can yield output
as it is produced while implementing a timeout and not requiring the use of
worker threads.

TODO(maruel): Add VOID and TIMED_OUT support like subprocess2.
"""

import logging
import os
import time

import subprocess

from subprocess import PIPE, STDOUT, call, check_output  # pylint: disable=W0611


if subprocess.mswindows:
  import msvcrt  # pylint: disable=F0401
  from ctypes import wintypes
  from ctypes import windll

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

    If timeout is None, it's blocking. If timeout is 0, it is not blocking.
    """
    # TODO(maruel): Use WaitForMultipleObjects(). Python creates anonymous pipes
    # for proc.stdout and proc.stderr but they are implemented as named pipes on
    # Windows. Since named pipes are not waitable object, they can't be passed
    # as-is to WFMO(). So this means N times CreateEvent(), N times ReadFile()
    # and finally WFMO(). This requires caching the events handles in the Popen
    # object and remembering the pending ReadFile() calls. This will require
    # some re-architecture to store the relevant event handle and OVERLAPPEDIO
    # object in Popen or the file object.
    maxsize = max(maxsize or 16384, 1)
    if timeout:
      start = time.time()
    handles = [
      (i, msvcrt.get_osfhandle(c.fileno())) for i, c in enumerate(conns)
    ]
    while handles:
      for index, handle in handles:
        try:
          avail = min(PeekNamedPipe(handle), maxsize)
          if avail:
            return index, ReadFile(handle, avail)[1]
          if (timeout and (time.time() - start) >= timeout) or timeout == 0:
            return None, None
          # Polling rocks.
          time.sleep(0.001)
        except OSError:
          handles.remove((index, handle))
          break
    # Nothing to wait for.
    return None, None

else:
  import fcntl  # pylint: disable=F0401
  import select

  def recv_multi_impl(conns, maxsize, timeout):
    """Reads from the first available pipe.

    If timeout is None, it's blocking. If timeout is 0, it is not blocking.
    """
    try:
      r, _, _ = select.select(conns, [], [], timeout)
    except select.error:
      return None, None
    if not r:
      return None, None

    conn = r[0]
    # Temporarily make it non-blocking.
    # TODO(maruel): This is not very ifficient when the caller is doing this in
    # a loop. Add a mechanism to have the caller handle this.
    flags = fcntl.fcntl(conn, fcntl.F_GETFL)
    if not conn.closed:
      # pylint: disable=E1101
      fcntl.fcntl(conn, fcntl.F_SETFL, flags | os.O_NONBLOCK)
    try:
      data = conn.read(max(maxsize or 16384, 1))
      return conns.index(conn), data
    finally:
      if not conn.closed:
        fcntl.fcntl(conn, fcntl.F_SETFL, flags)


class Popen(subprocess.Popen):
  """Adds timeout support on stdout and stderr.

  Inspired by
  http://code.activestate.com/recipes/440554-module-to-allow-asynchronous-subprocess-use-on-win/
  """
  def __init__(self, *args, **kwargs):
    self.start = time.time()
    self.end = None
    super(Popen, self).__init__(*args, **kwargs)

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

  def yield_any(self, timeout=None):
    """Yields output until the process terminates or is killed by a timeout.

    Yielded values are in the form (pipename, data).

    If timeout is None, it is blocking. If timeout is 0, it doesn't block. This
    is not generally useful to use timeout=0.
    """
    remaining = 0
    while self.poll() is None:
      if timeout:
        # While these float() calls seem redundant, they are to force
        # ResetableTimeout to "render" itself into a float. At each call, the
        # resulting value could be different, depending if a .reset() call
        # occurred.
        remaining = max(float(timeout) - self.duration(), 0.001)
      else:
        remaining = timeout
      t, data = self.recv_any(timeout=remaining)
      if data or timeout == 0:
        yield (t, data)
      if timeout and self.duration() >= float(timeout):
        break
    if self.poll() is None and timeout and self.duration() >= float(timeout):
      logging.debug('Kill %s %s', self.duration(), float(timeout))
      self.kill()
    self.wait()
    # Read all remaining output in the pipes.
    while True:
      t, data = self.recv_any()
      if not data:
        break
      yield (t, data)

  def recv_any(self, maxsize=None, timeout=None):
    """Reads from stderr and if empty, from stdout.

    If timeout is None, it is blocking. If timeout is 0, it doesn't block.
    """
    pipes = [
      x for x in ((self.stderr, 'stderr'), (self.stdout, 'stdout')) if x[0]
    ]
    if len(pipes) == 2 and self.stderr.fileno() == self.stdout.fileno():
      pipes.pop(0)
    if not pipes:
      return None, None
    conns, names = zip(*pipes)
    index, data = recv_multi_impl(conns, maxsize, timeout)
    if index is None:
      return index, data
    if not data:
      self._close(names[index])
      return None, None
    if self.universal_newlines:
      data = self._translate_newlines(data)
    return names[index], data

  def recv_out(self, maxsize=None, timeout=None):
    """Reads from stdout asynchronously."""
    return self._recv('stdout', maxsize, timeout)

  def recv_err(self, maxsize=None, timeout=None):
    """Reads from stderr asynchronously."""
    return self._recv('stderr', maxsize, timeout)

  def _close(self, which):
    getattr(self, which).close()
    setattr(self, which, None)

  def _recv(self, which, maxsize, timeout):
    conn = getattr(self, which)
    if conn is None:
      return None
    data = recv_multi_impl([conn], maxsize, timeout)
    if not data:
      return self._close(which)
    if self.universal_newlines:
      data = self._translate_newlines(data)
    return data


def call_with_timeout(cmd, timeout, **kwargs):
  """Runs an executable with an optional timeout.

  timeout 0 or None disables the timeout.
  """
  proc = Popen(
      cmd,
      stdin=subprocess.PIPE,
      stdout=subprocess.PIPE,
      **kwargs)
  if timeout:
    out = ''
    err = ''
    for t, data in proc.yield_any(timeout):
      if t == 'stdout':
        out += data
      else:
        err += data
  else:
    # This code path is much faster.
    out, err = proc.communicate()
  return out, err, proc.returncode, proc.duration()
