# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys
import unittest
import urllib2

from query_crbug import CheckIssueClosed

SRC = os.path.join(os.path.dirname(__file__), os.path.pardir, os.path.pardir)
sys.path.append(os.path.join(SRC, 'third_party', 'pymock'))

import mock

_current_directory = os.path.dirname(__file__)
_test_data_directory = os.path.join(_current_directory, 'test_data')

# These strings are simulated responses to various conditions when querying
# the chromium issue tracker.
CLOSED_ISSUE_DATA = open(os.path.join(_test_data_directory,
                                      'closed.json')).read()
OPEN_ISSUE_DATA = open(os.path.join(_test_data_directory,
                                    'open.json')).read()
UNEXPECTED_FORMAT_DATA = CLOSED_ISSUE_DATA.replace('issues$state', 'gibberish')
BROKEN_ISSUE_DATA = "\n<HTML><HEAD><TITLE>Not a JSON Doc</TITLE></HEAD></HTML>"


class mockResponse(object):
  def __init__(self, result):
    self._result = result

  def read(self):
    return self._result


def mockUrlOpen(url):
  # Note that these strings DO NOT represent http responses. They are just
  # memorable numeric bug ids to use.
  if '200' in url:
    return mockResponse(CLOSED_ISSUE_DATA)
  elif '201' in url:
    return mockResponse(OPEN_ISSUE_DATA)
  elif '300' in url:
    return mockResponse(UNEXPECTED_FORMAT_DATA)
  elif '403' in url:
    raise urllib2.URLError('')
  elif '404' in url:
    return mockResponse('')
  elif '500' in url:
    return mockResponse(BROKEN_ISSUE_DATA)


class crbugQueryTest(unittest.TestCase):
  @mock.patch('urllib2.urlopen',mockUrlOpen)
  def testClosedIssueIsClosed(self):
    self.assertTrue(CheckIssueClosed(200))

  @mock.patch('urllib2.urlopen',mockUrlOpen)
  def testOpenIssueIsNotClosed(self):
    self.assertFalse(CheckIssueClosed(201))

  @mock.patch('urllib2.urlopen',mockUrlOpen)
  def testUnexpectedFormat(self):
    self.assertFalse(CheckIssueClosed(300))

  @mock.patch('urllib2.urlopen',mockUrlOpen)
  def testUrlError(self):
    self.assertFalse(CheckIssueClosed(403))

  @mock.patch('urllib2.urlopen',mockUrlOpen)
  def testEmptyResponse(self):
    self.assertFalse(CheckIssueClosed(404))

  @mock.patch('urllib2.urlopen',mockUrlOpen)
  def testBrokenResponse(self):
    self.assertFalse(CheckIssueClosed(500))


if __name__ == '__main__':
  unittest.main()
