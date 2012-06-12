# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Implements a multithreaded worker pool oriented for mapping jobs with
thread-local result storage.
"""

import Queue
import sys
import time
import threading


class QueueWithTimeout(Queue.Queue):
  """Implements timeout support in join()."""

  # QueueWithTimeout.join: Arguments number differs from overridden method
  # pylint: disable=W0221
  def join(self, timeout=None):
    """Returns True if all tasks are finished."""
    if not timeout:
      return Queue.Queue.join(self)
    start = time.time()
    self.all_tasks_done.acquire()
    try:
      while self.unfinished_tasks:
        remaining = time.time() - start - timeout
        if remaining <= 0:
          break
        self.all_tasks_done.wait(remaining)
      return not self.unfinished_tasks
    finally:
      self.all_tasks_done.release()


class WorkerThread(threading.Thread):
  """Keeps the results of each task in a thread-local outputs variable."""
  def __init__(self, tasks, *args, **kwargs):
    super(WorkerThread, self).__init__(*args, **kwargs)
    self._tasks = tasks
    self.outputs = []

    self.daemon = True
    self.start()

  def run(self):
    """Runs until a None task is queued."""
    while True:
      task = self._tasks.get()
      if task is None:
        # We're done.
        return
      try:
        func, args, kwargs = task
        self.outputs.append(func(*args, **kwargs))
      except Exception, e:
        self.outputs.append(e)
      finally:
        self._tasks.task_done()


class ThreadPool(object):
  def __init__(self, num_threads):
    self._tasks = QueueWithTimeout()
    self._workers = [
      WorkerThread(self._tasks, name='worker-%d' % i)
      for i in range(num_threads)
    ]

  def add_task(self, func, *args, **kwargs):
    """Adds a task, a function to be executed by a worker.

    The function's return value will be stored in the the worker's thread local
    outputs list.
    """
    self._tasks.put((func, args, kwargs))

  def join(self, progress=None, timeout=None):
    """Extracts all the results from each threads unordered."""
    if progress and timeout:
      while not self._tasks.join(timeout):
        progress.print_update()
    else:
      self._tasks.join()
    out = []
    for w in self._workers:
      out.extend(w.outputs)
      w.outputs = []
    return out

  def close(self):
    """Closes all the threads."""
    for _ in range(len(self._workers)):
      # Enqueueing None causes the worker to stop.
      self._tasks.put(None)
    for t in self._workers:
      t.join()

  def __enter__(self):
    """Enables 'with' statement."""
    return self

  def __exit__(self, exc_type, exc_value, traceback):
    """Enables 'with' statement."""
    self.close()


class Progress(object):
  """Prints progress and accepts updates thread-safely."""
  def __init__(self, size):
    self.last_printed_line = ''
    self.next_line = ''
    self.index = -1
    self.size = size
    self.start = time.time()
    self.lock = threading.Lock()
    self.update_item('')

  def update_item(self, name):
    with self.lock:
      self.index += 1
      self.next_line = '%d of %d (%.1f%%), %.1fs: %s' % (
            self.index,
            self.size,
            self.index * 100. / self.size,
            time.time() - self.start,
            name)

  def print_update(self):
    """Prints the current status."""
    with self.lock:
      if self.next_line == self.last_printed_line:
        return
      line = '\r%s%s' % (
          self.next_line,
          ' ' * max(0, len(self.last_printed_line) - len(self.next_line)))
      self.last_printed_line = self.next_line
    sys.stderr.write(line)

  def increase_count(self):
    with self.lock:
      self.size += 1
