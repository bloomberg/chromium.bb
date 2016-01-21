# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Task-based unittest for the Legion event server."""

import argparse
import httplib
import sys
import unittest


class EventServerTest(unittest.TestCase):

  def __init__(self, *args, **kwargs):
    super(EventServerTest, self).__init__(*args, **kwargs)

    parser = argparse.ArgumentParser()
    parser.add_argument('--address')
    parser.add_argument('--port', type=int)
    self.args, _ = parser.parse_known_args()

  def Connect(self, verb, path):
    conn = httplib.HTTPConnection(self.args.address, self.args.port)
    conn.request(verb, path)
    return conn.getresponse().status

  def testSettingGettingAndClearingAnEvent(self):
    self.assertEquals(self.Connect('GET', '/events/event1'), 404)
    self.assertEquals(self.Connect('PUT', '/events/event1'), 200)
    self.assertEquals(self.Connect('GET', '/events/event1'), 200)
    self.assertEquals(self.Connect('GET', '/events/event2'), 404)
    self.assertEquals(self.Connect('DELETE', '/events/event1'), 200)
    self.assertEquals(self.Connect('DELETE', '/events/event1'), 404)
    self.assertEquals(self.Connect('GET', '/events/event1'), 404)

  def testErrors(self):
    for verb in ['GET', 'PUT', 'DELETE']:
      self.assertEquals(self.Connect(verb, '/'), 403)
      self.assertEquals(self.Connect(verb, '/foobar'), 405)
      self.assertEquals(self.Connect(verb, '/events'), 405)


if __name__ == '__main__':
  unittest.main(argv=sys.argv[:1])
