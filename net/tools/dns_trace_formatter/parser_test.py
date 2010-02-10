#!/usr/bin/python2.4
# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for the parser module."""

import parser
import unittest

class TestParser(unittest.TestCase):

    def testParseInvalidLine(self):
      """Should return None when fails to parse a line."""
      l = parser.ParseLine('''This is not a valid line!''')
      self.assertEqual(None, l)


    def testParseLine(self):
      """Tests parsing some valid lines"""
      l = parser.ParseLine(
          '''t=170099840:  "Received request r17 for {hostname='clients1.goog'''
          '''le.lv', port=80, priority=3, speculative=0, address_family=0, '''
          '''allow_cached=1, referrer=''}"''')
      self.assertEqual(parser.LINE_TYPE_RECEIVED_REQUEST, l.line_type)
      self.assertEqual(170099840, l.t)
      self.assertEqual('r17', l.entity_id)

      l = parser.ParseLine(
          '''t=170099840:  "Created job j15 for {hostname='clients1.go'''
          '''ogle.lv', address_family=0}"''')
      self.assertEqual(parser.LINE_TYPE_CREATED_JOB, l.line_type)
      self.assertEqual(170099840, l.t)
      self.assertEqual('j15', l.entity_id)

      l = parser.ParseLine('t=170099840:  "Attached request r17 to job j15"')
      self.assertEqual(parser.LINE_TYPE_ATTACHED_REQUEST, l.line_type)
      self.assertEqual(170099840, l.t)
      self.assertEqual('r17', l.entity_id)

      l = parser.ParseLine('t=170103144:  "Finished request r18 with error=0"')
      self.assertEqual(parser.LINE_TYPE_FINISHED_REQUEST, l.line_type)
      self.assertEqual(170103144, l.t)
      self.assertEqual('r18', l.entity_id)

      l = parser.ParseLine('t=170103461:  "Starting job j16"')
      self.assertEqual(parser.LINE_TYPE_STARTING_JOB, l.line_type)
      self.assertEqual(170103461, l.t)
      self.assertEqual('j16', l.entity_id)

      l = parser.ParseLine('t=170103461:  "[resolver thread] Running job j1"')
      self.assertEqual(parser.LINE_TYPE_RUNNING_JOB, l.line_type)
      self.assertEqual(170103461, l.t)
      self.assertEqual('j1', l.entity_id)

      l = parser.ParseLine('t=170110496:  "[resolver thread] Completed job j6"')
      self.assertEqual(parser.LINE_TYPE_COMPLETED_JOB, l.line_type)
      self.assertEqual(170110496, l.t)
      self.assertEqual('j6', l.entity_id)

      l = parser.ParseLine('t=170110496:  "Completing job j4"')
      self.assertEqual(parser.LINE_TYPE_COMPLETING_JOB, l.line_type)
      self.assertEqual(170110496, l.t)
      self.assertEqual('j4', l.entity_id)

      l = parser.ParseLine('t=170110496:  "Cancelled request r9"')
      self.assertEqual(parser.LINE_TYPE_CANCELLED_REQUEST, l.line_type)
      self.assertEqual(170110496, l.t)
      self.assertEqual('r9', l.entity_id)


if __name__ == '__main__':
    unittest.main()
