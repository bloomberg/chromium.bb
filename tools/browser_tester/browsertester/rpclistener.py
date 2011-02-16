#!/usr/bin/python
# Copyright 2010 The Native Client Authors.  All rights reserved.
# Use of this source code is governed by a BSD-style license that can
# be found in the LICENSE file.


class RPCListener(object):

  def __init__(self, shutdown_callback):
    self.shutdown_callback = shutdown_callback
    self.prefix = '**** '
    self.ever_failed = False
    self.ResetStatus()

  def ResetStatus(self):
    # NOTE this does not reset ever_failed because it should never be reset
    self.passed = 0
    self.failed = 0
    self.errors = 0

  def Log(self, message):
    print '%s%s\n' % (self.prefix, message),

  def NewLine(self):
    self.Log('')

  def Startup(self):
    self.ResetStatus()
    self.Log('[STARTUP]')
    self.NewLine()
    return 'OK'

  def TestBegin(self, test_name):
    self.Log('[%s BEGIN]' % (test_name,))
    return 'OK'

  def TestLog(self, test_name, message):
    self.Log('[%s LOG] %s' % (test_name, message))
    return 'OK'

  def TestPassed(self, test_name):
    self.passed += 1
    self.Log('[%s PASSED]' % (test_name,))
    self.NewLine()
    return 'OK'

  # The test decided it failed
  def TestFailed(self, test_name, message):
    self._TestFailed()
    self.Log('[%s FAILED] %s' % (test_name, message))
    self.NewLine()
    return 'OK'

  # A recognizable exception
  def TestException(self, test_name, message, stack):
    self._TestFailed()
    self.Log('[%s EXCEPTION] %s' % (test_name, message.rstrip()))
    for line in stack.rstrip().split('\n'):
      self.Log('    ' + line)
    self.NewLine()
    return 'OK'

  # An unrecognizable exception
  def TestError(self, test_name, message):
    self._TestFailed()
    self.Log('[%s ERROR] %s' % (test_name, message))
    self.NewLine()
    return 'OK'

  # Something went wrong on the client side, the test is probably screwed up
  def ClientError(self, message):
    self._ExternalError()
    self.Log('[CLIENT_ERROR] %s' % (message,))
    self.NewLine()
    # Keep running... see what else the client says.
    return 'OK'

  # Something went very wrong on the server side, everything is horked?
  def ServerError(self, message):
    self._ExternalError()
    self.Log('[SERVER_ERROR] %s' % (message,))
    self._TestingDone()
    return 'OK'

  def Shutdown(self):
    self.Log('[SHUTDOWN] %s' % (self.StatusString(),))
    self._TestingDone()
    return 'OK'

  def _TestFailed(self):
    self.failed += 1
    self.ever_failed = True

  def _ExternalError(self):
    self.errors += 1
    self.ever_failed = True

  def StatusString(self):
    if self.errors:
      return '%d passed, %d failed, %d errors' % (self.passed, self.failed,
                                                  self.errors)
    else:
      return '%d passed, %d failed' % (self.passed, self.failed)

  def _TestingDone(self):
    self.shutdown_callback()