#!/usr/bin/python
# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import functools
import logging
import os
import signal
import sys

# We want to use correct version of libraries even when executed through symlink.
path = os.path.realpath(__file__)
path = os.path.normpath(os.path.join(os.path.dirname(path), '..', '..'))
sys.path.insert(0, path)
del path


def FindTarget(name):
  target = os.path.basename(sys.argv[0])
  # Compatibility for badly named scripts that can't yet be fixed.
  if target.endswith('.py'):
    target = target[:-3]
  target = 'chromite.scripts.%s' % target
  module = __import__(target)
  # __import__ gets us the root of the namespace import; walk our way up.
  for attr in target.split('.')[1:]:
    module = getattr(module, attr)

  if hasattr(module, 'main'):
    # This might seem pointless using functools here; we do it since down the
    # line, the allowed 'main' prototypes will change.  Thus we define the
    # FindTarget api to just return an invokable, allowing the consumer
    # to not know nor care about the specifics.
    return functools.partial(module.main, sys.argv[1:])
  return None


class _ShutDownException(SystemExit):

  def __init__(self, signal, message):
    self.signal = signal
    # Setup a usage mesage primarily for any code that may intercept it
    # while this exception is crashing back up the stack to us.
    SystemExit.__init__(self, message)


def _DefaultHandler(signum, frame):
  # Don't double process sigterms; just trigger shutdown from the first
  # exception.
  signal.signal(signum, signal.SIG_IGN)
  raise _ShutDownException(
      signum, "Received signal %i; shutting down" % (signum,))


if __name__ == '__main__':
  name = os.path.basename(sys.argv[0])
  target = FindTarget(name)
  if target is None:
    print >>sys.stderr, ("Internal error detected in wrapper.py: no main "
                         "functor found in module %r." % (target,))
    sys.exit(100)

  # Set up basic logging information for all modules that use logging.
  # Note a script target may setup default logging in it's module namespace
  # which will take precedence over this.
  logging.basicConfig(
      level=logging.DEBUG,
      format='%(levelname)s: %(asctime)s: %(message)s',
      datefmt='%H:%M:%S')


  signal.signal(signal.SIGTERM, _DefaultHandler)

  ret = 1
  try:
    ret = target()
  except _ShutDownException, e:
    print >>sys.stderr, ("%s: Signaled to shutdown: caught %i signal." %
                         (name, e.signal,))
  except SystemExit, e:
    # Right now, let this crash through- longer term, we'll update the scripts
    # in question to not use sys.exit, and make this into a flagged error.
    raise
  except Exception, e:
    print >>sys.stderr, ("%s: Unhandled exception:" % (name,))
    raise

  if ret is None:
    ret = 0
  sys.exit(ret)
