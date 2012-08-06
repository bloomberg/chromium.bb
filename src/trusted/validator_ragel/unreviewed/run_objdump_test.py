#!/usr/bin/python
# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Checks that known instructions are disassebled identically to "objdump -d".

The single-threaded generator produces byte sequences of all instructions it
knows about and emits them in portions, file by file.  Process these files in
parallel.
"""

import optparse
import os
import signal
import subprocess
import sys
import threading
import multiprocessing
import time


CV = None  # condition variable for all wait/resume
CAPACITY = 8


class TestGenerator(threading.Thread):
  """Runs the generator, produces names of the files to run tests on."""

  def __init__(self, gen_cmd):
    threading.Thread.__init__(self)
    self.generator_command = gen_cmd
    self.generated_filenames = []
    self.is_eof = False
    self.proc = None

  def run(self):
    """Updates generated_filenames as soon as filenames arrive.

    Starts the generator, receives filenames from its stdout as soon as they
    appear, notifies workers to take them.  Once over capacity, stops
    the generator to continue later.
    """
    self.proc = subprocess.Popen(self.generator_command, stdout=subprocess.PIPE)
    while True:
      CV.acquire()
      try:
        while len(self.generated_filenames) >= CAPACITY:
          self.proc.send_signal(signal.SIGSTOP)
          CV.wait()
          self.proc.send_signal(signal.SIGCONT)
      except OSError, err:
        # OS Error with errno == 3 (ESRCH) means "no such process". The process
        # has exited, ignore the error.
        if err.errno != 3:
          print >> sys.stderr, ('error: Unknown OSError while trying to ' +
              'stop the generator: {0}'.format(err))
          os._exit(4)
      line = self.proc.stdout.readline()
      if line == '':
        self.is_eof = True
        CV.notifyAll()
        CV.release()
        break
      self.generated_filenames.append(line.rstrip())
      CV.notifyAll()
      CV.release()
      # Yield to give Workers an opportunity to run.
      time.sleep(0.000001)

  def NextAvailable(self):
    """Indicates whether a worker has a file to take.

    If the generator is finished and there is no file left in the queue, a
    worker should consume an empty value and return silently.  Must be run under
    a lock.
    """
    if self.generated_filenames or self.is_eof:
      return True
    return False

  def TakeNext(self):
    """Pops the next filename from the queue."""

    if self.generated_filenames:
      ret = self.generated_filenames.pop(0)
      return ret
    if self.is_eof:
      return None

  def Cleanup(self):
    if self.proc:
      self.proc.kill()


class Worker(threading.Thread):
  """Takes filenames from the queue and processes them until empty is seen.

  Kills all testing once a single failure report is observed.
  """

  def __init__(self, gen, option_parser):
    threading.Thread.__init__(self)
    self.generator = gen
    self.opt = option_parser

  def run(self):
    while True:
      CV.acquire()
      while not self.generator.NextAvailable():
        CV.wait()
      f = self.generator.TakeNext()
      CV.notifyAll()
      CV.release()
      if not f:
        return
      self.ProcessFile(f)

  def ProcessFile(self, filename):
    pid = os.fork()
    if pid == 0:
      os.execl(self.opt.tester, self.opt.tester,
               'GAS="' + self.opt.gas_path + '"',
               'OBJDUMP="' + self.opt.objdump_path + '"',
               'DECODER="' + self.opt.decoder_path + '"',
               'ASMFILE="' + filename + '"')

    else:
      (child_pid, retcode) = os.wait()
    if retcode != 0:
      print >> sys.stderr, ('error: return code 0 expected from tester, ' +
          'got {0} instead'.format(retcode))
      self.ExitFail()
    print '{0}..done'.format(filename)

  def ExitFail(self):
    self.generator.Cleanup()
    os._exit(3)


def Main():
  parser = optparse.OptionParser()
  parser.add_option(
      '-a', '--gas', dest='gas_path',
      default=None,
      help='path to find the assembler binary')
  parser.add_option(
      '-o', '--objdump', dest='objdump_path',
      default=None,
      help='path to find the objdump binary')
  parser.add_option(
      '-d', '--decoder', dest='decoder_path',
      default=None,
      help='path to find the tested decoder')
  parser.add_option(
      '-n', '--nthreads', dest='nthreads',
      default=0,
      type=int,
      help='amount of threads to use for running decoders')
  parser.add_option(
      '-t', '--tester', dest='tester',
      default=None,
      help='script to test an individual file')
  opt, generator_command = parser.parse_args()

  if (not opt.gas_path or
      not opt.objdump_path or
      not opt.decoder_path or
      not opt.tester):
    parser.error('invalid arguments')

  if opt.nthreads == 0:
    opt.nthreads = multiprocessing.cpu_count()
  assert opt.nthreads > 0

  global CV
  CV = threading.Condition()
  gen = TestGenerator(generator_command)

  # Start all threads.
  workers = []
  for i in xrange(opt.nthreads):
    w = Worker(gen, opt)
    workers.append(w)
    w.start()
  gen.start()

  # Wait for all threads to finish.
  gen.join()
  for w in workers:
    w.join()
  return 0


if __name__ == '__main__':
  sys.exit(Main())
