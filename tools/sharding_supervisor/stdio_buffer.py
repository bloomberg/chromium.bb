# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Syncronized Standard IO Linebuffer implemented with cStringIO."""

import cStringIO
import os
import sys
import threading
import Queue


class StdioBuffer(object):
  def __init__(self, shard):
    self.line_ready_event = threading.Event()
    self.queue = Queue.Queue()
    self.lock = threading.Lock()
    self.completed = 0
    self.shard = shard

  def _pipe_handler(self, system_pipe, program_pipe):
    """Helper method for collecting stdio output.  Output is collected until
    a newline is seen, at which point an event is triggered and the line is
    pushed to a buffer as a (stdio, line) tuple."""
    buffer = cStringIO.StringIO()
    pipe_running = True
    while pipe_running:
      char = program_pipe.read(1)
      if not char and self.shard.poll() is not None:
        pipe_running = False
        self.line_ready_event.set()
      buffer.write(char)
      if char == '\n' or not pipe_running:
        line = buffer.getvalue()
        if not line and not pipe_running:
          with self.lock:
            self.completed += 1
            self.line_ready_event.set()
          break
        self.queue.put((system_pipe, line))
        self.line_ready_event.set()
        buffer.close()
        buffer = cStringIO.StringIO()

  def handle_pipe(self, system_pipe, program_pipe):
    t = threading.Thread(target=self._pipe_handler, args=[system_pipe,
                                                          program_pipe])
    t.start()
    return t

  def readline(self):
    """Emits a tuple of (sys.stderr, line) or (sys.stdout, line).  This is a
    blocking call."""
    while self.completed < 2 and self.queue.empty():
      self.line_ready_event.wait()
      self.line_ready_event.clear()
    if not self.queue.empty():
      return self.queue.get_nowait()
    else:
      return (None, None)