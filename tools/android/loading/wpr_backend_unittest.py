# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

from wpr_backend import WprUrlEntry


class MockWprResponse(object):
  def __init__(self, headers):
    self.headers = headers

class WprUrlEntryTest(unittest.TestCase):

  @classmethod
  def _CreateWprUrlEntry(cls, headers):
    wpr_response = MockWprResponse(headers)
    return WprUrlEntry('GET http://a.com/', wpr_response)

  def test_ExtractUrl(self):
    self.assertEquals('http://aa.bb/c',
                      WprUrlEntry._ExtractUrl('GET http://aa.bb/c'))
    self.assertEquals('http://aa.b/c',
                      WprUrlEntry._ExtractUrl('POST http://aa.b/c'))
    self.assertEquals('http://a.bb/c',
                      WprUrlEntry._ExtractUrl('WHATEVER http://a.bb/c'))
    self.assertEquals('https://aa.bb/c',
                      WprUrlEntry._ExtractUrl('GET https://aa.bb/c'))
    self.assertEquals('http://aa.bb',
                      WprUrlEntry._ExtractUrl('GET http://aa.bb'))
    self.assertEquals('http://aa.bb',
                      WprUrlEntry._ExtractUrl('GET http://aa.bb FOO BAR'))

  def test_GetResponseHeadersDict(self):
    entry = self._CreateWprUrlEntry([('header0', 'value0'),
                                     ('header1', 'value1'),
                                     ('header0', 'value2'),
                                     ('header2', 'value3'),
                                     ('header0', 'value4'),
                                     ('HEadEr3', 'VaLue4')])
    headers = entry.GetResponseHeadersDict()
    self.assertEquals(4, len(headers))
    self.assertEquals('value0,value2,value4', headers['header0'])
    self.assertEquals('value1', headers['header1'])
    self.assertEquals('value3', headers['header2'])
    self.assertEquals('VaLue4', headers['header3'])

  def test_SetResponseHeader(self):
    entry = self._CreateWprUrlEntry([('header0', 'value0'),
                                     ('header1', 'value1')])
    entry.SetResponseHeader('new_header0', 'new_value0')
    headers = entry.GetResponseHeadersDict()
    self.assertEquals(3, len(headers))
    self.assertEquals('new_value0', headers['new_header0'])
    self.assertEquals('new_header0', entry._wpr_response.headers[2][0])

    entry = self._CreateWprUrlEntry([('header0', 'value0'),
                                     ('header1', 'value1'),
                                     ('header2', 'value1'),])
    entry.SetResponseHeader('header1', 'new_value1')
    headers = entry.GetResponseHeadersDict()
    self.assertEquals(3, len(headers))
    self.assertEquals('new_value1', headers['header1'])
    self.assertEquals('header1', entry._wpr_response.headers[1][0])

    entry = self._CreateWprUrlEntry([('header0', 'value0'),
                                     ('hEADEr1', 'value1'),
                                     ('header2', 'value1'),])
    entry.SetResponseHeader('header1', 'new_value1')
    headers = entry.GetResponseHeadersDict()
    self.assertEquals(3, len(headers))
    self.assertEquals('new_value1', headers['header1'])
    self.assertEquals('hEADEr1', entry._wpr_response.headers[1][0])

    entry = self._CreateWprUrlEntry([('header0', 'value0'),
                                     ('header1', 'value1'),
                                     ('header2', 'value2'),
                                     ('header1', 'value3'),
                                     ('header3', 'value4'),
                                     ('heADer1', 'value5')])
    entry.SetResponseHeader('header1', 'new_value2')
    headers = entry.GetResponseHeadersDict()
    self.assertEquals(4, len(headers))
    self.assertEquals('new_value2', headers['header1'])
    self.assertEquals('header1', entry._wpr_response.headers[1][0])
    self.assertEquals('header3', entry._wpr_response.headers[3][0])
    self.assertEquals('value4', entry._wpr_response.headers[3][1])

    entry = self._CreateWprUrlEntry([('header0', 'value0'),
                                     ('heADer1', 'value1'),
                                     ('header2', 'value2'),
                                     ('HEader1', 'value3'),
                                     ('header3', 'value4'),
                                     ('header1', 'value5')])
    entry.SetResponseHeader('header1', 'new_value2')
    headers = entry.GetResponseHeadersDict()
    self.assertEquals(4, len(headers))
    self.assertEquals('new_value2', headers['header1'])
    self.assertEquals('heADer1', entry._wpr_response.headers[1][0])
    self.assertEquals('header3', entry._wpr_response.headers[3][0])
    self.assertEquals('value4', entry._wpr_response.headers[3][1])

  def test_DeleteResponseHeader(self):
    entry = self._CreateWprUrlEntry([('header0', 'value0'),
                                     ('header1', 'value1'),
                                     ('header0', 'value2'),
                                     ('header2', 'value3')])
    entry.DeleteResponseHeader('header1')
    self.assertNotIn('header1', entry.GetResponseHeadersDict())
    self.assertEquals(2, len(entry.GetResponseHeadersDict()))
    entry.DeleteResponseHeader('header0')
    self.assertNotIn('header0', entry.GetResponseHeadersDict())
    self.assertEquals(1, len(entry.GetResponseHeadersDict()))

    entry = self._CreateWprUrlEntry([('header0', 'value0'),
                                     ('hEAder1', 'value1'),
                                     ('header0', 'value2'),
                                     ('heaDEr2', 'value3')])
    entry.DeleteResponseHeader('header1')
    self.assertNotIn('header1', entry.GetResponseHeadersDict())
    self.assertEquals(2, len(entry.GetResponseHeadersDict()))


if __name__ == '__main__':
  unittest.main()
