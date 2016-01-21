# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittest for legion.lib.event_server."""

import httplib

# pylint: disable=relative-import
import legion_unittest

from legion.lib import event_server


class EventServerTest(legion_unittest.TestCase):

  def setUp(self):
    super(EventServerTest, self).setUp()
    self.server = event_server.ThreadedServer()
    self.server.start()

  def tearDown(self):
    try:
      self.server.shutdown()
    finally:
      super(EventServerTest, self).tearDown()

  def Connect(self, verb, path):
    conn = httplib.HTTPConnection('localhost', self.server.port)
    conn.request(verb, path)
    return conn.getresponse().status

  def testSettingGettingAndClearingAnEvent(self):
    self.assertEquals(self.Connect('GET', '/events/event1'), 404)
    self.assertEquals(self.Connect('PUT', '/events/event1'), 200)
    self.assertEquals(self.Connect('GET', '/events/event1'), 200)
    self.assertEquals(self.Connect('DELETE', '/events/event1'), 200)
    self.assertEquals(self.Connect('DELETE', '/events/event1'), 404)
    self.assertEquals(self.Connect('GET', '/events/event1'), 404)

  def testErrors(self):
    for verb in ['GET', 'PUT', 'DELETE']:
      self.assertEquals(self.Connect(verb, '/'), 403)
      self.assertEquals(self.Connect(verb, '/foobar'), 405)
      self.assertEquals(self.Connect(verb, '/events'), 405)


if __name__ == '__main__':
  legion_unittest.main()
