# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Classes and functions related to threading."""

import inspect
import logging
import os
import Queue
import sys
import threading
import time
import traceback


class ThreadPoolError(Exception):
  """Base class for exceptions raised by ThreadPool."""
  pass


class ThreadPoolEmpty(ThreadPoolError):
  """Trying to get task result from a thread pool with no pending tasks."""
  pass


class ThreadPoolClosed(ThreadPoolError):
  """Trying to do something with a closed thread pool."""
  pass


class ThreadPool(object):
  """Implements a multithreaded worker pool oriented for mapping jobs with
  thread-local result storage.

  Arguments:
  - initial_threads: Number of threads to start immediately. Can be 0 if it is
    uncertain that threads will be needed.
  - max_threads: Maximum number of threads that will be started when all the
                 threads are busy working. Often the number of CPU cores.
  - queue_size: Maximum number of tasks to buffer in the queue. 0 for unlimited
                queue. A non-zero value may make add_task() blocking.
  - prefix: Prefix to use for thread names. Pool's threads will be
            named '<prefix>-<thread index>'.
  """
  QUEUE_CLASS = Queue.PriorityQueue

  def __init__(self, initial_threads, max_threads, queue_size, prefix=None):
    prefix = prefix or 'tp-0x%0x' % id(self)
    logging.debug(
        'New ThreadPool(%d, %d, %d): %s', initial_threads, max_threads,
        queue_size, prefix)
    assert initial_threads <= max_threads
    # Update this check once 256 cores CPU are common.
    assert max_threads <= 256

    self.tasks = self.QUEUE_CLASS(queue_size)
    self._max_threads = max_threads
    self._prefix = prefix

    # Used to assign indexes to tasks.
    self._num_of_added_tasks_lock = threading.Lock()
    self._num_of_added_tasks = 0

    # Lock that protected everything below (including conditional variable).
    self._lock = threading.Lock()

    # Condition 'bool(_outputs) or bool(_exceptions) or _pending_count == 0'.
    self._outputs_exceptions_cond = threading.Condition(self._lock)
    self._outputs = []
    self._exceptions = []

    # Number of pending tasks (queued or being processed now).
    self._pending_count = 0

    # List of threads.
    self._workers = []
    # Number of threads that are waiting for new tasks.
    self._ready = 0
    # Number of threads already added to _workers, but not yet running the loop.
    self._starting = 0
    # True if close was called. Forbids adding new tasks.
    self._is_closed = False

    for _ in range(initial_threads):
      self._add_worker()

  def _add_worker(self):
    """Adds one worker thread if there isn't too many. Thread-safe."""
    with self._lock:
      if len(self._workers) >= self._max_threads or self._is_closed:
        return False
      worker = threading.Thread(
        name='%s-%d' % (self._prefix, len(self._workers)), target=self._run)
      self._workers.append(worker)
      self._starting += 1
    logging.debug('Starting worker thread %s', worker.name)
    worker.daemon = True
    worker.start()
    return True

  def add_task(self, priority, func, *args, **kwargs):
    """Adds a task, a function to be executed by a worker.

    |priority| can adjust the priority of the task versus others. Lower priority
    takes precedence.

    |func| can either return a return value to be added to the output list or
    be a generator which can emit multiple values.

    Returns the index of the item added, e.g. the total number of enqueued items
    up to now.
    """
    assert isinstance(priority, int)
    assert callable(func)
    with self._lock:
      if self._is_closed:
        raise ThreadPoolClosed('Can not add a task to a closed ThreadPool')
      start_new_worker = (
        # Pending task count plus new task > number of available workers.
        self.tasks.qsize() + 1 > self._ready + self._starting and
        # Enough slots.
        len(self._workers) < self._max_threads
      )
      self._pending_count += 1
    with self._num_of_added_tasks_lock:
      self._num_of_added_tasks += 1
      index = self._num_of_added_tasks
    self.tasks.put((priority, index, func, args, kwargs))
    if start_new_worker:
      self._add_worker()
    return index

  def _run(self):
    """Worker thread loop. Runs until a None task is queued."""
    # Thread has started, adjust counters.
    with self._lock:
      self._starting -= 1
      self._ready += 1
    while True:
      try:
        task = self.tasks.get()
      finally:
        with self._lock:
          self._ready -= 1
      try:
        if task is None:
          # We're done.
          return
        _priority, _index, func, args, kwargs = task
        if inspect.isgeneratorfunction(func):
          for out in func(*args, **kwargs):
            self._output_append(out)
        else:
          out = func(*args, **kwargs)
          self._output_append(out)
      except Exception as e:
        logging.warning('Caught exception: %s', e)
        exc_info = sys.exc_info()
        logging.info(''.join(traceback.format_tb(exc_info[2])))
        with self._outputs_exceptions_cond:
          self._exceptions.append(exc_info)
          self._outputs_exceptions_cond.notifyAll()
      finally:
        try:
          # Mark thread as ready again, mark task as processed. Do it before
          # waking up threads waiting on self.tasks.join(). Otherwise they might
          # find ThreadPool still 'busy' and perform unnecessary wait on CV.
          with self._outputs_exceptions_cond:
            self._ready += 1
            self._pending_count -= 1
            if self._pending_count == 0:
              self._outputs_exceptions_cond.notifyAll()
          self.tasks.task_done()
        except Exception as e:
          # We need to catch and log this error here because this is the root
          # function for the thread, nothing higher will catch the error.
          logging.exception('Caught exception while marking task as done: %s',
                            e)

  def _output_append(self, out):
    if out is not None:
      with self._outputs_exceptions_cond:
        self._outputs.append(out)
        self._outputs_exceptions_cond.notifyAll()

  def join(self):
    """Extracts all the results from each threads unordered.

    Call repeatedly to extract all the exceptions if desired.

    Note: will wait for all work items to be done before returning an exception.
    To get an exception early, use get_one_result().
    """
    # TODO(maruel): Stop waiting as soon as an exception is caught.
    self.tasks.join()
    with self._outputs_exceptions_cond:
      if self._exceptions:
        e = self._exceptions.pop(0)
        raise e[0], e[1], e[2]
      out = self._outputs
      self._outputs = []
    return out

  def get_one_result(self):
    """Returns the next item that was generated or raises an exception if one
    occurred.

    Raises:
      ThreadPoolEmpty - no results available.
    """
    # Get first available result.
    for result in self.iter_results():
      return result
    # No results -> tasks queue is empty.
    raise ThreadPoolEmpty('Task queue is empty')

  def iter_results(self):
    """Yields results as they appear until all tasks are processed."""
    while True:
      # Check for pending results.
      result = None
      with self._outputs_exceptions_cond:
        if self._exceptions:
          e = self._exceptions.pop(0)
          raise e[0], e[1], e[2]
        if self._outputs:
          # Remember the result to yield it outside of the lock.
          result = self._outputs.pop(0)
        else:
          # No pending tasks -> all tasks are done.
          if not self._pending_count:
            return
          # Some task is queued, wait for its result to appear.
          # Use non-None timeout so that process reacts to Ctrl+C and other
          # signals, see http://bugs.python.org/issue8844.
          self._outputs_exceptions_cond.wait(timeout=5)
          continue
      yield result

  def close(self):
    """Closes all the threads."""
    # Ensure no new threads can be started, self._workers is effectively
    # a constant after that and can be accessed outside the lock.
    with self._lock:
      if self._is_closed:
        raise ThreadPoolClosed('Can not close already closed ThreadPool')
      self._is_closed = True
    for _ in range(len(self._workers)):
      # Enqueueing None causes the worker to stop.
      self.tasks.put(None)
    for t in self._workers:
      t.join()
    logging.debug(
      'Thread pool \'%s\' closed: spawned %d threads total',
      self._prefix, len(self._workers))

  def __enter__(self):
    """Enables 'with' statement."""
    return self

  def __exit__(self, _exc_type, _exc_value, _traceback):
    """Enables 'with' statement."""
    self.close()


class Progress(object):
  """Prints progress and accepts updates thread-safely."""
  def __init__(self, size):
    # To be used in the primary thread
    self.last_printed_line = ''
    self.index = 0
    self.start = time.time()
    self.size = size
    self.use_cr_only = True
    self.unfinished_commands = set()

    # To be used in all threads.
    self.queued_lines = Queue.Queue()

  def update_item(self, name, index=True, size=False):
    """Queue information to print out.

    |index| notes that the index should be incremented.
    |size| note that the total size should be incremented.
    """
    self.queued_lines.put((name, index, size))

  def print_update(self):
    """Prints the current status."""
    # Flush all the logging output so it doesn't appear within this output.
    for handler in logging.root.handlers:
      handler.flush()

    while True:
      try:
        name, index, size = self.queued_lines.get_nowait()
      except Queue.Empty:
        break

      if size:
        self.size += 1
      if index:
        self.index += 1
      if not name:
        continue

      if index:
        alignment = str(len(str(self.size)))
        next_line = ('[%' + alignment + 'd/%d] %6.2fs %s') % (
            self.index,
            self.size,
            time.time() - self.start,
            name)
        # Fill it with whitespace only if self.use_cr_only is set.
        prefix = ''
        if self.use_cr_only:
          if self.last_printed_line:
            prefix = '\r'
        if self.use_cr_only:
          suffix = ' ' * max(0, len(self.last_printed_line) - len(next_line))
        else:
          suffix = '\n'
        line = '%s%s%s' % (prefix, next_line, suffix)
        self.last_printed_line = next_line
      else:
        line = '\n%s\n' % name.strip('\n')
        self.last_printed_line = ''

      sys.stdout.write(line)

    # Ensure that all the output is flush to prevent it from getting mixed with
    # other output streams (like the logging streams).
    sys.stdout.flush()

    if self.unfinished_commands:
      logging.debug('Waiting for the following commands to finish:\n%s',
                    '\n'.join(self.unfinished_commands))


class QueueWithProgress(Queue.PriorityQueue):
  """Implements progress support in join()."""
  def __init__(self, maxsize, *args, **kwargs):
    Queue.PriorityQueue.__init__(self, *args, **kwargs)
    self.progress = Progress(maxsize)

  def set_progress(self, progress):
    """Replace the current progress, mainly used when a progress should be
    shared between queues."""
    self.progress = progress

  def task_done(self):
    """Contrary to Queue.task_done(), it wakes self.all_tasks_done at each task
    done.
    """
    with self.all_tasks_done:
      try:
        unfinished = self.unfinished_tasks - 1
        if unfinished < 0:
          raise ValueError('task_done() called too many times')
        self.unfinished_tasks = unfinished
        # This is less efficient, because we want the Progress to be updated.
        self.all_tasks_done.notify_all()
      except Exception as e:
        logging.exception('task_done threw an exception.\n%s', e)

  def wake_up(self):
    """Wakes up all_tasks_done.

    Unlike task_done(), do not substract one from self.unfinished_tasks.
    """
    # TODO(maruel): This is highly inefficient, since the listener is awaken
    # twice; once per output, once per task. There should be no relationship
    # between the number of output and the number of input task.
    with self.all_tasks_done:
      self.all_tasks_done.notify_all()

  def join(self):
    """Calls print_update() whenever possible."""
    self.progress.print_update()
    with self.all_tasks_done:
      while self.unfinished_tasks:
        self.progress.print_update()
        self.all_tasks_done.wait(60.)
      self.progress.print_update()


class ThreadPoolWithProgress(ThreadPool):
  QUEUE_CLASS = QueueWithProgress

  def __init__(self, progress, *args, **kwargs):
    super(ThreadPoolWithProgress, self).__init__(*args, **kwargs)
    self.tasks.set_progress(progress)

  def _output_append(self, out):
    """Also wakes up the listener on new completed test_case."""
    super(ThreadPoolWithProgress, self)._output_append(out)
    self.tasks.wake_up()


class DeadlockDetector(object):
  """Context manager that can detect deadlocks.

  It will dump stack frames of all running threads if its 'ping' method isn't
  called in time.

  Usage:
    with DeadlockDetector(timeout=60) as detector:
      for item in some_work():
        ...
        detector.ping()
        ...

  Arguments:
    timeout - maximum allowed time between calls to 'ping'.
  """

  def __init__(self, timeout):
    self.timeout = timeout
    self._thread = None
    # Thread stop condition. Also lock for shared variables below.
    self._stop_cv = threading.Condition()
    self._stop_flag = False
    # Time when 'ping' was called last time.
    self._last_ping = None
    # True if pings are coming on time.
    self._alive = True

  def __enter__(self):
    """Starts internal watcher thread."""
    assert self._thread is None
    self.ping()
    self._thread = threading.Thread(name='deadlock-detector', target=self._run)
    self._thread.daemon = True
    self._thread.start()
    return self

  def __exit__(self, *_args):
    """Stops internal watcher thread."""
    assert self._thread is not None
    with self._stop_cv:
      self._stop_flag = True
      self._stop_cv.notify()
    self._thread.join()
    self._thread = None
    self._stop_flag = False

  def ping(self):
    """Notify detector that main thread is still running.

    Should be called periodically to inform the detector that everything is
    running as it should.
    """
    with self._stop_cv:
      self._last_ping = time.time()
      self._alive = True

  def _run(self):
    """Loop that watches for pings and dumps threads state if ping is late."""
    with self._stop_cv:
      while not self._stop_flag:
        # Skipped deadline? Dump threads and switch to 'not alive' state.
        if self._alive and time.time() > self._last_ping + self.timeout:
          self.dump_threads(time.time() - self._last_ping, True)
          self._alive = False

        # Pings are on time?
        if self._alive:
          # Wait until the moment we need to dump stack traces.
          # Most probably some other thread will call 'ping' to move deadline
          # further in time. We don't bother to wake up after each 'ping',
          # only right before initial expected deadline.
          self._stop_cv.wait(self._last_ping + self.timeout - time.time())
        else:
          # Skipped some pings previously. Just periodically silently check
          # for new pings with some arbitrary frequency.
          self._stop_cv.wait(self.timeout * 0.1)

  @staticmethod
  def dump_threads(timeout=None, skip_current_thread=False):
    """Dumps stack frames of all running threads."""
    all_threads = threading.enumerate()
    current_thread_id = threading.current_thread().ident

    # Collect tracebacks: thread name -> traceback string.
    tracebacks = {}

    # pylint: disable=W0212
    for thread_id, frame in sys._current_frames().iteritems():
      # Don't dump deadlock detector's own thread, it's boring.
      if thread_id == current_thread_id and not skip_current_thread:
        continue

      # Try to get more informative symbolic thread name.
      name = 'untitled'
      for thread in all_threads:
        if thread.ident == thread_id:
          name = thread.name
          break
      name += ' #%d' % (thread_id,)
      tracebacks[name] = ''.join(traceback.format_stack(frame))

    # Function to print a message. Makes it easier to change output destination.
    def output(msg):
      logging.warning(msg.rstrip())

    # Print tracebacks, sorting them by thread name. That way a thread pool's
    # threads will be printed as one group.
    output('=============== Potential deadlock detected ===============')
    if timeout is not None:
      output('No pings in last %d sec.' % (timeout,))
    output('Dumping stack frames for all threads:')
    for name in sorted(tracebacks):
      output('Traceback for \'%s\':\n%s' % (name, tracebacks[name]))
    output('===========================================================')


class Bit(object):
  """Thread safe setable bit."""

  def __init__(self):
    self._lock = threading.Lock()
    self._value = False

  def get(self):
    with self._lock:
      return self._value

  def set(self):
    with self._lock:
      self._value = True


def num_processors():
  """Returns the number of processors.

  Python on OSX 10.6 raises a NotImplementedError exception.
  """
  try:
    # Multiprocessing
    import multiprocessing
    return multiprocessing.cpu_count()
  except:  # pylint: disable=W0702
    try:
      # Mac OS 10.6
      return int(os.sysconf('SC_NPROCESSORS_ONLN'))  # pylint: disable=E1101
    except:
      # Some of the windows builders seem to get here.
      return 4
